// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "wifi_air.hpp"

#include <algorithm>

namespace simnodus::emu {

namespace {

constexpr std::uint64_t beacon_interval_ns = 102'400'000; // 100 TU
constexpr std::uint64_t response_delay_ns = 50'000;       // SIFS plus a short backoff

void put16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

void element(std::vector<std::uint8_t>& out, std::uint8_t id, std::initializer_list<std::uint8_t> bytes)
{
    out.push_back(id);
    out.push_back(static_cast<std::uint8_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

} // namespace

std::uint64_t wifi_airtime_ns(std::size_t bytes)
{
    return 192'000 + (bytes + 4) * 8'000;
}

WifiAir::WifiAir(const WifiApConfig& ap) : ap_(ap), network_(ap.bssid), next_beacon_ns_(beacon_interval_ns / 2) {}

WifiAir::~WifiAir()
{
    for (Esp32C3* board : boards_) {
        board->remove_timed(this);
        board->set_wifi_tx(nullptr);
    }
}

void WifiAir::attach(Esp32C3& board)
{
    boards_.push_back(&board);
    board.add_timed(this);
    board.set_wifi_tx([this, &board](const Esp32C3::WifiFrame& frame) { return transmit(board, frame); });
}

void WifiAir::on_ns(std::uint64_t now)
{
    while (next_beacon_ns_ <= now) {
        send(beacon(next_beacon_ns_ / 1000), next_beacon_ns_, nullptr);
        next_beacon_ns_ += beacon_interval_ns;
    }
}

bool WifiAir::transmit(Esp32C3& from, const Esp32C3::WifiFrame& frame)
{
    if (monitor_) {
        monitor_(frame);
    }
    for (Esp32C3* board : boards_) {
        if (board != &from) {
            Esp32C3::WifiFrame copy = frame;
            copy.rssi = ap_.rssi;
            board->wifi_receive(std::move(copy));
        }
    }
    const std::vector<std::uint8_t>& b = frame.bytes;
    if (frame.channel != ap_.channel || b.size() < 24) {
        return false;
    }
    const bool to_ap = std::equal(ap_.bssid.begin(), ap_.bssid.end(), b.begin() + 4);
    MacAddress station{};
    std::copy(b.begin() + 10, b.begin() + 16, station.begin());
    if (to_ap) {
        if ((b[0] & 0x0Cu) == 0x00) {
            management(frame, station);
        } else if ((b[0] & 0x0Cu) == 0x08) {
            data(frame, station);
        }
        return true; // the access point acknowledges what is addressed to it
    }
    if (b[0] == 0x40) { // probe request
        // Answered when it asks for any network or for this SSID.
        bool asked = true;
        for (std::size_t i = 24; i + 2 <= b.size(); i += 2u + b[i + 1]) {
            if (b[i] == 0 && b[i + 1] != 0) {
                asked = b[i + 1] == ap_.ssid.size() && i + 2 + b[i + 1] <= b.size()
                    && std::equal(ap_.ssid.begin(), ap_.ssid.end(), b.begin() + static_cast<std::ptrdiff_t>(i + 2));
            }
        }
        if (asked) {
            std::array<std::uint8_t, 6> to{};
            std::copy(b.begin() + 10, b.begin() + 16, to.begin());
            reply(probe_response(to, (frame.end_ns + response_delay_ns) / 1000), frame.end_ns);
        }
    }
    return false;
}

bool WifiAir::associated(const MacAddress& station) const
{
    const auto it = stations_.find(station);
    return it != stations_.end() && it->second.associated;
}

void WifiAir::reply(std::vector<std::uint8_t> bytes, std::uint64_t after_ns)
{
    const std::uint64_t start = std::max(after_ns, air_free_ns_) + response_delay_ns;
    air_free_ns_ = start + wifi_airtime_ns(bytes.size());
    send(std::move(bytes), start, nullptr);
}

void WifiAir::management(const Esp32C3::WifiFrame& frame, const MacAddress& station)
{
    const std::vector<std::uint8_t>& b = frame.bytes;
    const std::uint8_t subtype = b[0] >> 4;
    Station& s = stations_[station];
    if (subtype == 11 && b.size() >= 30) { // authentication
        const std::uint16_t algorithm = static_cast<std::uint16_t>(b[24] | (b[25] << 8));
        const std::uint16_t sequence = static_cast<std::uint16_t>(b[26] | (b[27] << 8));
        if (sequence != 1) {
            return;
        }
        std::vector<std::uint8_t> out;
        management_header(out, 11, station);
        put16(out, algorithm);
        put16(out, 2);
        put16(out, algorithm == 0 ? 0 : 13); // success, or "algorithm not supported"
        s.authenticated = algorithm == 0;
        s.associated = false;
        reply(std::move(out), frame.end_ns);
    } else if ((subtype == 0 || subtype == 2) && b.size() >= 28) { // (re)association request
        std::vector<std::uint8_t> out;
        management_header(out, subtype == 0 ? 1 : 3, station);
        put16(out, static_cast<std::uint16_t>(0x0401 | (ap_.wpa2 ? 0x0010 : 0)));
        if (!s.authenticated) {
            put16(out, 9); // status: not authenticated
            put16(out, 0);
        } else {
            if (s.aid == 0) {
                s.aid = static_cast<std::uint16_t>(stations_.size());
            }
            s.associated = true;
            put16(out, 0); // success
            put16(out, static_cast<std::uint16_t>(0xC000 | s.aid));
            element(out, 1, {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C});
            element(out, 50, {0x0C, 0x12, 0x18, 0x60});
        }
        reply(std::move(out), frame.end_ns);
    } else if (subtype == 10 || subtype == 12) { // disassociation, deauthentication
        s.associated = false;
        s.authenticated = subtype == 10 && s.authenticated;
    }
}

void WifiAir::data(const Esp32C3::WifiFrame& frame, const MacAddress& station)
{
    const std::vector<std::uint8_t>& b = frame.bytes;
    // To the distribution system, not protected, from an associated station.
    if ((b[1] & 0x03u) != 0x01 || (b[1] & 0x40u) || !associated(station)) {
        return;
    }
    const std::size_t header = (b[0] & 0x80u) ? 26 : 24; // QoS data has a QoS control field
    if ((b[0] & 0x40u) || b.size() < header + 8 || b[header] != 0xAA || b[header + 1] != 0xAA || b[header + 2] != 0x03) {
        return; // null data, or not LLC/SNAP
    }
    EtherPacket packet;
    std::copy(b.begin() + 16, b.begin() + 22, packet.to.begin()); // address 3: the destination
    packet.from = station;
    packet.ethertype = static_cast<std::uint16_t>((b[header + 6] << 8) | b[header + 7]);
    packet.payload.assign(b.begin() + static_cast<std::ptrdiff_t>(header + 8), b.end());
    for (const EtherPacket& answer : network_.handle(packet)) {
        reply(data_frame(answer), frame.end_ns);
    }
}

std::vector<std::uint8_t> WifiAir::data_frame(const EtherPacket& packet)
{
    std::vector<std::uint8_t> out;
    out.push_back(0x08); // data
    out.push_back(0x02); // from the distribution system
    put16(out, 0);
    out.insert(out.end(), packet.to.begin(), packet.to.end());
    out.insert(out.end(), ap_.bssid.begin(), ap_.bssid.end());
    out.insert(out.end(), packet.from.begin(), packet.from.end());
    put16(out, static_cast<std::uint16_t>(sequence_++ << 4));
    out.insert(out.end(), {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00});
    out.push_back(static_cast<std::uint8_t>(packet.ethertype >> 8));
    out.push_back(static_cast<std::uint8_t>(packet.ethertype));
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());
    return out;
}

void WifiAir::send(std::vector<std::uint8_t> bytes, std::uint64_t start_ns, const Esp32C3* skip)
{
    Esp32C3::WifiFrame frame;
    frame.end_ns = start_ns + wifi_airtime_ns(bytes.size());
    frame.bytes = std::move(bytes);
    frame.channel = ap_.channel;
    frame.rssi = ap_.rssi;
    if (monitor_) {
        monitor_(frame);
    }
    for (Esp32C3* board : boards_) {
        if (board != skip) {
            board->wifi_receive(frame);
        }
    }
}

void WifiAir::management_header(std::vector<std::uint8_t>& out, std::uint8_t subtype, const std::array<std::uint8_t, 6>& to)
{
    out.push_back(static_cast<std::uint8_t>(subtype << 4)); // type 0: management
    out.push_back(0);
    put16(out, 0); // duration
    out.insert(out.end(), to.begin(), to.end());
    out.insert(out.end(), ap_.bssid.begin(), ap_.bssid.end()); // source
    out.insert(out.end(), ap_.bssid.begin(), ap_.bssid.end()); // BSSID
    put16(out, static_cast<std::uint16_t>(sequence_++ << 4));
}

void WifiAir::announce_body(std::vector<std::uint8_t>& out, std::uint64_t tsf_us, bool with_tim)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>(tsf_us >> (8 * i)));
    }
    put16(out, 100);                                          // beacon interval, TU
    put16(out, static_cast<std::uint16_t>(0x0401 | (ap_.wpa2 ? 0x0010 : 0))); // ESS, short slot, privacy
    out.push_back(0);                                         // SSID
    out.push_back(static_cast<std::uint8_t>(ap_.ssid.size()));
    out.insert(out.end(), ap_.ssid.begin(), ap_.ssid.end());
    element(out, 1, {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C}); // 1, 2, 5.5, 11 (basic), 18, 24, 36, 54
    element(out, 3, {static_cast<std::uint8_t>(ap_.channel)});       // DS parameter set
    if (with_tim) {
        element(out, 5, {0, 1, 0, 0}); // TIM: DTIM every beacon, no traffic buffered
    }
    element(out, 42, {0});                   // ERP
    element(out, 50, {0x0C, 0x12, 0x18, 0x60}); // 6, 9, 12, 48 Mbit/s
    if (ap_.wpa2) {
        // RSN: version 1, CCMP group and pairwise ciphers, PSK.
        element(out, 48, {1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 2, 0, 0});
    }
}

