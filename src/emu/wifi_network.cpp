// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "wifi_network.hpp"

#include <algorithm>

namespace simnodus::emu {

namespace {

constexpr std::uint16_t ethertype_ipv4 = 0x0800;
constexpr std::uint16_t ethertype_arp = 0x0806;
constexpr std::uint8_t protocol_icmp = 1;
constexpr std::uint8_t protocol_udp = 17;
constexpr Ipv4Address broadcast_ip{255, 255, 255, 255};
constexpr std::uint8_t first_lease = 2;
constexpr std::uint8_t last_lease = 20;

std::uint16_t be16(const std::vector<std::uint8_t>& b, std::size_t at)
{
    return static_cast<std::uint16_t>((b[at] << 8) | b[at + 1]);
}

void put16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    put16(out, static_cast<std::uint16_t>(v >> 16));
    put16(out, static_cast<std::uint16_t>(v));
}

template <std::size_t N>
void put(std::vector<std::uint8_t>& out, const std::array<std::uint8_t, N>& bytes)
{
    out.insert(out.end(), bytes.begin(), bytes.end());
}

Ipv4Address ipv4_at(const std::vector<std::uint8_t>& b, std::size_t at)
{
    return {b[at], b[at + 1], b[at + 2], b[at + 3]};
}

} // namespace

