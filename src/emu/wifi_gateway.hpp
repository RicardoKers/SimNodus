// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The gateway from SimNodus's virtual Wi-Fi network to the computer's network
// (experiment E-09, SN-268; decisions DE9b and DE9c). It is off unless a
// project turns it on. Then:
//
// - TCP from a station to any address is ended by lwIP (vendored in
//   third_party/lwip, unmodified) and carried on by an operating-system
//   socket: a new flow's destination is rewritten to lwIP's listener on
//   192.168.4.1, and lwIP's answers get the original address back.
// - UDP datagrams go through an operating-system socket per station port.
// - 192.168.4.254 ("host.simnodus") is the computer itself (127.0.0.1).
// - Forwarded ports listen on 127.0.0.1 (DE9c) and open a TCP connection
//   from 192.168.4.1 to a station's port.
// - Names are resolved with the computer's resolver.
//
// lwIP keeps global state, so one gateway exists at a time in a process.
// Nothing here is deterministic: it depends on the computer's network.
#pragma once

#include "wifi_network.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::emu {

struct WifiForward {
    std::uint16_t host_port = 0;          // on 127.0.0.1
    Ipv4Address station{192, 168, 4, 2}; // the board's address
    std::uint16_t station_port = 80;
};

class WifiGateway {
public:
    // Starts the gateway and the forwarded ports' listeners; nullptr with a
    // message if another gateway runs or a port cannot be opened.
    static std::unique_ptr<WifiGateway> start(const std::vector<WifiForward>& forwards, std::string& error);
    ~WifiGateway();
    WifiGateway(const WifiGateway&) = delete;
    WifiGateway& operator=(const WifiGateway&) = delete;

    // An IPv4 packet (TCP or UDP) from a station to the outside, or a TCP
    // packet to 192.168.4.1 (the forwarded ports' connections).
    void from_station(const std::vector<std::uint8_t>& ip_packet, std::uint64_t now_ns);
    // Moves data between lwIP and the sockets, runs lwIP's timers.
    void poll(std::uint64_t now_ns);
    // IPv4 packets for the stations since the last call.
    std::vector<std::vector<std::uint8_t>> take_to_stations();
    // The computer's resolver; host.simnodus is the computer.
    std::optional<Ipv4Address> resolve(const std::string& name) const;

    static constexpr Ipv4Address computer{192, 168, 4, 254};

    struct State;

private:
    WifiGateway();
    std::unique_ptr<State> state_;
};

} // namespace simnodus::emu
