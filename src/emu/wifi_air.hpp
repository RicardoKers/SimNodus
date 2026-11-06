// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The air between simulated ESP32-C3 boards and SimNodus's virtual access
// point (experiment E-09). Frames go from a board's MAC to every other node on
// the same channel, in virtual time. SN-266: the access point announces itself
// (beacons every 102.4 ms, probe responses); authentication, association,
// WPA2 and DHCP come with SN-267.
#pragma once

#include "esp32c3.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace simnodus::emu {

struct WifiApConfig {
    std::string ssid = "SimNodus";
    int channel = 6;
    // A locally administered address (02:...), "SN" in the middle.
    std::array<std::uint8_t, 6> bssid{0x02, 0x53, 0x4E, 0x00, 0x00, 0x01};
    bool wpa2 = false; // announced (RSN element) only; the handshake is SN-267
    int rssi = -40;    // dBm at the boards
};

class WifiAir : public Esp32C3::Timed {
public:
    explicit WifiAir(const WifiApConfig& ap);
    ~WifiAir() override;
    WifiAir(const WifiAir&) = delete;
    WifiAir& operator=(const WifiAir&) = delete;

    // Connects a board: its frames reach the air, and the air's reach it.
    void attach(Esp32C3& board);
    // Called with every frame on the air (for logs and pcap files).
    void set_monitor(std::function<void(const Esp32C3::WifiFrame&)> monitor) { monitor_ = std::move(monitor); }

    std::uint64_t next_ns() const override { return next_beacon_ns_; }
    void on_ns(std::uint64_t now) override;

    // Frame builders, public for the tests.
    std::vector<std::uint8_t> beacon(std::uint64_t tsf_us);
    std::vector<std::uint8_t> probe_response(const std::array<std::uint8_t, 6>& to, std::uint64_t tsf_us);

private:
    bool transmit(Esp32C3& from, const Esp32C3::WifiFrame& frame);
    void send(std::vector<std::uint8_t> bytes, std::uint64_t start_ns, const Esp32C3* skip);
    void management_header(std::vector<std::uint8_t>& out, std::uint8_t subtype, const std::array<std::uint8_t, 6>& to);
    void announce_body(std::vector<std::uint8_t>& out, std::uint64_t tsf_us, bool with_tim);

    WifiApConfig ap_;
    std::vector<Esp32C3*> boards_;
    std::function<void(const Esp32C3::WifiFrame&)> monitor_;
    std::uint64_t next_beacon_ns_ = 0;
    std::uint16_t sequence_ = 0;
};

// Airtime of a frame of `bytes` (without FCS) at 1 Mbit/s with the long
// preamble, as the MAC model times it (approximate).
std::uint64_t wifi_airtime_ns(std::size_t bytes);

// Frames as a pcap file (link type 105, IEEE 802.11 without radio header)
// that Wireshark opens; the time is the frame's end in virtual time.
class WifiPcap {
public:
    // Starts the file; false if it cannot be written.
    bool open(const std::string& path);
    void write(const Esp32C3::WifiFrame& frame);

private:
    std::ofstream file_;
};

} // namespace simnodus::emu
