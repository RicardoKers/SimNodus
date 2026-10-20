// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "devices/device.hpp"

#include <algorithm>
#include <array>
#include <string>

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
            return push_pull(((storage_ >> (role - Q0)) & 1u) != 0);
        }
        if (role == Q7S) {
            return push_pull((shift_ & 0x80u) != 0);
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
            return push_pull((shift_ & 0x8000u) != 0);
        }
        const bool scanning = !shutdown_ || test_;
        if (role < SEGA) {
            // Digit drivers: low (sinking) for the active digit, high for the others.
            return push_pull(!(scanning && role - DIG0 == digit_));
        }
        if (!scanning || !lit_) {
            return push_pull(false);
        }
        return push_pull(((segments(digit_) >> (role - SEGA)) & 1u) != 0);
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

// An I2C slave (NXP UM10204) at the bit level: START and STOP from SDA edges
// while SCL is high, bits taken at rising SCL, the acknowledge and the bits it
// sends put on SDA (open drain) after falling SCL. Subclasses give the address
// and the bytes.
class I2cSlave : public Device {
public:
    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == scl_) {
            scl_level_ = level;
        } else if (pin == sda_) {
            sda_level_ = level;
        }
        other_input(pin, level, 0, true);
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin == sda_) {
            const bool was = sda_level_;
            sda_level_ = level;
            if (scl_level_ && was != level) {
                if (!level) { // START (or repeated START)
                    state_ = State::Address;
                    bits_ = 0;
                    shift_ = 0;
                    ack_phase_ = false;
                    sda_low_ = false;
                    begin(time_ns);
                } else if (state_ != State::Idle || ack_phase_) { // STOP
                    state_ = State::Idle;
                    sda_low_ = false;
                    ack_phase_ = false;
                    end(time_ns);
                }
            }
            return;
        }
        if (pin == scl_) {
            const bool rising = level && !scl_level_;
            const bool falling = !level && scl_level_;
            scl_level_ = level;
            if (rising) {
                clock_rise();
            } else if (falling) {
                clock_fall(time_ns);
            }
            return;
        }
        other_input(pin, level, time_ns, false);
    }

    Output output(std::size_t pin) const override
    {
        if (pin == sda_) {
            return sda_low_ ? open_drain_low() : Output{};
        }
        return other_output(pin);
    }

protected:
    void set_bus_pins(std::size_t scl, std::size_t sda)
    {
        scl_ = scl;
        sda_ = sda;
    }
    virtual std::uint8_t address() const = 0;
    // The address matched: answer it (false: no acknowledge, a busy EEPROM).
    virtual bool select(bool read, std::uint64_t time_ns) = 0;
    // A byte from the master; false: no acknowledge.
    virtual bool receive(std::uint8_t byte, std::uint64_t time_ns) = 0;
    // The next byte for the master.
    virtual std::uint8_t transmit() = 0;
    virtual void begin(std::uint64_t) {}
    virtual void end(std::uint64_t) {}
    virtual void other_input(std::size_t, bool, std::uint64_t, bool) {}
    virtual Output other_output(std::size_t) const { return {}; }

