// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Non-blocking IPv4 sockets of the computer for the Wi-Fi gateway (SN-268),
// on Winsock and POSIX. Kept apart from lwIP's headers, which define the
// byte-order names the system headers also define.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace simnodus::emu {

using HostAddress = std::array<std::uint8_t, 4>;

class HostSocket {
public:
    // A TCP connection being opened to `address`:`port` (finished() says when).
    static std::unique_ptr<HostSocket> connect_tcp(const HostAddress& address, std::uint16_t port);
    // A TCP listener on 127.0.0.1:`port`; nullptr with a message if the port is taken.
    static std::unique_ptr<HostSocket> listen_tcp(std::uint16_t port, std::string& error);
    // A UDP socket on an ephemeral port, or on 127.0.0.1:`port`.
    static std::unique_ptr<HostSocket> open_udp(std::uint16_t port = 0);
    ~HostSocket();
    HostSocket(const HostSocket&) = delete;
    HostSocket& operator=(const HostSocket&) = delete;

    // A pending connection of a listener, or nullptr.
    std::unique_ptr<HostSocket> accept();
    // For connect_tcp: 0 still connecting, 1 connected, -1 failed.
    int finished();
    // Bytes sent (0 if the socket would block), or -1 on an error.
    long send(const std::uint8_t* data, std::size_t count);
    // Bytes received, 0 at the end of the stream, -1 if nothing is waiting,
    // -2 on an error.
    long receive(std::uint8_t* data, std::size_t count);
    void shutdown_send();
    bool send_to(const HostAddress& address, std::uint16_t port, const std::uint8_t* data, std::size_t count);
    // A datagram and its sender, or -1 if none is waiting.
    long receive_from(std::uint8_t* data, std::size_t count, HostAddress& address, std::uint16_t& port);

private:
    explicit HostSocket(std::intptr_t handle);
    std::intptr_t handle_;
};

// The computer's resolver, IPv4 only (blocking).
std::optional<HostAddress> host_resolve(const std::string& name);

} // namespace simnodus::emu
