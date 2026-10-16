// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// TCP transport of the GDB remote serial protocol (SN-180): a listening socket
// on 127.0.0.1 only (never on other interfaces), one client at a time, packet
// framing ($payload#checksum, acknowledgements, the 0x03 interrupt byte).
// Winsock on Windows, BSD sockets elsewhere; no Qt.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace simnodus::emu {

class GdbSocket {
public:
    GdbSocket();
    ~GdbSocket();
    GdbSocket(const GdbSocket&) = delete;
    GdbSocket& operator=(const GdbSocket&) = delete;

    // Listens on 127.0.0.1:port (0: any free port, see port()).
    bool listen(std::uint16_t port, std::string& error);
    std::uint16_t port() const { return port_; }
    // Waits up to timeout_ms for a client; true once one is connected.
    bool accept(int timeout_ms);
    bool connected() const;

    // Waits up to timeout_ms for data and reads what arrived: complete packets
    // (payloads, acknowledged unless no-ack mode is on) and whether an interrupt
    // byte came. Returns false when the client disconnected.
    bool poll(int timeout_ms, std::vector<std::string>& packets, bool& interrupt);
    void send(const std::string& payload); // frames it: $payload#checksum
    void set_no_ack(bool on) { no_ack_ = on; }
    void close_client();

    // A test client: connects to 127.0.0.1:port; send_raw/receive_raw move bytes.
    bool connect_client(std::uint16_t port, std::string& error);
    bool send_raw(const std::string& bytes);
    // Reads until one complete reply packet arrives (skipping acknowledgements);
    // false on timeout or disconnect.
    bool receive_packet(int timeout_ms, std::string& payload);

    static std::string frame(const std::string& payload);
    // Escapes '#', '$', '}' and '*' in binary data (X packets, qXfer replies).
    static std::string escape(const std::string& data);

private:
    bool wait_readable(std::intptr_t socket, int timeout_ms) const;
    void extract_packets(std::vector<std::string>& packets, bool& interrupt);

    std::intptr_t listener_ = -1;
    std::intptr_t client_ = -1;
    std::uint16_t port_ = 0;
    bool no_ack_ = false;
    std::string buffer_; // received bytes not yet framed
};

} // namespace simnodus::emu