private:
    enum class State { Idle, Address, Write, Read };

    void clock_rise()
    {
        if (state_ == State::Address || state_ == State::Write) {
            if (!ack_phase_ && bits_ < 8) {
                shift_ = static_cast<std::uint8_t>((shift_ << 1) | (sda_level_ ? 1u : 0u));
                ++bits_;
            }
        } else if (state_ == State::Read && bits_ == 8) {
            master_ack_ = !sda_level_; // the master's acknowledge after a byte
        }
    }

    void clock_fall(std::uint64_t time_ns)
    {
        switch (state_) {
        case State::Idle:
            break;
        case State::Address:
        case State::Write:
            if (ack_phase_) {
                // The acknowledge clock is over.
                ack_phase_ = false;
                sda_low_ = false;
                bits_ = 0;
                shift_ = 0;
                if (reading_) {
                    state_ = State::Read;
                    load(transmit());
                } else {
                    state_ = State::Write;
                }
            } else if (bits_ == 8) {
                bool ack = false;
                if (state_ == State::Address) {
                    reading_ = (shift_ & 1u) != 0;
                    ack = (shift_ >> 1) == address() && select(reading_, time_ns);
                    if (!ack) {
                        state_ = State::Idle; // not for us: wait for the next START
                        return;
                    }
                } else {
                    ack = receive(shift_, time_ns);
                }
                ack_phase_ = true;
                sda_low_ = ack;
            }
            break;
        case State::Read:
            if (bits_ < 8) {
                ++bits_;
                if (bits_ < 8) {
                    sda_low_ = ((out_ >> (7 - bits_)) & 1u) == 0;
                } else {
                    sda_low_ = false; // the master acknowledges
                }
            } else if (master_ack_) {
                load(transmit());
            } else {
                state_ = State::Idle; // NACK: the master ends the transfer
                sda_low_ = false;
            }
            break;
        }
    }

    void load(std::uint8_t byte)
    {
        out_ = byte;
        bits_ = 0;
        sda_low_ = (byte & 0x80u) == 0;
    }

    std::size_t scl_ = 0, sda_ = 1;
    bool scl_level_ = true, sda_level_ = true;
    State state_ = State::Idle;
    int bits_ = 0;
    std::uint8_t shift_ = 0;
    std::uint8_t out_ = 0;
    bool ack_phase_ = false;
    bool reading_ = false;
    bool master_ack_ = false;
    bool sda_low_ = false;
};

// Address pins A0-A2 of the chips below, read when the address byte arrives.
template <std::size_t N>
std::uint8_t address_bits(const std::array<bool, N>& levels, std::size_t a0)
{
    return static_cast<std::uint8_t>((levels[a0] ? 1u : 0u) | (levels[a0 + 1] ? 2u : 0u) | (levels[a0 + 2] ? 4u : 0u));
}

// 24C02 (Microchip/Atmel AT24C02): 2 Kbit (256 bytes) EEPROM at 1010 A2 A1 A0.
// After the address, the first byte written sets the word address; the next
// ones go into an 8-byte page (the address wraps inside the page) and are
// stored at STOP, which starts a 5 ms write cycle during which the chip does
// not acknowledge its address (acknowledge polling). Reads start at the word
// address and go on through the memory. WP high keeps the memory unchanged. It
// starts erased (0xFF).
class Eeprom24c02 : public I2cSlave {
public:
    static constexpr std::array<const char*, 6> names{"SCL", "SDA", "A0", "A1", "A2", "WP"};
    static constexpr std::uint64_t write_cycle_ns = 5000000;

