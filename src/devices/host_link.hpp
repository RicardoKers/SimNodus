// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The computer's end of a board's USB connector (SN-252, ADR 0107): a serial
// port of the computer (one end of a virtual port pair such as com0com) or a
// TCP server on 127.0.0.1 speaking RFC 2217 (Telnet COM port control), so
// that terminal programs, the Arduino IDE, Thonny, mpremote and esptool reach
// a simulated board. Bytes go both ways; the program's DTR and RTS come too,
// for the board's reset and boot circuit (SN-253). Qt-free, no dependencies.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace simnodus::devices {

// The program's control lines (asserted: true). Without a program connected
// both are false.
struct HostLines {
    bool dtr = false;
    bool rts = false;
    bool operator==(const HostLines&) const = default;
};

class HostLink {
public:
    virtual ~HostLink() = default;
    // Up to `size` bytes from the program; 0 at once when none are waiting.
    virtual std::size_t read(std::uint8_t* buffer, std::size_t size) = 0;
    // Bytes for the program; dropped while none is connected.
    virtual void write(const std::uint8_t* bytes, std::size_t size) = 0;
    virtual HostLines lines() = 0;
    // "COM7", "TCP port 4000 (RFC 2217)"
    virtual std::string description() const = 0;
};

// The TCP port of a link text "tcp:4000" (or "rfc2217:4000"); 0 when the
// text names a serial port instead.
std::uint16_t tcp_link_port(const std::string& spec);

// Opens a link: "COM7", "/dev/ttyUSB0" or "tcp:4000". nullptr and `error`
// (the system's reason) when it cannot be opened.
std::unique_ptr<HostLink> open_host_link(const std::string& spec, std::string& error);

// A test client of the TCP link (tests only): connects to 127.0.0.1:port,
// answers the server's Telnet negotiation and sets DTR and RTS with RFC 2217.
class Rfc2217TestClient {
public:
    Rfc2217TestClient();
    ~Rfc2217TestClient();
    Rfc2217TestClient(const Rfc2217TestClient&) = delete;
    Rfc2217TestClient& operator=(const Rfc2217TestClient&) = delete;
    bool connect(std::uint16_t port, std::string& error);
    void send(const std::string& bytes); // data, 0xFF doubled
    void set_lines(bool dtr, bool rts);
    void set_baud(std::uint32_t baud);
    // Data received within timeout_ms (Telnet commands removed), and the
    // number of RFC 2217 acknowledgements seen so far.
    std::string receive(int timeout_ms);
    int acknowledgements() const { return acks_; }

private:
    void raw(const std::string& bytes);
    std::intptr_t socket_ = -1;
    std::string pending_;
    int acks_ = 0;
};

} // namespace simnodus::devices
