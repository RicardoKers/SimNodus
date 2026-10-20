// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "devices/device.hpp"

#include <algorithm>
#include <array>

namespace simnodus::devices {

namespace {

// Finds the indexes of named pins; false with `error` if one is missing.
template <std::size_t N>
bool find_pins(const std::vector<std::string>& pins, const std::array<const char*, N>& names,
    std::array<std::size_t, N>& index, const std::string& kind, std::string& error)
{
    for (std::size_t i = 0; i < N; ++i) {
        const auto it = std::find(pins.begin(), pins.end(), names[i]);
        if (it == pins.end()) {
            error = kind + ": pin " + names[i] + " is missing";
            return false;
        }
        index[i] = static_cast<std::size_t>(it - pins.begin());
    }
    return true;
}

// 74HC595 (NXP datasheet): 8-bit serial-in shift register with an output
// storage register. DS shifts in at rising SHCP (Q0 first), the storage
// register takes the shift register at rising STCP, MR low clears the shift
// register, OE high puts Q0-Q7 in high impedance. Q7S, the last stage, is
// always driven, for chaining.
class Hc595 : public Device {
public:
    static constexpr std::array<const char*, 14> names{
        "DS", "SHCP", "STCP", "MR", "OE", "Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7", "Q7S"};
    enum { DS, SHCP, STCP, MR, OE, Q0, Q7 = Q0 + 7, Q7S };

    bool init(const std::vector<std::string>& pins, std::string& error)
    {
        if (!find_pins(pins, names, index_, "74HC595", error)) {
            return false;
        }
        role_.assign(pins.size(), -1);
        for (std::size_t i = 0; i < names.size(); ++i) {
            role_[index_[i]] = static_cast<int>(i);
        }
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t) override
    {
        if (pin >= role_.size() || role_[pin] < 0) {
            return;
        }
        const int role = role_[pin];
        const bool rising = level && !inputs_[static_cast<std::size_t>(role)];
        inputs_[static_cast<std::size_t>(role)] = level;
        switch (role) {
        case SHCP:
            if (rising && inputs_[MR]) {
                shift_ = static_cast<std::uint8_t>((shift_ << 1) | (inputs_[DS] ? 1u : 0u));
            }
            break;
        case STCP:
            if (rising) {
                storage_ = shift_;
            }
            break;
        case MR:
            if (!level) {
                shift_ = 0;
            }
            break;
        default: break;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin < role_.size() && role_[pin] >= 0) {
            inputs_[static_cast<std::size_t>(role_[pin])] = level;
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin >= role_.size()) {
            return {};
        }
        const int role = role_[pin];
        if (role >= Q0 && role <= Q7) {
            if (inputs_[OE]) {
                return {}; // OE high: high impedance
            }
            return {true, ((storage_ >> (role - Q0)) & 1u) != 0, false};
        }
        if (role == Q7S) {
            return {true, (shift_ & 0x80u) != 0, false};
        }
        return {};
    }

private:
    std::array<std::size_t, names.size()> index_{};
    std::vector<int> role_;
    std::array<bool, names.size()> inputs_{false, false, false, true, false};
    std::uint8_t shift_ = 0;
    std::uint8_t storage_ = 0;
};

// MAX7219 (Maxim datasheet): serially interfaced 8-digit LED display driver.
// 16-bit words shift in on rising CLK (MSB first), a rising LOAD stores the
// word in the register its address names (1-8 digits, 9 decode mode, 10
// intensity, 11 scan limit, 12 shutdown, 15 display test). While on, the chip
// scans digits 0 .. scan limit, about 156 us each (800 Hz for eight digits):
// the active DIG pin sinks (low), the others are high, and SEG A-G and DP are
// high for the segments that are on, for (2 * intensity + 1) / 32 of the slot.
// Code B decoding (0-9, -, E, H, L, P, blank) for the digits the decode mode
// selects. DOUT is the last stage of the shift register, for chaining. It
// starts in shutdown, as the chip does.
class Max7219 : public Device {
public:
    static constexpr std::array<const char*, 20> names{"DIN", "CLK", "LOAD", "DOUT", "DIG0", "DIG1", "DIG2",
        "DIG3", "DIG4", "DIG5", "DIG6", "DIG7", "SEGA", "SEGB", "SEGC", "SEGD", "SEGE", "SEGF", "SEGG", "SEGDP"};
    enum { DIN, CLK, LOAD, DOUT, DIG0, SEGA = DIG0 + 8, SEGDP = SEGA + 7 };
    static constexpr std::uint64_t slot_ns = 156250; // 1.25 ms / 8