    bool init(const std::vector<std::string>& pins, std::string& error)
    {
        if (!find_pins(pins, names, index_, "24C02", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        memory_.fill(0xFF);
        return true;
    }

protected:
    std::uint8_t address() const override { return static_cast<std::uint8_t>(0x50u | address_bits(levels_, 2)); }

    bool select(bool read, std::uint64_t time_ns) override
    {
        if (time_ns < busy_until_) {
            return false; // still writing
        }
        first_ = !read;
        page_.clear();
        return true;
    }

    bool receive(std::uint8_t byte, std::uint64_t) override
    {
        if (first_) {
            pointer_ = byte;
            first_ = false;
        } else {
            // Into the page buffer; the address wraps inside the 8-byte page, so a ninth
            // byte overwrites the first, as on the chip.
            page_.emplace_back(pointer_, byte);
            pointer_ = static_cast<std::uint8_t>((pointer_ & 0xF8u) | ((pointer_ + 1) & 7u));
        }
        return true;
    }

    std::uint8_t transmit() override
    {
        const std::uint8_t value = memory_[pointer_];
        pointer_ = static_cast<std::uint8_t>(pointer_ + 1);
        return value;
    }

    void end(std::uint64_t time_ns) override
    {
        if (page_.empty()) {
            return;
        }
        if (levels_[5]) {
            message("write protected (WP high): the EEPROM kept its contents");
        } else {
            for (const auto& [where, value] : page_) {
                memory_[where] = value;
            }
            busy_until_ = time_ns + write_cycle_ns;
        }
        page_.clear();
    }

    void other_input(std::size_t pin, bool level, std::uint64_t, bool) override
    {
        for (std::size_t i = 2; i < names.size(); ++i) {
            if (pin == index_[i]) {
                levels_[i] = level;
            }
        }
    }

private:
    std::array<std::size_t, names.size()> index_{};
    std::array<bool, names.size()> levels_{};
    std::array<std::uint8_t, 256> memory_{};
    std::vector<std::pair<std::uint8_t, std::uint8_t>> page_;
    std::uint8_t pointer_ = 0;
    bool first_ = false;
    std::uint64_t busy_until_ = 0;
};

// LM75 (NXP/TI): digital temperature sensor at 1001 A2 A1 A0. The first byte
// written selects a register (0 temperature, 1 configuration, 2 THYST, 3 TOS);
// THYST and TOS take two bytes, MSB first. The temperature reads as two bytes:
// whole degrees (two's complement), then bit 7 for half a degree. OS is an
// open-drain output: in comparator mode (the default) it goes active (low, or
// high with OS polarity 1) above TOS (80 C) and back below THYST (75 C). The
// temperature is the part's parameter.
class Lm75 : public I2cSlave {
public:
    static constexpr std::array<const char*, 6> names{"SCL", "SDA", "A0", "A1", "A2", "OS"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "LM75", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        const auto t = parameters.find("temperature");
        if (t != parameters.end()) {
            try {
                celsius_ = std::stod(t->second);
            } catch (...) {
                error = "LM75: the temperature \"" + t->second + "\" is not a number";
                return false;
            }
        }
        update_os();
        return true;
    }

protected:
    std::uint8_t address() const override { return static_cast<std::uint8_t>(0x48u | address_bits(levels_, 2)); }

    bool select(bool read, std::uint64_t) override
    {
        first_ = !read;
        byte_ = 0;
        return true;
    }

    bool receive(std::uint8_t byte, std::uint64_t) override
    {
        if (first_) {
            pointer_ = byte & 3u;
            first_ = false;
            byte_ = 0;
            return true;
        }
        if (pointer_ == 1) {
            config_ = byte;
        } else if (pointer_ >= 2) {
            auto& reg = pointer_ == 2 ? thyst_ : tos_;
            reg = byte_ == 0 ? static_cast<std::uint16_t>((reg & 0x00FFu) | (byte << 8))
                             : static_cast<std::uint16_t>((reg & 0xFF00u) | (byte & 0x80u));
            ++byte_;
        }
        update_os();
        return true;
    }

    std::uint8_t transmit() override
    {
        std::uint16_t value = 0;
        switch (pointer_) {
        case 0: value = temperature(); break;
        case 1: return config_;
        case 2: value = thyst_; break;
        default: value = tos_; break;
        }
        const std::uint8_t out = (byte_ & 1) == 0 ? static_cast<std::uint8_t>(value >> 8) : static_cast<std::uint8_t>(value);
        ++byte_;
        return out;
    }

    void other_input(std::size_t pin, bool level, std::uint64_t, bool) override
    {
        for (std::size_t i = 2; i < 5; ++i) {
            if (pin == index_[i]) {
                levels_[i] = level;
            }
        }
    }

    Output other_output(std::size_t pin) const override
    {
        if (pin == index_[5]) {
            const bool active_high = (config_ & 4u) != 0; // OS polarity
            const bool low = os_active_ != active_high;
            return low ? open_drain_low() : Output{};
        }
        return {};
    }

private:
    // Half degrees in bits 15..7.
    std::uint16_t temperature() const
    {
        const auto halves = static_cast<int>(celsius_ * 2.0 + (celsius_ < 0 ? -0.5 : 0.5));
        return static_cast<std::uint16_t>((halves & 0x1FF) << 7);
    }

    static double celsius_of(std::uint16_t reg) { return static_cast<std::int16_t>(reg & 0xFF80u) / 256.0; }

    void update_os()
    {
        if (celsius_ > celsius_of(tos_)) {
            os_active_ = true;
        } else if (celsius_ < celsius_of(thyst_)) {
            os_active_ = false;
        }
    }

    std::array<std::size_t, names.size()> index_{};
    std::array<bool, names.size()> levels_{};
    double celsius_ = 25.0;
    std::uint8_t pointer_ = 0;
    std::uint8_t config_ = 0;
    std::uint16_t thyst_ = 75 << 8;
    std::uint16_t tos_ = 80 << 8;
    bool first_ = false;
    int byte_ = 0;
    bool os_active_ = false;
};

// PCF8574 (NXP): 8-bit quasi-bidirectional I/O expander at 0100 A2 A1 A0
// (0111 A2 A1 A0 for the PCF8574A variant). A written byte sets P0-P7: a 0
// pulls the pin low, a 1 leaves it high through a weak pull-up (about 100 uA),
// so it can be read as an input. A read returns the levels on the pins. INT
// (open drain) goes low when an input changes and is cleared by the next read
// or write. It starts with all pins high.
class Pcf8574 : public I2cSlave {
public:
    static constexpr std::array<const char*, 14> names{
        "SCL", "SDA", "A0", "A1", "A2", "INT", "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "PCF8574", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        const auto variant = parameters.find("variant");
        base_ = variant != parameters.end() && variant->second == "PCF8574A" ? 0x38u : 0x20u;
        return true;
    }

protected:
    std::uint8_t address() const override { return static_cast<std::uint8_t>(base_ | address_bits(levels_, 2)); }

    bool select(bool, std::uint64_t) override
    {
        interrupt_ = false;
        return true;
    }

    bool receive(std::uint8_t byte, std::uint64_t) override
    {
        latch_ = byte;
        interrupt_ = false;
        return true;
    }

    std::uint8_t transmit() override
    {
        interrupt_ = false;
        std::uint8_t value = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            value |= static_cast<std::uint8_t>((levels_[6 + i] ? 1u : 0u) << i);
        }
        return value;
    }

    void other_input(std::size_t pin, bool level, std::uint64_t, bool initial) override
    {
        for (std::size_t i = 2; i < names.size(); ++i) {
            if (pin != index_[i]) {
                continue;
            }
            if (i >= 6 && !initial && levels_[i] != level) {
                interrupt_ = true; // an input changed
            }
            levels_[i] = level;
        }
    }

    Output other_output(std::size_t pin) const override
    {
        if (pin == index_[5]) {
            return interrupt_ ? open_drain_low() : Output{};
        }
        for (std::size_t i = 0; i < 8; ++i) {
            if (pin == index_[6 + i]) {
                if ((latch_ >> i) & 1u) {
                    return weak_high(); // high, or an input
                }
                return push_pull(false);
            }
        }
        return {};
    }

private:
    std::array<std::size_t, names.size()> index_{};
    std::array<bool, names.size()> levels_{};
    std::uint8_t latch_ = 0xFF;
    std::uint8_t base_ = 0x20;
    bool interrupt_ = false;
};

} // namespace

std::unique_ptr<Device> make_device(const std::string& kind, const std::vector<std::string>& pins,
    const std::map<std::string, std::string>& parameters, std::string& error)
{
    if (kind == "74hc595") {
        auto device = std::make_unique<Hc595>();
        if (!device->init(pins, error)) {
            return nullptr;
        }
        return device;
    }
    if (kind == "24c02") {
        auto device = std::make_unique<Eeprom24c02>();
        return device->init(pins, error) ? std::move(device) : nullptr;
    }
    if (kind == "lm75") {
        auto device = std::make_unique<Lm75>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "pcf8574") {
        auto device = std::make_unique<Pcf8574>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
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
