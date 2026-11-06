// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The IP network behind SimNodus's virtual access point (experiment E-09,
// SN-267): the access point is 192.168.4.1/24, gives stations 192.168.4.2
// to 192.168.4.20 by DHCP, answers ARP and ping for itself, and answers DNS
// queries. Without a gateway every name is "no such name"; with one (SN-268)
// names resolve through the computer and TCP and UDP to the outside go
// through it. Packets are Ethernet-like: addresses, EtherType, payload.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace simnodus::emu {

class WifiGateway;

using MacAddress = std::array<std::uint8_t, 6>;
using Ipv4Address = std::array<std::uint8_t, 4>;

struct EtherPacket {
    MacAddress to{};
    MacAddress from{};
    std::uint16_t ethertype = 0;
    std::vector<std::uint8_t> payload;
};

class WifiNetwork {
public:
    explicit WifiNetwork(const MacAddress& own);

    // A packet from a station at `now_ns`; returns the network's answers.
    std::vector<EtherPacket> handle(const EtherPacket& packet, std::uint64_t now_ns = 0);
    // The gateway to the computer's network, or nullptr (off).
    void set_gateway(WifiGateway* through) { gateway_ = through; }
    // Packets the gateway has for the stations.
    std::vector<EtherPacket> from_gateway();

    // The address leased to `station`, if any.
    std::optional<Ipv4Address> lease_of(const MacAddress& station) const;

    static constexpr Ipv4Address gateway{192, 168, 4, 1};

private:
    std::vector<EtherPacket> arp(const EtherPacket& packet);
    std::vector<EtherPacket> ipv4(const EtherPacket& packet, std::uint64_t now_ns);
    std::vector<EtherPacket> dhcp(const EtherPacket& packet, const std::vector<std::uint8_t>& message);
    std::vector<EtherPacket> dns(const EtherPacket& packet, const Ipv4Address& from, std::uint16_t port,
        const std::vector<std::uint8_t>& query);
    EtherPacket udp(const MacAddress& to, const Ipv4Address& dst, std::uint16_t src_port, std::uint16_t dst_port,
        const std::vector<std::uint8_t>& data);
    EtherPacket ip(const MacAddress& to, const Ipv4Address& dst, std::uint8_t protocol,
        const std::vector<std::uint8_t>& data);

    MacAddress own_;
    WifiGateway* gateway_ = nullptr;
    std::map<MacAddress, Ipv4Address> leases_;
    std::uint16_t ip_id_ = 1;
};

// The Internet checksum (RFC 1071) of `bytes`, starting from `sum`.
std::uint16_t internet_checksum(const std::uint8_t* bytes, std::size_t count, std::uint32_t sum = 0);

} // namespace simnodus::emu