    bool init(const std::vector<std::string>& pins, std::string& error)
    {
        if (!find_pins(pins, names, index_, "MAX7219", error)) {
            return false;
        }
        role_.assign(pins.size(), -1);
        for (std::size_t i = 0; i < names.size(); ++i) {
            role_[index_[i]] = static_cast<int>(i);
        }
        return true;
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin < role_.size() && role_[pin] >= 0 && role_[pin] < 3) {
            inputs_[static_cast<std::size_t>(role_[pin])] = level;
        }
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin >= role_.size() || role_[pin] < 0 || role_[pin] >= 3) {
            return;
        }
        const auto role = static_cast<std::size_t>(role_[pin]);
        const bool rising = level && !inputs_[role];
        inputs_[role] = level;
        if (!rising) {
            return;
        }
        if (role == CLK) {
            shift_ = static_cast<std::uint16_t>((shift_ << 1) | (inputs_[DIN] ? 1u : 0u));
        } else if (role == LOAD) {
            store(static_cast<std::uint8_t>((shift_ >> 8) & 0xFu), static_cast<std::uint8_t>(shift_ & 0xFFu), time_ns);
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin >= role_.size() || role_[pin] < 3) {
            return {};
        }
        const int role = role_[pin];
        if (role == DOUT) {
            return {true, (shift_ & 0x8000u) != 0, false};
        }
        const bool scanning = !shutdown_ || test_;
        if (role < SEGA) {
            // Digit drivers: low (sinking) for the active digit, high for the others.
            return {true, !(scanning && role - DIG0 == digit_), false};
        }
        if (!scanning || !lit_) {
            return {true, false, false};
        }
        return {true, ((segments(digit_) >> (role - SEGA)) & 1u) != 0, false};
    }

    std::uint64_t next_event_ns() const override
    {
        return (!shutdown_ || test_) ? next_ns_ : no_event;
    }

    void advance(std::uint64_t time_ns) override
    {
        if (shutdown_ && !test_) {
            return;
        }
        if (lit_ && time_ns < slot_start_ + slot_ns) {
            lit_ = false; // the intensity's part of the slot is over
            next_ns_ = slot_start_ + slot_ns;
            return;
        }
        const int last = test_ ? 7 : scan_limit_;
        digit_ = digit_ >= last ? 0 : digit_ + 1;
        begin_slot(std::max(time_ns, slot_start_ + slot_ns));
    }

private:
    // Segment bits in pin order A, B, ..., G, DP.
    std::uint8_t segments(int digit) const
    {
        if (test_) {
            return 0xFF;
        }
        const std::uint8_t data = digits_[static_cast<std::size_t>(digit)];
        if ((decode_ >> digit) & 1u) {
            // Code B: 0-9, -, E, H, L, P, blank (bits A..G); bit 7 is DP.
            static constexpr std::array<std::uint8_t, 16> code_b{0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
                0x7F, 0x6F, 0x40, 0x79, 0x76, 0x38, 0x73, 0x00};
            return static_cast<std::uint8_t>(code_b[data & 0xFu] | (data & 0x80u));
        }
        // No decode: D7 = DP, D6 = A, D5 = B, ..., D0 = G.
        std::uint8_t bits = 0;
        for (int s = 0; s < 7; ++s) {
            bits |= static_cast<std::uint8_t>(((data >> (6 - s)) & 1u) << s);
        }
        return static_cast<std::uint8_t>(bits | (data & 0x80u));
    }

    void begin_slot(std::uint64_t time_ns)
    {
        slot_start_ = time_ns;
        lit_ = true;
        const std::uint64_t on = test_ ? slot_ns : slot_ns * static_cast<std::uint64_t>(2 * intensity_ + 1) / 32;
        next_ns_ = time_ns + (on < slot_ns ? on : slot_ns);
    }

    void store(std::uint8_t address, std::uint8_t data, std::uint64_t time_ns)
    {
        const bool was_scanning = !shutdown_ || test_;
        switch (address) {
        case 0x0: break; // no-op (for chained chips)
        case 0x9: decode_ = data; break;
        case 0xA: intensity_ = data & 0xFu; break;
        case 0xB: scan_limit_ = data & 7u; break;
        case 0xC: shutdown_ = (data & 1u) == 0; break;
        case 0xF: test_ = (data & 1u) != 0; break;
        default:
            if (address >= 1 && address <= 8) {
                digits_[static_cast<std::size_t>(address - 1)] = data;
            }
            break;
        }
        if (!was_scanning && (!shutdown_ || test_)) {
            digit_ = 0;
            begin_slot(time_ns);
        }
    }

    std::array<std::size_t, names.size()> index_{};
    std::vector<int> role_;
    std::array<bool, 3> inputs_{};
    std::uint16_t shift_ = 0;
    std::array<std::uint8_t, 8> digits_{};
    std::uint8_t decode_ = 0;
    unsigned intensity_ = 0;
    int scan_limit_ = 0;
    bool shutdown_ = true;
    bool test_ = false;
    int digit_ = 0;
    bool lit_ = false;
    std::uint64_t slot_start_ = 0;
    std::uint64_t next_ns_ = no_event;
};

} // namespace

std::unique_ptr<Device> make_device(const std::string& kind, const std::vector<std::string>& pins,
    const std::map<std::string, std::string>& parameters, std::string& error)
{
    (void)parameters;
    if (kind == "74hc595") {
        auto device = std::make_unique<Hc595>();
        if (!device->init(pins, error)) {
            return nullptr;
        }
        return device;
    }
    if (kind == "max7219") {
        auto device = std::make_unique<Max7219>();
        if (!device->init(pins, error)) {
            return nullptr;
        }
        return device;
    }
    error = "unknown device kind \"" + kind + "\"";
    return nullptr;
}

} // namespace simnodus::devices