std::vector<std::uint8_t> WifiAir::beacon(std::uint64_t tsf_us)
{
    std::vector<std::uint8_t> out;
    management_header(out, 8, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
    announce_body(out, tsf_us, true);
    return out;
}

std::vector<std::uint8_t> WifiAir::probe_response(const std::array<std::uint8_t, 6>& to, std::uint64_t tsf_us)
{
    std::vector<std::uint8_t> out;
    management_header(out, 5, to);
    announce_body(out, tsf_us, false);
    return out;
}

namespace {

void put32(std::ofstream& file, std::uint32_t v)
{
    const char b[4] = {static_cast<char>(v & 0xFFu), static_cast<char>((v >> 8) & 0xFFu),
        static_cast<char>((v >> 16) & 0xFFu), static_cast<char>(v >> 24)};
    file.write(b, 4);
}

} // namespace

bool WifiPcap::open(const std::string& path)
{
    file_.open(path, std::ios::binary);
    if (!file_) {
        return false;
    }
    put32(file_, 0xA1B2C3D4u); // microsecond time stamps
    put32(file_, 0x00040002u); // version 2.4
    put32(file_, 0);           // time zone
    put32(file_, 0);           // accuracy
    put32(file_, 65535);       // snap length
    put32(file_, 105);         // LINKTYPE_IEEE802_11
    return true;
}

void WifiPcap::write(const Esp32C3::WifiFrame& frame)
{
    if (!file_.is_open()) {
        return;
    }
    const std::uint64_t us = frame.end_ns / 1000;
    put32(file_, static_cast<std::uint32_t>(us / 1'000'000));
    put32(file_, static_cast<std::uint32_t>(us % 1'000'000));
    put32(file_, static_cast<std::uint32_t>(frame.bytes.size()));
    put32(file_, static_cast<std::uint32_t>(frame.bytes.size()));
    file_.write(reinterpret_cast<const char*>(frame.bytes.data()), static_cast<std::streamsize>(frame.bytes.size()));
}

} // namespace simnodus::emu
