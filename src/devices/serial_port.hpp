// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A serial port of the computer (SN-237) for the Serial Port part: opened raw
// with the part's settings, read without waiting, written with a short
// timeout. Win32 on Windows, termios elsewhere. The name "loopback" gives a
// port that sends back what it receives, for tests and for trying a circuit
// without an adapter. Qt-free, no dependencies.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace simnodus::devices {

inline constexpr const char* loopback_port = "loopback";

struct SerialSettings {
    std::string name;            // "COM3", "/dev/ttyUSB0", "loopback"
    unsigned baud = 115200;
    int data_bits = 8;           // 5 to 8
    char parity = 'N';           // 'N', 'E' or 'O'
    int stop_bits = 1;           // 1 or 2
};

class SerialPort {
public:
    virtual ~SerialPort() = default;
    // Up to `size` received bytes; 0 at once when none are waiting.
    virtual std::size_t read(std::uint8_t* buffer, std::size_t size) = 0;
    // False when the bytes could not be written (the adapter was unplugged).
    virtual bool write(const std::uint8_t* bytes, std::size_t size) = 0;
    // DSR and CTS now (the other end's DTR and RTS on a virtual port pair);
    // false when the port cannot tell.
    virtual bool modem_status(bool& dsr, bool& cts)
    {
        (void)dsr;
        (void)cts;
        return false;
    }
};

// The opened port, or nullptr and `error` (the system's reason).
std::unique_ptr<SerialPort> open_serial_port(const SerialSettings& settings, std::string& error);

// The serial ports the computer has now ("COM3", "/dev/ttyUSB0"), sorted.
std::vector<std::string> serial_port_names();

} // namespace simnodus::devices