std::uint16_t internet_checksum(const std::uint8_t* bytes, std::size_t count, std::uint32_t sum)
{
    for (std::size_t i = 0; i + 1 < count; i += 2) {
        sum += static_cast<std::uint32_t>((bytes[i] << 8) | bytes[i + 1]);
    }
    if (count & 1u) {
        sum += static_cast<std::uint32_t>(bytes[count - 1] << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return static_cast<std::uint16_t>(~sum);
}

WifiNetwork::WifiNetwork(const MacAddress& own) : own_(own) {}

std::optional<Ipv4Address> WifiNetwork::lease_of(const MacAddress& station) const
{
    const auto it = leases_.find(station);
    if (it == leases_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<EtherPacket> WifiNetwork::handle(const EtherPacket& packet)
{
    switch (packet.ethertype) {
    case ethertype_arp: return arp(packet);
    case ethertype_ipv4: return ipv4(packet);
    default: return {};
    }
}

std::vector<EtherPacket> WifiNetwork::arp(const EtherPacket& packet)
{
    const std::vector<std::uint8_t>& b = packet.payload;
    // Ethernet and IPv4, a request for the gateway's address.
    if (b.size() < 28 || be16(b, 0) != 1 || be16(b, 2) != ethertype_ipv4 || be16(b, 6) != 1 || ipv4_at(b, 24) != gateway) {
        return {};
    }
    EtherPacket reply;
    reply.to = packet.from;
    reply.from = own_;
    reply.ethertype = ethertype_arp;
    std::vector<std::uint8_t>& out = reply.payload;
    put16(out, 1);
    put16(out, ethertype_ipv4);
    out.push_back(6);
    out.push_back(4);
    put16(out, 2); // reply
    put(out, own_);
    put(out, gateway);
    out.insert(out.end(), b.begin() + 8, b.begin() + 18); // the asker's MAC and IP
    return {reply};
}

std::vector<EtherPacket> WifiNetwork::ipv4(const EtherPacket& packet)
{
    const std::vector<std::uint8_t>& b = packet.payload;
    if (b.size() < 20 || (b[0] >> 4) != 4) {
        return {};
    }
    const std::size_t header = (b[0] & 0x0Fu) * 4u;
    const std::size_t total = std::min<std::size_t>(be16(b, 2), b.size());
    if (header < 20 || total < header) {
        return {};
    }
    const Ipv4Address src = ipv4_at(b, 12);
    const Ipv4Address dst = ipv4_at(b, 16);
    const std::vector<std::uint8_t> data(b.begin() + static_cast<std::ptrdiff_t>(header), b.begin() + static_cast<std::ptrdiff_t>(total));
    if (b[9] == protocol_udp && data.size() >= 8) {
        const std::uint16_t src_port = be16(data, 0);
        const std::uint16_t dst_port = be16(data, 2);
        const std::vector<std::uint8_t> body(data.begin() + 8, data.end());
        if (dst_port == 67 && src_port == 68) {
            return dhcp(packet, body);
        }
        if (dst_port == 53 && dst == gateway) {
            return dns(packet, src, src_port, body);
        }
        return {};
    }
    if (b[9] == protocol_icmp && dst == gateway && data.size() >= 8 && data[0] == 8) { // echo request
        std::vector<std::uint8_t> reply = data;
        reply[0] = 0; // echo reply
        reply[2] = 0;
        reply[3] = 0;
        const std::uint16_t sum = internet_checksum(reply.data(), reply.size());
        reply[2] = static_cast<std::uint8_t>(sum >> 8);
        reply[3] = static_cast<std::uint8_t>(sum);
        return {ip(packet.from, src, protocol_icmp, reply)};
    }
    return {};
}

std::vector<EtherPacket> WifiNetwork::dhcp(const EtherPacket& packet, const std::vector<std::uint8_t>& m)
{
    if (m.size() < 240 || m[0] != 1 || m[236] != 99 || m[237] != 130 || m[238] != 83 || m[239] != 99) {
        return {};
    }
    MacAddress client{};
    std::copy(m.begin() + 28, m.begin() + 34, client.begin());
    std::uint8_t type = 0;
    std::optional<Ipv4Address> requested;
    for (std::size_t i = 240; i < m.size() && m[i] != 255;) {
        if (m[i] == 0) {
            ++i;
            continue;
        }
        if (i + 1 >= m.size() || i + 2 + m[i + 1] > m.size()) {
            break;
        }
        if (m[i] == 53 && m[i + 1] >= 1) {
            type = m[i + 2];
        } else if (m[i] == 50 && m[i + 1] == 4) {
            requested = ipv4_at(m, i + 2);
        }
        i += 2u + m[i + 1];
    }
    if (type == 7) { // RELEASE
        leases_.erase(client);
        return {};
    }
    if (type != 1 && type != 3) {
        return {};
    }
    auto lease = leases_.find(client);
    if (lease == leases_.end()) {
        for (std::uint8_t host = first_lease; host <= last_lease; ++host) {
            const Ipv4Address candidate{192, 168, 4, host};
            if (std::none_of(leases_.begin(), leases_.end(), [&](const auto& l) { return l.second == candidate; })) {
                lease = leases_.emplace(client, candidate).first;
                break;
            }
        }
        if (lease == leases_.end()) {
            return {}; // the pool is used up
        }
    }
    const Ipv4Address address = lease->second;
    if (type == 3 && !requested) {
        requested = ipv4_at(m, 12); // ciaddr while renewing
    }
    const bool nak = type == 3 && requested && *requested != address && *requested != Ipv4Address{};
    std::vector<std::uint8_t> r;
    r.push_back(2); // BOOTREPLY
    r.push_back(1);
    r.push_back(6);
    r.push_back(0);
    r.insert(r.end(), m.begin() + 4, m.begin() + 12); // xid, secs, flags
    r.insert(r.end(), 4, 0);                          // ciaddr
    if (nak) {
        r.insert(r.end(), 4, 0);
    } else {
        put(r, address); // yiaddr
    }
    put(r, gateway);                                  // siaddr
    r.insert(r.end(), 4, 0);                          // giaddr
    r.insert(r.end(), m.begin() + 28, m.begin() + 44); // chaddr
    r.insert(r.end(), 192, 0);                        // sname, file
    r.insert(r.end(), {99, 130, 83, 99});
    r.insert(r.end(), {53, 1, static_cast<std::uint8_t>(nak ? 6 : type == 1 ? 2 : 5)});
    r.insert(r.end(), {54, 4});
    put(r, gateway);
    if (!nak) {
        r.insert(r.end(), {51, 4});
        put32(r, 86400); // one day
        r.insert(r.end(), {1, 4, 255, 255, 255, 0});
        r.insert(r.end(), {3, 4});
        put(r, gateway);
        r.insert(r.end(), {6, 4});
        put(r, gateway);
    }
    r.push_back(255);
    if (nak) {
        leases_.erase(client);
    }
    return {udp(packet.from, broadcast_ip, 67, 68, r)};
}

std::vector<EtherPacket> WifiNetwork::dns(const EtherPacket& packet, const Ipv4Address& from, std::uint16_t port,
    const std::vector<std::uint8_t>& query)
{
    if (query.size() < 12 || (query[2] & 0x80u)) {
        return {};
    }
    // "No such name" with the question repeated.
    std::vector<std::uint8_t> r(query.begin(), query.end());
    r[2] = static_cast<std::uint8_t>(0x80u | (query[2] & 0x01u)); // response, RD as asked
    r[3] = 0x83;                                                  // RA, NXDOMAIN
    r[6] = r[7] = r[8] = r[9] = r[10] = r[11] = 0;                // no answers or other records
    return {udp(packet.from, from, 53, port, r)};
}

EtherPacket WifiNetwork::udp(const MacAddress& to, const Ipv4Address& dst, std::uint16_t src_port, std::uint16_t dst_port,
    const std::vector<std::uint8_t>& data)
{
    std::vector<std::uint8_t> u;
    put16(u, src_port);
    put16(u, dst_port);
    put16(u, static_cast<std::uint16_t>(8 + data.size()));
    put16(u, 0);
    u.insert(u.end(), data.begin(), data.end());
    // Checksum over the pseudo header.
    std::uint32_t sum = 0;
    sum += static_cast<std::uint32_t>((gateway[0] << 8) | gateway[1]) + static_cast<std::uint32_t>((gateway[2] << 8) | gateway[3]);
    sum += static_cast<std::uint32_t>((dst[0] << 8) | dst[1]) + static_cast<std::uint32_t>((dst[2] << 8) | dst[3]);
    sum += protocol_udp + static_cast<std::uint32_t>(u.size());
    std::uint16_t check = internet_checksum(u.data(), u.size(), sum);
    if (check == 0) {
        check = 0xFFFF;
    }
    u[6] = static_cast<std::uint8_t>(check >> 8);
    u[7] = static_cast<std::uint8_t>(check);
    return ip(to, dst, protocol_udp, u);
}

EtherPacket WifiNetwork::ip(const MacAddress& to, const Ipv4Address& dst, std::uint8_t protocol, const std::vector<std::uint8_t>& data)
{
    EtherPacket packet;
    packet.to = to;
    packet.from = own_;
    packet.ethertype = ethertype_ipv4;
    std::vector<std::uint8_t>& h = packet.payload;
    h.push_back(0x45);
    h.push_back(0);
    put16(h, static_cast<std::uint16_t>(20 + data.size()));
    put16(h, ip_id_++);
    put16(h, 0);
    h.push_back(64);
    h.push_back(protocol);
    put16(h, 0);
    put(h, gateway);
    put(h, dst);
    const std::uint16_t sum = internet_checksum(h.data(), 20);
    h[10] = static_cast<std::uint8_t>(sum >> 8);
    h[11] = static_cast<std::uint8_t>(sum);
    h.insert(h.end(), data.begin(), data.end());
    return packet;
}

} // namespace simnodus::emu
