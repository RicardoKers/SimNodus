// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "devices/device.hpp"
#include "devices/serial_port.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
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

// Logic chips in DIP packages (SN-222): pins with fixed roles. Inputs keep
// their last level; async() applies the level-sensitive inputs (resets,
// parallel loads) after every change, edge() the clocks.
template <std::size_t N>
class LogicChip : public Device {
public:
    bool bind(const std::vector<std::string>& pins, const std::array<const char*, N>& names, const char* kind,
        std::string& error)
    {
        std::array<std::size_t, N> index{};
        if (!find_pins(pins, names, index, kind, error)) {
            return false;
        }
        role_.assign(pins.size(), -1);
        for (std::size_t i = 0; i < N; ++i) {
            role_[index[i]] = static_cast<int>(i);
        }
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t) override
    {
        const int r = role(pin);
        if (r < 0) {
            return;
        }
        const bool was = in_[static_cast<std::size_t>(r)];
        in_[static_cast<std::size_t>(r)] = level;
        if (was != level) {
            edge(r, level);
        }
        async();
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (const int r = role(pin); r >= 0) {
            in_[static_cast<std::size_t>(r)] = level;
            async();
        }
    }

    Output output(std::size_t pin) const override
    {
        const int r = role(pin);
        return r < 0 ? Output{} : out(r);
    }

protected:
    virtual void edge(int role, bool level) = 0; // `role` changed to `level`
    virtual void async() {}
    virtual Output out(int role) const = 0;
    bool in(int role) const { return in_[static_cast<std::size_t>(role)]; }
    unsigned bits(int first, int count) const
    {
        unsigned value = 0;
        for (int i = 0; i < count; ++i) {
            value |= (in(first + i) ? 1u : 0u) << i;
        }
        return value;
    }

private:
    int role(std::size_t pin) const { return pin < role_.size() ? role_[pin] : -1; }
    std::vector<int> role_;
    std::array<bool, N> in_{};
};

// 74HC161 (NXP datasheet): synchronous 4-bit binary counter. At a rising CP,
// PE low loads D0-D3, else CEP and CET high count up; MR low clears at once.
// TC = CET and a count of 15.
class Hc161 : public LogicChip<14> {
public:
    static constexpr std::array<const char*, 14> names{
        "MRN", "CP", "D0", "D1", "D2", "D3", "CEP", "PEN", "CET", "Q0", "Q1", "Q2", "Q3", "TC"};
    enum { MRN, CP, D0, CEP = D0 + 4, PEN, CET, Q0, TC = Q0 + 4 };

protected:
    void edge(int role, bool level) override
    {
        if (role != CP || !level || !in(MRN)) {
            return;
        }
        if (!in(PEN)) {
            count_ = bits(D0, 4);
        } else if (in(CEP) && in(CET)) {
            count_ = (count_ + 1) & 15u;
        }
    }
    void async() override
    {
        if (!in(MRN)) {
            count_ = 0;
        }
    }
    Output out(int role) const override
    {
        if (role >= Q0 && role < TC) {
            return push_pull(((count_ >> (role - Q0)) & 1u) != 0);
        }
        return role == TC ? push_pull(in(CET) && count_ == 15) : Output{};
    }

private:
    unsigned count_ = 0;
};

// 74HC193 (NXP datasheet): presettable synchronous 4-bit up/down counter.
// Rising CPU with CPD high counts up, rising CPD with CPU high counts down;
// MR high clears and PL low loads D0-D3, at once. TCU low at 15 while CPU is
// low, TCD low at 0 while CPD is low.
class Hc193 : public LogicChip<14> {
public:
    static constexpr std::array<const char*, 14> names{
        "D0", "D1", "D2", "D3", "CPU", "CPD", "PLN", "MR", "Q0", "Q1", "Q2", "Q3", "TCUN", "TCDN"};
    enum { D0, CPU = D0 + 4, CPD, PLN, MR, Q0, TCUN = Q0 + 4, TCDN };

protected:
    void edge(int role, bool level) override
    {
        if (!level || in(MR) || !in(PLN)) {
            return;
        }
        if (role == CPU && in(CPD)) {
            count_ = (count_ + 1) & 15u;
        } else if (role == CPD && in(CPU)) {
            count_ = (count_ + 15) & 15u;
        }
    }
    void async() override
    {
        if (in(MR)) {
            count_ = 0;
        } else if (!in(PLN)) {
            count_ = bits(D0, 4);
        }
    }
    Output out(int role) const override
    {
        if (role >= Q0 && role < TCUN) {
            return push_pull(((count_ >> (role - Q0)) & 1u) != 0);
        }
        if (role == TCUN) {
            return push_pull(!(count_ == 15 && !in(CPU)));
        }
        return role == TCDN ? push_pull(!(count_ == 0 && !in(CPD))) : Output{};
    }

private:
    unsigned count_ = 0;
};

// 74HC165 (NXP datasheet): 8-bit parallel-in/serial-out shift register. PL
// low loads D0-D7 at once; with PL high a rising CP while CE is low shifts
// one stage toward Q7 and takes DS into stage 0.
class Hc165 : public LogicChip<14> {
public:
    static constexpr std::array<const char*, 14> names{
        "PLN", "CP", "CEN", "DS", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "Q7", "Q7N"};
    enum { PLN, CP, CEN, DS, D0, Q7 = D0 + 8, Q7N };

protected:
    void edge(int role, bool level) override
    {
        if (role == CP && level && !in(CEN) && in(PLN)) {
            stages_ = ((stages_ << 1) | (in(DS) ? 1u : 0u)) & 0xFFu;
        }
    }
    void async() override
    {
        if (!in(PLN)) {
            stages_ = bits(D0, 8);
        }
    }
    Output out(int role) const override
    {
        const bool q7 = (stages_ & 0x80u) != 0;
        if (role == Q7) {
            return push_pull(q7);
        }
        return role == Q7N ? push_pull(!q7) : Output{};
    }

private:
    unsigned stages_ = 0;
};

// CD4017B (TI datasheet): decade counter with ten decoded outputs. It
// advances at a rising CP0 while CP1 is low, or a falling CP1 while CP0 is
// high; MR high returns to Q0 at once. CO is high for counts 0-4.
class Cd4017 : public LogicChip<14> {
public:
    static constexpr std::array<const char*, 14> names{
        "CP0", "CP1", "MR", "Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7", "Q8", "Q9", "CO"};
    enum { CP0, CP1, MR, Q0, CO = Q0 + 10 };

protected:
    void edge(int role, bool level) override
    {
        if (in(MR)) {
            return;
        }
        if ((role == CP0 && level && !in(CP1)) || (role == CP1 && !level && in(CP0))) {
            count_ = (count_ + 1) % 10u;
        }
    }
    void async() override
    {
        if (in(MR)) {
            count_ = 0;
        }
    }
    Output out(int role) const override
    {
        if (role >= Q0 && role < CO) {
            return push_pull(count_ == static_cast<unsigned>(role - Q0));
        }
        return role == CO ? push_pull(count_ < 5) : Output{};
    }

private:
    unsigned count_ = 0;
};

// MAX7219 (Maxim datasheet): serially interfaced 8-digit LED display driver.
// 16-bit words shift in on rising CLK (MSB first), a rising LOAD stores the
// word in the register its address names (1-8 digits, 9 decode mode, 10
// intensity, 11 scan limit, 12 shutdown, 15 display test). While on, the chip
// scans digits 0 .. scan limit, about 156 us each (800 Hz for eight digits):
// the active DIG pin sinks (low), the others are high, and SEG A-G and DP are
// high for the segments that are on. The chip lights a segment for
// (2 * intensity + 1) / 32 of the slot; here it stays on for the whole slot
// with that fraction of its drive, the same average current: pulses as short
// as 5 us would be averaged into the circuit's 100 us steps together with the
// neighbouring digit and light the wrong rows (owner's test, 2026-10-20).
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
        if (!scanning) {
            return push_pull(false);
        }
        const bool on = ((segments(digit_) >> (role - SEGA)) & 1u) != 0;
        return on ? push_pull(true, test_ ? 1.0 : (2.0 * intensity_ + 1.0) / 32.0) : push_pull(false);
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
        next_ns_ = time_ns + slot_ns;
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
            ++version_;
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

public:
    std::vector<std::uint8_t> memory() const override { return {memory_.begin(), memory_.end()}; }
    std::uint64_t memory_version() const override { return version_; }
    void load_memory(const std::vector<std::uint8_t>& bytes) override
    {
        memory_.fill(0xFF);
        std::copy_n(bytes.begin(), std::min(bytes.size(), memory_.size()), memory_.begin());
        ++version_;
    }

private:
    std::array<std::size_t, names.size()> index_{};
    std::array<bool, names.size()> levels_{};
    std::uint64_t version_ = 0;
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
        if (t != parameters.end() && !parse(t->second, celsius_)) {
            error = "LM75: the temperature \"" + t->second + "\" is not a number";
            return false;
        }
        update_os();
        return true;
    }

    // The temperature can change while running (owner's request, 2026-10-20).
    void set_parameter(const std::string& name, const std::string& value) override
    {
        if (name == "temperature" && parse(value, celsius_)) {
            update_os();
        }
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
    static bool parse(const std::string& text, double& celsius)
    {
        char* end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str()) {
            return false;
        }
        celsius = std::clamp(value, -55.0, 125.0);
        return true;
    }

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

// A number parameter of a sensor chip; false if the text is not a number.
bool number_parameter(const std::map<std::string, std::string>& parameters, const char* name, double& value)
{
    const auto it = parameters.find(name);
    if (it == parameters.end()) {
        return true;
    }
    char* end = nullptr;
    const double parsed = std::strtod(it->second.c_str(), &end);
    if (end == it->second.c_str()) {
        return false;
    }
    value = parsed;
    return true;
}

// I2C chips with a register pointer (SN-223): the first byte written after
// the address sets the pointer, further bytes are written from there, reads
// start at the pointer; the pointer advances after each byte. Values that
// change are taken once per read transfer (prepare_read), so the bytes of a
// burst read belong together, as in the chips' shadow registers.
class RegisterSlave : public I2cSlave {
protected:
    bool select(bool read, std::uint64_t time_ns) override
    {
        first_ = !read;
        if (read) {
            prepare_read(time_ns);
        } else {
            begin_write(time_ns);
        }
        return true;
    }

    bool receive(std::uint8_t byte, std::uint64_t time_ns) override
    {
        if (first_) {
            first_ = false;
            pointer_ = set_pointer(byte);
            return true;
        }
        write_register(pointer_, byte, time_ns);
        pointer_ = next(pointer_);
        return true;
    }

    std::uint8_t transmit() override
    {
        const std::uint8_t value = read_register(pointer_);
        pointer_ = next(pointer_);
        return value;
    }

    virtual std::uint8_t read_register(std::uint8_t reg) = 0;
    virtual void write_register(std::uint8_t reg, std::uint8_t value, std::uint64_t time_ns) = 0;
    virtual std::uint8_t set_pointer(std::uint8_t byte) { return byte; }
    virtual std::uint8_t next(std::uint8_t reg) const { return static_cast<std::uint8_t>(reg + 1); }
    virtual void prepare_read(std::uint64_t) {}
    virtual void begin_write(std::uint64_t) {}

    std::uint8_t pointer_ = 0;

private:
    bool first_ = false;
};

// BMP280 (Bosch datasheet BST-BMP280-DS001): pressure and temperature sensor
// at 0x76 + SDO. Chip ID 0x58 at 0xD0; the calibration words at 0x88-0x9F are
// the datasheet's example; the raw values at 0xF7-0xFC are found by inverting
// the datasheet's compensation, so a driver computes the set temperature and
// pressure. A forced measurement (mode 1 or 2 in ctrl_meas) takes the values
// at once and returns to sleep; normal mode (3) follows them. Data read 0x80000
// until the first measurement or with oversampling 0 (skipped).
class Bmp280 : public RegisterSlave {
public:
    static constexpr std::array<const char*, 3> names{"SCL", "SDA", "SDO"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "BMP280", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        if (!number_parameter(parameters, "temperature", celsius_) || !number_parameter(parameters, "pressure", hpa_)) {
            error = "BMP280: the temperature and the pressure must be numbers";
            return false;
        }
        reset();
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        number_parameter({{name, value}}, "temperature", celsius_);
        number_parameter({{name, value}}, "pressure", hpa_);
    }

protected:
    std::uint8_t address() const override { return sdo_ ? 0x77 : 0x76; }

    void other_input(std::size_t pin, bool level, std::uint64_t, bool) override
    {
        if (pin == index_[2]) {
            sdo_ = level;
        }
    }

    void prepare_read(std::uint64_t) override
    {
        if ((ctrl_meas_ & 3u) == 3u) {
            measure();
        }
    }

    std::uint8_t read_register(std::uint8_t reg) override
    {
        if (reg >= 0x88 && static_cast<std::size_t>(reg) < 0x88 + calibration.size() * 2) {
            const auto word = static_cast<std::uint16_t>(calibration[(reg - 0x88) / 2]);
            return (reg & 1u) == 0 ? static_cast<std::uint8_t>(word) : static_cast<std::uint8_t>(word >> 8);
        }
        switch (reg) {
        case 0xD0: return 0x58;
        case 0xF3: return 0; // never busy: measurements are instantaneous
        case 0xF4: return ctrl_meas_;
        case 0xF5: return config_;
        case 0xF7: return static_cast<std::uint8_t>(adc_p_ >> 12);
        case 0xF8: return static_cast<std::uint8_t>(adc_p_ >> 4);
        case 0xF9: return static_cast<std::uint8_t>((adc_p_ & 0xFu) << 4);
        case 0xFA: return static_cast<std::uint8_t>(adc_t_ >> 12);
        case 0xFB: return static_cast<std::uint8_t>(adc_t_ >> 4);
        case 0xFC: return static_cast<std::uint8_t>((adc_t_ & 0xFu) << 4);
        default: return 0;
        }
    }

    void write_register(std::uint8_t reg, std::uint8_t value, std::uint64_t) override
    {
        if (reg == 0xE0 && value == 0xB6) {
            reset();
        } else if (reg == 0xF4) {
            ctrl_meas_ = value;
            if ((value & 3u) != 0) {
                measure();
                if ((value & 3u) != 3u) {
                    ctrl_meas_ = static_cast<std::uint8_t>(value & ~3u); // forced: back to sleep
                }
            }
        } else if (reg == 0xF5) {
            config_ = value;
        }
    }

private:
    // dig_T1..dig_T3, dig_P1..dig_P9 of the datasheet's example (section 8.2).
    static constexpr std::array<std::int32_t, 12> calibration{
        27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000};

    void reset()
    {
        ctrl_meas_ = 0;
        config_ = 0;
        adc_t_ = 0x80000;
        adc_p_ = 0x80000;
    }

    static std::int32_t t_fine(std::int32_t adc)
    {
        const std::int32_t t1 = calibration[0], t2 = calibration[1], t3 = calibration[2];
        const std::int32_t var1 = (((adc >> 3) - (t1 << 1)) * t2) >> 11;
        const std::int32_t var2 = (((((adc >> 4) - t1) * ((adc >> 4) - t1)) >> 12) * t3) >> 14;
        return var1 + var2;
    }

    // Pressure in Pa x 256 (datasheet, 64-bit version).
    static std::int64_t pressure(std::int32_t adc, std::int32_t fine)
    {
        const std::int64_t p1 = calibration[3], p2 = calibration[4], p3 = calibration[5], p4 = calibration[6],
                           p5 = calibration[7], p6 = calibration[8], p7 = calibration[9], p8 = calibration[10],
                           p9 = calibration[11];
        std::int64_t var1 = static_cast<std::int64_t>(fine) - 128000;
        std::int64_t var2 = var1 * var1 * p6;
        var2 = var2 + ((var1 * p5) * 131072);
        var2 = var2 + (p4 * 34359738368LL);
        var1 = ((var1 * var1 * p3) / 256) + ((var1 * p2) * 4096);
        var1 = ((140737488355328LL + var1) * p1) / 8589934592LL;
        if (var1 == 0) {
            return 0;
        }
        std::int64_t p = 1048576 - adc;
        p = (((p * 2147483648LL) - var2) * 3125) / var1;
        var1 = (p9 * (p / 8192) * (p / 8192)) / 33554432;
        var2 = (p8 * p) / 524288;
        return ((p + var1 + var2) / 256) + (p7 * 16);
    }

    void measure()
    {
        const auto target_t = static_cast<std::int32_t>(std::lround(std::clamp(celsius_, -40.0, 85.0) * 100));
        // Temperature rises with the raw value, pressure falls: binary searches over 20 bits.
        std::int32_t low = 0, high = 0xFFFFF;
        while (low < high) {
            const std::int32_t mid = (low + high) / 2;
            if ((t_fine(mid) * 5 + 128) / 256 < target_t) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        const std::int32_t adc_t = low;
        const std::int32_t fine = t_fine(adc_t);
        const auto target_p = static_cast<std::int64_t>(std::lround(std::clamp(hpa_, 300.0, 1100.0) * 100 * 256));
        low = 0;
        high = 0xFFFFF;
        while (low < high) {
            const std::int32_t mid = (low + high) / 2;
            if (pressure(mid, fine) > target_p) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        const bool temperature_on = ((ctrl_meas_ >> 5) & 7u) != 0;
        const bool pressure_on = ((ctrl_meas_ >> 2) & 7u) != 0;
        adc_t_ = temperature_on ? static_cast<std::uint32_t>(adc_t) : 0x80000u;
        adc_p_ = pressure_on ? static_cast<std::uint32_t>(low) : 0x80000u;
    }

    std::array<std::size_t, names.size()> index_{};
    bool sdo_ = false;
    double celsius_ = 25.0;
    double hpa_ = 1013.25;
    std::uint8_t ctrl_meas_ = 0;
    std::uint8_t config_ = 0;
    std::uint32_t adc_t_ = 0x80000;
    std::uint32_t adc_p_ = 0x80000;
};

// MPU-6050 (InvenSense register map RM-MPU-6000A): accelerometer and
// gyroscope at 0x68 + AD0. WHO_AM_I (0x75) is 0x68; it starts asleep
// (PWR_MGMT_1 0x40): clear SLEEP to measure. Accelerations, rotation rates and
// the temperature are parameters; the full scales of ACCEL_CONFIG (0x1C) and
// GYRO_CONFIG (0x1B) scale them; samples are big-endian at 0x3B-0x48 and
// change between reads, never inside one.
class Mpu6050 : public RegisterSlave {
public:
    static constexpr std::array<const char*, 3> names{"SCL", "SDA", "AD0"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "MPU6050", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        for (std::size_t i = 0; i < values_.size(); ++i) {
            if (!number_parameter(parameters, parameter_names[i], values_[i])) {
                error = std::string("MPU6050: ") + parameter_names[i] + " must be a number";
                return false;
            }
        }
        reset();
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        for (std::size_t i = 0; i < values_.size(); ++i) {
            number_parameter({{name, value}}, parameter_names[i], values_[i]);
        }
    }

protected:
    std::uint8_t address() const override { return ad0_ ? 0x69 : 0x68; }

    void other_input(std::size_t pin, bool level, std::uint64_t, bool) override
    {
        if (pin == index_[2]) {
            ad0_ = level;
        }
    }

    void prepare_read(std::uint64_t) override
    {
        if ((registers_[0x6B] & 0x40u) != 0) {
            return; // asleep: the samples stay as they are
        }
        const double accel_lsb = 16384.0 / static_cast<double>(1 << ((registers_[0x1C] >> 3) & 3u));
        const double gyro_lsb = 131.0 / static_cast<double>(1 << ((registers_[0x1B] >> 3) & 3u));
        auto put = [this](std::size_t reg, double value) {
            const auto raw = static_cast<std::int16_t>(std::clamp(std::lround(value), -32768L, 32767L));
            registers_[reg] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(raw) >> 8);
            registers_[reg + 1] = static_cast<std::uint8_t>(raw);
        };
        for (std::size_t i = 0; i < 3; ++i) {
            put(0x3B + 2 * i, values_[i] * accel_lsb);
            put(0x43 + 2 * i, values_[3 + i] * gyro_lsb);
        }
        put(0x41, (values_[6] - 36.53) * 340.0);
    }

    std::uint8_t read_register(std::uint8_t reg) override
    {
        return reg == 0x75 ? 0x68 : registers_[reg & 0x7Fu];
    }

    void write_register(std::uint8_t reg, std::uint8_t value, std::uint64_t) override
    {
        if (reg == 0x6B && (value & 0x80u) != 0) {
            reset(); // DEVICE_RESET
        } else if (reg < 0x3B || reg > 0x60) {
            registers_[reg & 0x7Fu] = value; // the samples are read-only
        }
    }

private:
    static constexpr std::array<const char*, 7> parameter_names{
        "accel_x", "accel_y", "accel_z", "gyro_x", "gyro_y", "gyro_z", "temperature"};

    void reset()
    {
        registers_.fill(0);
        registers_[0x6B] = 0x40;
    }

    std::array<std::size_t, names.size()> index_{};
    bool ad0_ = false;
    std::array<double, 7> values_{0, 0, 1, 0, 0, 0, 25};
    std::array<std::uint8_t, 128> registers_{};
};

// Days since 1970-01-01 of a date and back (H. Hinnant's civil calendar algorithms).
std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2 ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

void civil_from_days(std::int64_t z, std::int64_t& y, unsigned& m, unsigned& d)
{
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y = static_cast<std::int64_t>(yoe) + era * 400 + (m <= 2 ? 1 : 0);
}

std::uint8_t to_bcd(unsigned v) { return static_cast<std::uint8_t>(((v / 10) << 4) | (v % 10)); }
unsigned from_bcd(std::uint8_t v) { return (v >> 4) * 10u + (v & 0xFu); }

// DS3231 (Analog Devices/Maxim datasheet): real-time clock at 0x68. Time and
// date in BCD at 0x00-0x06 (24- or 12-hour mode), two alarms (0x07-0x0D),
// control (0x0E, starts 0x1C) and status (0x0F, starts 0x88 with OSF set), the
// temperature at 0x11-0x12. The clock runs with the simulation from the
// "start" parameter ("now" is the computer's local time when the run starts).
// SQW (open drain) gives the 1 Hz-8.192 kHz square wave while INTCN is 0, or
// goes low for an enabled alarm flag while INTCN is 1. 32K is not driven.
class Ds3231 : public RegisterSlave {
public:
    static constexpr std::array<const char*, 3> names{"SCL", "SDA", "SQW"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "DS3231", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        const auto start = parameters.find("start");
        if (!set_start(start == parameters.end() ? std::string("now") : start->second)) {
            error = "DS3231: the start time \"" + start->second + "\" is not \"now\" or YYYY-MM-DD HH:MM:SS";
            return false;
        }
        if (!number_parameter(parameters, "temperature", celsius_)) {
            error = "DS3231: the temperature must be a number";
            return false;
        }
        registers_[0x0E] = 0x1C;
        registers_[0x0F] = 0x88;
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        number_parameter({{name, value}}, "temperature", celsius_);
    }

    std::uint64_t next_event_ns() const override
    {
        // The next second for the alarms, or the next half period of the square wave.
        const std::uint64_t half = square_wave() ? half_period_ns() : 1000000000ull;
        const std::uint64_t phase = (now_ns_ - std::min(now_ns_, base_ns_)) % half;
        return now_ns_ + (half - phase);
    }

    void advance(std::uint64_t time_ns) override
    {
        const std::int64_t before = seconds(now_ns_);
        now_ns_ = time_ns;
        if (seconds(time_ns) != before) {
            check_alarms(seconds(time_ns));
        }
    }

protected:
    std::uint8_t address() const override { return 0x68; }

    Output other_output(std::size_t pin) const override
    {
        if (pin != index_[2]) {
            return {};
        }
        if (square_wave()) {
            const bool high = ((now_ns_ - std::min(now_ns_, base_ns_)) / half_period_ns()) % 2 == 0;
            return high ? Output{} : open_drain_low();
        }
        const std::uint8_t control = registers_[0x0E], status = registers_[0x0F];
        const bool alarm = ((status & 1u) && (control & 1u)) || ((status & 2u) && (control & 2u));
        return alarm ? open_drain_low() : Output{};
    }

    void prepare_read(std::uint64_t time_ns) override
    {
        now_ns_ = std::max(now_ns_, time_ns);
        snapshot(seconds(now_ns_));
        const int quarters = static_cast<int>(std::lround(celsius_ * 4));
        registers_[0x11] = static_cast<std::uint8_t>(quarters >> 2);
        registers_[0x12] = static_cast<std::uint8_t>((quarters & 3) << 6);
    }

    void begin_write(std::uint64_t time_ns) override
    {
        now_ns_ = std::max(now_ns_, time_ns);
        snapshot(seconds(now_ns_));
        time_written_ = false;
    }

    void end(std::uint64_t time_ns) override
    {
        if (!time_written_) {
            return;
        }
        time_written_ = false;
        // The written time starts now, at the start of a second.
        const unsigned hour_reg = registers_[0x02];
        unsigned hour = (hour_reg & 0x40u) ? from_bcd(hour_reg & 0x1Fu) % 12 + ((hour_reg & 0x20u) ? 12 : 0)
                                           : from_bcd(hour_reg & 0x3Fu);
        twelve_hour_ = (hour_reg & 0x40u) != 0;
        const std::int64_t day = days_from_civil(2000 + from_bcd(registers_[0x06]), from_bcd(registers_[0x05] & 0x1Fu),
            std::max(1u, from_bcd(registers_[0x04] & 0x3Fu)));
        const std::int64_t second = day * 86400 + hour * 3600 + from_bcd(registers_[0x01] & 0x7Fu) * 60
            + from_bcd(registers_[0x00] & 0x7Fu);
        weekday_offset_ = static_cast<int>(((registers_[0x03] & 7u) + 6 - static_cast<unsigned>((day % 7 + 7) % 7)) % 7);
        base_seconds_ = second;
        base_ns_ = time_ns;
        now_ns_ = std::max(now_ns_, time_ns);
    }

    std::uint8_t read_register(std::uint8_t reg) override { return reg < registers_.size() ? registers_[reg] : 0; }

    void write_register(std::uint8_t reg, std::uint8_t value, std::uint64_t) override
    {
        if (reg >= registers_.size() || reg == 0x11 || reg == 0x12) {
            return;
        }
        if (reg <= 0x06) {
            time_written_ = true;
        }
        if (reg == 0x0F) {
            value = static_cast<std::uint8_t>((registers_[0x0F] & value & 0x83u) | (value & 0x08u)); // flags clear only
        }
        registers_[reg] = value;
    }

    std::uint8_t next(std::uint8_t reg) const override
    {
        return static_cast<std::uint8_t>(static_cast<std::size_t>(reg) + 1 >= registers_.size() ? 0 : reg + 1);
    }

private:
    bool set_start(const std::string& text)
    {
        std::int64_t y = 0;
        unsigned mo = 0, d = 0, h = 0, mi = 0, s = 0;
        if (text.empty() || text == "now") {
            const std::time_t t = std::time(nullptr);
            std::tm local{};
#ifdef _WIN32
            localtime_s(&local, &t);
#else
            localtime_r(&t, &local);
#endif
            y = local.tm_year + 1900;
            mo = static_cast<unsigned>(local.tm_mon + 1);
            d = static_cast<unsigned>(local.tm_mday);
            h = static_cast<unsigned>(local.tm_hour);
            mi = static_cast<unsigned>(local.tm_min);
            s = static_cast<unsigned>(local.tm_sec);
        } else {
            std::array<long, 6> f{}; // year, month, day, hour, minute, second
            std::size_t n = 0;
            for (const char* p = text.c_str(); *p != 0 && n < f.size();) {
                if (std::isdigit(static_cast<unsigned char>(*p))) {
                    char* end = nullptr;
                    f[n++] = std::strtol(p, &end, 10);
                    p = end;
                } else {
                    ++p;
                }
            }
            const long year = f[0], month = f[1], day = f[2], hour = f[3], minute = f[4], second = f[5];
            if (n < 3 || year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0
                || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
                return false;
            }
            y = year;
            mo = static_cast<unsigned>(month);
            d = static_cast<unsigned>(day);
            h = static_cast<unsigned>(hour);
            mi = static_cast<unsigned>(minute);
            s = static_cast<unsigned>(second);
        }
        base_seconds_ = days_from_civil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
        return true;
    }

    std::int64_t seconds(std::uint64_t time_ns) const
    {
        return base_seconds_ + static_cast<std::int64_t>((time_ns - std::min(time_ns, base_ns_)) / 1000000000ull);
    }

    void snapshot(std::int64_t second)
    {
        const std::int64_t day = second >= 0 ? second / 86400 : (second - 86399) / 86400;
        const auto in_day = static_cast<unsigned>(second - day * 86400);
        std::int64_t y = 0;
        unsigned m = 0, d = 0;
        civil_from_days(day, y, m, d);
        const unsigned hour = in_day / 3600;
        registers_[0x00] = to_bcd(in_day % 60);
        registers_[0x01] = to_bcd(in_day / 60 % 60);
        if (twelve_hour_) {
            const unsigned h12 = hour % 12 == 0 ? 12 : hour % 12;
            registers_[0x02] = static_cast<std::uint8_t>(0x40u | (hour >= 12 ? 0x20u : 0u) | to_bcd(h12));
        } else {
            registers_[0x02] = to_bcd(hour);
        }
        registers_[0x03] = static_cast<std::uint8_t>(((day % 7 + 7) % 7 + static_cast<std::int64_t>(weekday_offset_)) % 7 + 1);
        registers_[0x04] = to_bcd(d);
        registers_[0x05] = to_bcd(m);
        registers_[0x06] = to_bcd(static_cast<unsigned>(std::clamp<std::int64_t>(y - 2000, 0, 99)));
    }

    void check_alarms(std::int64_t second)
    {
        snapshot(second);
        const auto& r = registers_;
        auto field = [&](std::uint8_t alarm, std::uint8_t now, std::uint8_t mask) {
            return (alarm & 0x80u) != 0 || (alarm & mask) == (now & mask);
        };
        auto day_matches = [&](std::uint8_t alarm) {
            if ((alarm & 0x80u) != 0) {
                return true;
            }
            return (alarm & 0x40u) ? (alarm & 0x0Fu) == r[0x03] : (alarm & 0x3Fu) == r[0x04];
        };
        if (field(r[0x07], r[0x00], 0x7F) && field(r[0x08], r[0x01], 0x7F) && field(r[0x09], r[0x02], 0x7F)
            && day_matches(r[0x0A])) {
            registers_[0x0F] |= 1u; // A1F
        }
        if (r[0x00] == 0 && field(r[0x0B], r[0x01], 0x7F) && field(r[0x0C], r[0x02], 0x7F) && day_matches(r[0x0D])) {
            registers_[0x0F] |= 2u; // A2F
        }
    }

    bool square_wave() const { return (registers_[0x0E] & 0x04u) == 0; }
    std::uint64_t half_period_ns() const
    {
        static constexpr std::array<std::uint64_t, 4> halves{500000000ull, 488281ull, 122070ull, 61035ull};
        return halves[(registers_[0x0E] >> 3) & 3u];
    }

    std::array<std::size_t, names.size()> index_{};
    std::array<std::uint8_t, 0x13> registers_{};
    std::int64_t base_seconds_ = 0; // clock seconds since 1970 at simulation time base_ns_
    std::uint64_t base_ns_ = 0;
    std::uint64_t now_ns_ = 0;
    int weekday_offset_ = 4; // day 1 is Sunday (1970-01-01 was a Thursday)
    bool twelve_hour_ = false;
    bool time_written_ = false;
    double celsius_ = 25.0;
};

// TCS34725 (ams datasheet): RGB color sensor at 0x29. A command byte (bit 7
// set) selects the register (bits 4:0), auto-incrementing when bits 6:5 are
// 01. ID 0x44 at 0x12; with PON and AEN set in ENABLE, STATUS shows AVALID and
// the clear, red, green and blue counts (0x14-0x1B, low byte first) follow the
// color parameters, the light level, the gain (CONTROL) and the integration
// time (ATIME): counts = level x light / 300 lux x gain x cycles x 40, at most
// 1024 per cycle and 65535.
class Tcs34725 : public RegisterSlave {
public:
    static constexpr std::array<const char*, 2> names{"SCL", "SDA"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "TCS34725", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        for (std::size_t i = 0; i < values_.size(); ++i) {
            if (!number_parameter(parameters, parameter_names[i], values_[i])) {
                error = std::string("TCS34725: ") + parameter_names[i] + " must be a number";
                return false;
            }
        }
        registers_[0x01] = 0xFF;
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        for (std::size_t i = 0; i < values_.size(); ++i) {
            number_parameter({{name, value}}, parameter_names[i], values_[i]);
        }
    }

protected:
    std::uint8_t address() const override { return 0x29; }

    std::uint8_t set_pointer(std::uint8_t byte) override
    {
        increment_ = ((byte >> 5) & 3u) == 1u;
        return static_cast<std::uint8_t>(byte & 0x1Fu);
    }

    std::uint8_t next(std::uint8_t reg) const override
    {
        return increment_ ? static_cast<std::uint8_t>((reg + 1) & 0x1Fu) : reg;
    }

    void prepare_read(std::uint64_t) override
    {
        const bool on = (registers_[0x00] & 3u) == 3u; // PON and AEN
        static constexpr std::array<int, 4> gains{1, 4, 16, 60};
        const int cycles = 256 - registers_[0x01];
        const double most = std::min(65535.0, 1024.0 * cycles);
        const double scale = values_[3] / 300.0 * gains[registers_[0x0F] & 3u] * cycles * 40.0;
        const std::array<double, 4> level{
            (values_[0] + values_[1] + values_[2]) / 255.0, values_[0] / 255.0, values_[1] / 255.0, values_[2] / 255.0};
        for (std::size_t i = 0; i < 4; ++i) {
            const auto count = on ? static_cast<std::uint16_t>(std::min(most, std::max(0.0, level[i] * scale))) : 0;
            registers_[0x14 + 2 * i] = static_cast<std::uint8_t>(count);
            registers_[0x15 + 2 * i] = static_cast<std::uint8_t>(count >> 8);
        }
        registers_[0x13] = on ? 1 : 0; // AVALID
    }

    std::uint8_t read_register(std::uint8_t reg) override { return reg == 0x12 ? 0x44 : registers_[reg & 0x1Fu]; }

    void write_register(std::uint8_t reg, std::uint8_t value, std::uint64_t) override
    {
        if (reg < 0x12) {
            registers_[reg] = value;
        }
    }

private:
    static constexpr std::array<const char*, 4> parameter_names{"red", "green", "blue", "light"};

    std::array<std::size_t, names.size()> index_{};
    std::array<double, 4> values_{255, 255, 255, 300};
    std::array<std::uint8_t, 32> registers_{};
    bool increment_ = false;
};

// SSD1306 (Solomon Systech datasheet): 128x64 or 128x32 monochrome OLED
// controller at 0x3C or 0x3D. After the address, a control byte says whether
// commands (0x00) or display data (0x40) follow; with Co (bit 7) set, one byte
// and then another control byte. The commands that set what is shown work:
// display on/off, addressing modes and windows, page and column in page mode,
// start line, segment remap, COM scan direction, inverse and entire display on;
// the electrical ones (contrast, clock, charge pump, precharge, VCOMH) and the
// scroll commands are taken with their arguments and ignored. text() gives the
// screen as "128x64" and one row of 32 hex digits per line ("128x64 off" while
// the display is off). With the usual initialization (segment remap and COM
// scan reversed, A1 and C8) RAM column 0 is at the left and page 0 at the top.
class Ssd1306 : public I2cSlave {
public:
    static constexpr std::array<const char*, 2> names{"SCL", "SDA"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "SSD1306", error)) {
            return false;
        }
        set_bus_pins(index_[0], index_[1]);
        const auto address = parameters.find("address");
        address_ = address != parameters.end() && address->second == "0x3D" ? 0x3D : 0x3C;
        const auto height = parameters.find("height");
        height_ = height != parameters.end() && height->second == "32" ? 32 : 64;
        return true;
    }

    std::string text() const override
    {
        if (!dirty_) {
            return text_;
        }
        dirty_ = false;
        text_ = render();
        return text_;
    }

protected:
    std::uint8_t address() const override { return address_; }

    bool select(bool read, std::uint64_t) override
    {
        control_next_ = true;
        return !read; // the I2C interface has no reads
    }

    bool receive(std::uint8_t byte, std::uint64_t) override
    {
        dirty_ = true;
        if (control_next_) {
            data_ = (byte & 0x40u) != 0;
            single_ = (byte & 0x80u) != 0;
            control_next_ = false;
            return true;
        }
        if (data_) {
            write_data(byte);
        } else {
            command(byte);
        }
        control_next_ = single_;
        return true;
    }

    std::uint8_t transmit() override { return 0; }

private:
    std::string render() const
    {
        const int rows = std::min(height_, multiplex_);
        std::string out = "128x" + std::to_string(rows);
        if (!on_) {
            return out + " off";
        }
        static const char* hex = "0123456789ABCDEF";
        for (int y = 0; y < rows; ++y) {
            out += '\n';
            const int com = reverse_com_ ? y : rows - 1 - y;
            const int row = (com + start_line_) & 63;
            for (int x0 = 0; x0 < 128; x0 += 4) {
                unsigned nibble = 0;
                for (int b = 0; b < 4; ++b) {
                    const int x = x0 + b;
                    const int column = remap_ ? x : 127 - x;
                    bool lit = ((ram_[static_cast<std::size_t>((row / 8) * 128 + column)] >> (row % 8)) & 1u) != 0;
                    lit = entire_on_ || (lit != inverse_);
                    nibble = (nibble << 1) | (lit ? 1u : 0u);
                }
                out += hex[nibble];
            }
        }
        return out;
    }

    void write_data(std::uint8_t byte)
    {
        ram_[static_cast<std::size_t>(page_ * 128 + column_)] = byte;
        if (mode_ == 1) { // vertical
            if (++page_ > page_end_) {
                page_ = page_start_;
                column_ = column_ >= column_end_ ? column_start_ : column_ + 1;
            }
        } else if (mode_ == 0) { // horizontal
            if (++column_ > column_end_) {
                column_ = column_start_;
                page_ = page_ >= page_end_ ? page_start_ : page_ + 1;
            }
        } else { // page
            column_ = column_ >= 127 ? 0 : column_ + 1;
        }
    }

    void command(std::uint8_t byte)
    {
        if (pending_ > 0) {
            arguments_[static_cast<std::size_t>(arguments_needed_ - pending_)] = byte;
            if (--pending_ == 0) {
                finish(current_);
            }
            return;
        }
        current_ = byte;
        int needed = 0;
        switch (byte) {
        case 0x20: case 0x81: case 0x8D: case 0xA8: case 0xD3: case 0xD5: case 0xD9: case 0xDA: case 0xDB: needed = 1; break;
        case 0x21: case 0x22: case 0xA3: needed = 2; break;
        case 0x29: case 0x2A: needed = 5; break;
        case 0x26: case 0x27: needed = 6; break;
        default: break;
        }
        if (needed > 0) {
            pending_ = arguments_needed_ = needed;
            return;
        }
        if (byte <= 0x0F) {
            column_ = (column_ & 0xF0) | byte;
        } else if (byte <= 0x1F) {
            column_ = (column_ & 0x0F) | ((byte & 0x07) << 4);
        } else if (byte >= 0x40 && byte <= 0x7F) {
            start_line_ = byte & 0x3F;
        } else if (byte == 0xA0 || byte == 0xA1) {
            remap_ = byte == 0xA1;
        } else if (byte == 0xA4 || byte == 0xA5) {
            entire_on_ = byte == 0xA5;
        } else if (byte == 0xA6 || byte == 0xA7) {
            inverse_ = byte == 0xA7;
        } else if (byte == 0xAE || byte == 0xAF) {
            on_ = byte == 0xAF;
        } else if (byte >= 0xB0 && byte <= 0xB7) {
            page_ = byte & 7;
        } else if (byte == 0xC0 || byte == 0xC8) {
            reverse_com_ = byte == 0xC8;
        }
    }

    void finish(std::uint8_t command)
    {
        switch (command) {
        case 0x20: mode_ = arguments_[0] & 3u; break;
        case 0x21:
            column_start_ = column_ = arguments_[0] & 0x7F;
            column_end_ = arguments_[1] & 0x7F;
            break;
        case 0x22:
            page_start_ = page_ = arguments_[0] & 7;
            page_end_ = arguments_[1] & 7;
            break;
        case 0xA8: multiplex_ = (arguments_[0] & 0x3F) + 1; break;
        default: break;
        }
    }

    std::array<std::size_t, names.size()> index_{};
    std::uint8_t address_ = 0x3C;
    int height_ = 64;
    std::array<std::uint8_t, 1024> ram_{};
    bool control_next_ = true, data_ = false, single_ = false;
    std::uint8_t current_ = 0;
    std::array<std::uint8_t, 6> arguments_{};
    int pending_ = 0, arguments_needed_ = 0;
    unsigned mode_ = 2; // page addressing after reset
    int column_ = 0, page_ = 0;
    int column_start_ = 0, column_end_ = 127, page_start_ = 0, page_end_ = 7;
    int multiplex_ = 64, start_line_ = 0;
    bool remap_ = false, reverse_com_ = false, inverse_ = false, entire_on_ = false, on_ = false;
    mutable std::string text_;
    mutable bool dirty_ = true;
};

// The supply voltage the session gives every chip as the parameter "@supply".
double supply_of(const std::map<std::string, std::string>& parameters)
{
    double volts = 0.0;
    number_parameter(parameters, "@supply", volts);
    return volts;
}

// SPI chips that exchange whole bytes (SN-224), modes 0 and 3: with CS low,
// MOSI is taken at each rising SCK and MISO changes at each falling SCK (the
// first bit is out when CS falls). exchange() answers each complete byte with
// the byte to shift out next; MISO floats while CS is high.
class SpiByteSlave : public Device {
public:
    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin == cs_) {
            if (!level && cs_level_) {
                cs_level_ = false;
                bits_ = 0;
                shift_in_ = 0;
                begin(time_ns);
                out_ = next_ = first_byte();
                loaded_ = true;
                shown_ = (out_ & 0x80u) != 0;
            } else if (level && !cs_level_) {
                cs_level_ = true;
                end(time_ns);
            }
        } else if (pin == sck_) {
            const bool rising = level && !sck_level_;
            const bool falling = !level && sck_level_;
            sck_level_ = level;
            if (cs_level_) {
                return;
            }
            if (rising) {
                shift_in_ = static_cast<std::uint8_t>((shift_in_ << 1) | (mosi_level_ ? 1u : 0u));
                if (++bits_ == 8) {
                    next_ = exchange(shift_in_, time_ns);
                    bits_ = 0;
                    loaded_ = false;
                }
            } else if (falling) {
                if (bits_ == 0 && !loaded_) {
                    out_ = next_;
                    loaded_ = true;
                }
                shown_ = ((out_ >> (7 - bits_)) & 1u) != 0;
            }
        } else if (pin == mosi_) {
            mosi_level_ = level;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == cs_) {
            cs_level_ = level;
        } else if (pin == sck_) {
            sck_level_ = level;
        } else if (pin == mosi_) {
            mosi_level_ = level;
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin != miso_ || cs_level_) {
            return {};
        }
        return push_pull(shown_);
    }

protected:
    void set_spi_pins(std::size_t cs, std::size_t sck, std::size_t mosi, std::size_t miso)
    {
        cs_ = cs;
        sck_ = sck;
        mosi_ = mosi;
        miso_ = miso;
    }
    virtual std::uint8_t first_byte() { return 0xFF; }
    virtual std::uint8_t exchange(std::uint8_t in, std::uint64_t time_ns) = 0;
    virtual void begin(std::uint64_t) {}
    virtual void end(std::uint64_t) {}

private:
    std::size_t cs_ = 0, sck_ = 1, mosi_ = 2, miso_ = 3;
    bool cs_level_ = true, sck_level_ = false, mosi_level_ = false, loaded_ = true, shown_ = true;
    int bits_ = 0;
    std::uint8_t shift_in_ = 0, out_ = 0xFF, next_ = 0xFF;
};

// MCP3208 (Microchip datasheet): 8-channel 12-bit A/D converter on SPI. With
// CS low, the first 1 on DIN is the start bit, then SGL/DIFF and D2 D1 D0
// choose the input (single-ended CHn against AGND, or a differential pair);
// the input is sampled until the falling edge of the fifth clock after the
// start bit, when DOUT gives a null bit; the 12 result bits follow MSB first
// at falling CLK edges (then the same bits LSB first). The result is
// 4096 x VIN / VREF, from the circuit's voltages.
class Mcp3208 : public Device {
public:
    static constexpr std::array<const char*, 12> names{
        "CS", "CLK", "DIN", "DOUT", "CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7"};
    enum { CS, CLK, DIN, DOUT, CH0 };

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "MCP3208", error)) {
            return false;
        }
        supply_ = supply_of(parameters);
        const auto vref = std::find(pins.begin(), pins.end(), "VREF");
        vref_ = vref == pins.end() ? index_[CH0] : static_cast<std::size_t>(vref - pins.begin());
        has_vref_ = vref != pins.end();
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin == index_[CS]) {
            cs_ = level;
            if (level) {
                state_ = State::Idle;
            } else {
                state_ = State::Start;
                bits_ = 0;
            }
        } else if (pin == index_[DIN]) {
            din_ = level;
        } else if (pin == index_[CLK]) {
            const bool rising = level && !clk_;
            const bool falling = !level && clk_;
            clk_ = level;
            if (cs_) {
                return;
            }
            if (rising) {
                if (state_ == State::Start && din_) {
                    state_ = State::Config;
                    bits_ = 0;
                    config_ = 0;
                } else if (state_ == State::Config) {
                    config_ = (config_ << 1) | (din_ ? 1u : 0u);
                    if (++bits_ == 4) {
                        state_ = State::Sample; // sampling from this edge to the falling edge of the next clock
                        sample_edges_ = 0;
                    }
                }
            } else if (falling) {
                if (state_ == State::Sample && ++sample_edges_ == 2) {
                    result_ = convert(time_ns);
                    state_ = State::Out;
                    position_ = -1; // the null bit
                } else if (state_ == State::Out) {
                    ++position_;
                }
            }
        } else {
            levels_[pin < levels_.size() ? pin : 0] = level;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == index_[CS]) {
            cs_ = level;
        } else if (pin == index_[CLK]) {
            clk_ = level;
        } else if (pin == index_[DIN]) {
            din_ = level;
        } else if (pin < levels_.size()) {
            levels_[pin] = level;
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin != index_[DOUT] || cs_ || state_ != State::Out) {
            return {};
        }
        if (position_ < 0) {
            return push_pull(false);
        }
        int bit = 0;
        if (position_ < 12) {
            bit = 11 - position_; // MSB first
        } else if (position_ < 23) {
            bit = position_ - 11; // then LSB first, B1..B11
        } else {
            return push_pull(false);
        }
        return push_pull(((result_ >> bit) & 1u) != 0);
    }

private:
    enum class State { Idle, Start, Config, Sample, Out };

    double volts(std::size_t pin, std::uint64_t time_ns) const
    {
        const bool high = pin < levels_.size() && levels_[pin];
        return analog(pin, time_ns, high ? supply_ : 0.0);
    }

    unsigned convert(std::uint64_t time_ns) const
    {
        const bool single = (config_ & 8u) != 0;
        const unsigned channel = config_ & 7u;
        const double vref = has_vref_ ? volts(vref_, time_ns) : supply_;
        double vin = 0.0;
        if (single) {
            vin = volts(index_[CH0 + channel], time_ns);
        } else {
            const unsigned plus = channel & 6u;
            const unsigned a = (channel & 1u) ? plus + 1 : plus;
            const unsigned b = (channel & 1u) ? plus : plus + 1;
            vin = volts(index_[CH0 + a], time_ns) - volts(index_[CH0 + b], time_ns);
        }
        if (vref <= 0.0) {
            return 0;
        }
        return static_cast<unsigned>(std::clamp(std::floor(4096.0 * vin / vref), 0.0, 4095.0));
    }

    std::array<std::size_t, names.size()> index_{};
    std::size_t vref_ = 0;
    bool has_vref_ = false;
    std::array<bool, 32> levels_{};
    bool cs_ = true, clk_ = false, din_ = false;
    State state_ = State::Idle;
    int bits_ = 0;
    unsigned config_ = 0;
    unsigned result_ = 0;
    int position_ = 0;
    int sample_edges_ = 0;
    double supply_ = 5.0;
};

// MCP41010 (Microchip datasheet): 10 kOhm digital potentiometer on SPI. A
// command byte (bits 5:4: 01 write, 10 shutdown; bits 1:0 select pot 0) and a
// data byte shift in at rising SCK with CS low and act when CS rises after
// exactly 16 bits: the wiper goes to step D of 256 (PW0 at D/256 from PB0
// towards PA0). Shutdown opens PA0 and connects the wiper to PB0. The wiper
// starts at mid-scale (0x80). The internal pin WIPER carries D/256 to the
// part's template as its output strength.
class Mcp41010 : public Device {
public:
    static constexpr std::array<const char*, 4> names{"CS", "SCK", "SI", "WIPER"};

    bool init(const std::vector<std::string>& pins, std::string& error)
    {
        return find_pins(pins, names, index_, "MCP41010", error);
    }

    void set_input(std::size_t pin, bool level, std::uint64_t) override
    {
        if (pin == index_[0]) {
            if (!level) {
                bits_ = 0;
                shift_ = 0;
            } else if (!cs_ && bits_ == 16) {
                const unsigned command = (shift_ >> 12) & 3u;
                if ((shift_ >> 8 & 3u) & 1u) { // pot 0 selected
                    if (command == 1) {
                        wiper_ = shift_ & 0xFFu;
                        shutdown_ = false;
                    } else if (command == 2) {
                        shutdown_ = true;
                    }
                }
            }
            cs_ = level;
        } else if (pin == index_[1]) {
            if (level && !sck_ && !cs_ && bits_ < 16) {
                shift_ = (shift_ << 1) | (si_ ? 1u : 0u);
                ++bits_;
            }
            sck_ = level;
        } else if (pin == index_[2]) {
            si_ = level;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == index_[0]) {
            cs_ = level;
        } else if (pin == index_[1]) {
            sck_ = level;
        } else if (pin == index_[2]) {
            si_ = level;
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin != index_[3]) {
            return {};
        }
        // The fraction of the track from PB0 to the wiper; 0 in shutdown.
        return push_pull(true, shutdown_ ? 0.0 : wiper_ / 256.0);
    }

private:
    std::array<std::size_t, names.size()> index_{};
    bool cs_ = true, sck_ = false, si_ = false, shutdown_ = false;
    int bits_ = 0;
    unsigned shift_ = 0;
    unsigned wiper_ = 0x80;
};

// W25Q32 (Winbond datasheet): 32 Mbit (4 MB) SPI NOR flash. Commands: 06/04
// write enable/disable, 05/35 status registers 1 and 2 (BUSY, WEL), 9F JEDEC ID
// (EF 40 16), 90 manufacturer/device ID (EF 15), AB release from power-down
// (device ID 15), 4B unique ID, 03 read, 0B fast read, 02 page program (within
// a 256-byte page, only clearing bits), 20/52/D8 erase of 4/32/64 KB, C7/60 chip
// erase, B9 power-down. Program and erase need WEL and keep BUSY for the
// datasheet's typical times (0.7 ms page, 45 ms sector, 120/150 ms blocks,
// 10 s chip); meanwhile only the status can be read. Starts erased (0xFF) or
// with its contents file.
class W25q32 : public SpiByteSlave {
public:
    static constexpr std::array<const char*, 4> names{"CS", "CLK", "DI", "DO"};
    static constexpr std::size_t size = 4u * 1024u * 1024u;

    bool init(const std::vector<std::string>& pins, std::string& error)
    {
        std::array<std::size_t, names.size()> index{};
        if (!find_pins(pins, names, index, "W25Q32", error)) {
            return false;
        }
        set_spi_pins(index[0], index[1], index[2], index[3]);
        memory_.assign(size, 0xFF);
        return true;
    }

    std::vector<std::uint8_t> memory() const override { return memory_; }
    std::uint64_t memory_version() const override { return version_; }
    void load_memory(const std::vector<std::uint8_t>& bytes) override
    {
        std::copy_n(bytes.begin(), std::min(bytes.size(), memory_.size()), memory_.begin());
        ++version_;
    }

    std::uint64_t next_event_ns() const override { return busy_until_ > 0 ? busy_until_ : no_event; }
    void advance(std::uint64_t time_ns) override
    {
        if (busy_until_ > 0 && time_ns >= busy_until_) {
            busy_until_ = 0;
            wel_ = false;
        }
    }

protected:
    void begin(std::uint64_t time_ns) override
    {
        advance(time_ns);
        command_ = -1;
        count_ = 0;
        address_ = 0;
        page_.clear();
    }

    std::uint8_t exchange(std::uint8_t in, std::uint64_t time_ns) override
    {
        advance(time_ns);
        if (command_ < 0) {
            command_ = in;
            if (busy() && in != 0x05 && in != 0x35) {
                command_ = 0; // ignored while busy
            } else if (powered_down_ && in != 0xAB) {
                command_ = 0;
            } else if (in == 0x06) {
                wel_ = true;
            } else if (in == 0x04) {
                wel_ = false;
            }
            return answer();
        }
        ++count_;
        switch (command_) {
        case 0x03: case 0x0B: case 0x02: case 0x20: case 0x52: case 0xD8: case 0x90: case 0xAB:
            if (count_ <= 3) {
                address_ = ((address_ << 8) | in) & (size - 1);
            } else if (command_ == 0x02) {
                page_.push_back(in);
            }
            break;
        default: break;
        }
        return answer();
    }

    void end(std::uint64_t time_ns) override
    {
        advance(time_ns);
        if (command_ == 0xB9) {
            powered_down_ = true;
        } else if (command_ == 0xAB) {
            powered_down_ = false;
        }
        if (!wel_ || busy()) {
            return;
        }
        std::uint64_t duration = 0;
        if (command_ == 0x02 && count_ >= 4) {
            const std::size_t page = address_ & ~std::size_t{0xFF};
            for (std::size_t i = 0; i < page_.size(); ++i) {
                const std::size_t a = page | ((address_ + i) & 0xFFu);
                memory_[a] = static_cast<std::uint8_t>(memory_[a] & page_[i]);
            }
            duration = 700000;
        } else if ((command_ == 0x20 || command_ == 0x52 || command_ == 0xD8) && count_ >= 3) {
            const std::size_t block = command_ == 0x20 ? 4096 : command_ == 0x52 ? 32768 : 65536;
            std::fill_n(memory_.begin() + static_cast<std::ptrdiff_t>(address_ & ~(block - 1)), block, std::uint8_t{0xFF});
            duration = command_ == 0x20 ? 45000000 : command_ == 0x52 ? 120000000 : 150000000;
        } else if (command_ == 0xC7 || command_ == 0x60) {
            std::fill(memory_.begin(), memory_.end(), std::uint8_t{0xFF});
            duration = 10000000000ull;
        } else {
            return;
        }
        ++version_;
        busy_until_ = time_ns + duration;
    }

private:
    bool busy() const { return busy_until_ > 0; }

    // The byte shifted out during the next byte of the current command.
    std::uint8_t answer()
    {
        switch (command_) {
        case 0x05: return static_cast<std::uint8_t>((busy() ? 1u : 0u) | (wel_ ? 2u : 0u));
        case 0x35: return 0x00;
        case 0x9F: {
            static constexpr std::array<std::uint8_t, 3> id{0xEF, 0x40, 0x16};
            return count_ < 3 ? id[static_cast<std::size_t>(count_)] : 0xFF;
        }
        case 0x90: return count_ < 3 ? 0xFF : ((count_ - 3) % 2 == 0 ? 0xEF : 0x15);
        case 0xAB: return count_ < 3 ? 0xFF : 0x15;
        case 0x4B: {
            static constexpr std::array<std::uint8_t, 8> unique{0x53, 0x49, 0x4D, 0x4E, 0x4F, 0x44, 0x55, 0x53};
            return count_ >= 4 && count_ < 12 ? unique[static_cast<std::size_t>(count_ - 4)] : 0xFF;
        }
        case 0x03:
        case 0x0B: {
            const int data_from = command_ == 0x03 ? 3 : 4; // fast read: a dummy byte first
            if (count_ < data_from) {
                return 0xFF;
            }
            return memory_[(address_ + static_cast<std::size_t>(count_ - data_from)) & (size - 1)];
        }
        default: return 0xFF;
        }
    }

    std::vector<std::uint8_t> memory_;
    std::uint64_t version_ = 1;
    int command_ = -1;
    int count_ = 0;
    std::size_t address_ = 0;
    std::vector<std::uint8_t> page_;
    bool wel_ = false, powered_down_ = false;
    std::uint64_t busy_until_ = 0;
};

// microSD card in SPI mode (SD Physical Layer Simplified Specification): an
// SDHC card of the size of its image (block addresses). CMD0 enters SPI mode
// (idle), CMD8 echoes its check pattern, CMD55 + ACMD41 ends the
// initialization, CMD58 reports CCS (high capacity), CMD9/CMD10 give CSD and
// CID, CMD16 is accepted, CMD17/CMD18 read blocks (start token 0xFE, 512 bytes,
// two CRC bytes) and CMD12 stops a multiple read, CMD24/CMD25 write blocks
// (tokens 0xFE / 0xFC, 0xFD ends) answered with 0x05 and a short busy time.
// CRCs are not checked. The contents start from its image file (the Memory
// Viewer can save them); writes stay in the simulation.
class SdCard : public SpiByteSlave {
public:
    static constexpr std::array<const char*, 4> names{"CS", "SCK", "MOSI", "MISO"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        std::array<std::size_t, names.size()> index{};
        if (!find_pins(pins, names, index, "microSD", error)) {
            return false;
        }
        set_spi_pins(index[0], index[1], index[2], index[3]);
        std::size_t bytes = 8u * 1024u * 1024u;
        if (const auto it = parameters.find("size"); it != parameters.end()) {
            char* end = nullptr;
            const double megabytes = std::strtod(it->second.c_str(), &end);
            if (end != it->second.c_str() && megabytes >= 1 && megabytes <= 64) {
                bytes = static_cast<std::size_t>(megabytes) * 1024u * 1024u;
            }
        }
        memory_.assign(bytes, 0x00);
        return true;
    }

    std::vector<std::uint8_t> memory() const override { return memory_; }
    std::uint64_t memory_version() const override { return version_; }
    void load_memory(const std::vector<std::uint8_t>& bytes) override
    {
        std::copy_n(bytes.begin(), std::min(bytes.size(), memory_.size()), memory_.begin());
        ++version_;
    }

protected:
    std::uint8_t exchange(std::uint8_t in, std::uint64_t) override
    {
        // Data phase of a write: token, 512 bytes, 2 CRC bytes, then the data response.
        if (write_state_ != Write::None) {
            return write_byte(in);
        }
        // Collecting a command: 0b01xxxxxx, four argument bytes, CRC.
        if (command_.empty()) {
            if ((in & 0xC0u) == 0x40u) {
                command_.push_back(in);
                return 0xFF;
            }
            return next_out();
        }
        command_.push_back(in);
        if (command_.size() < 6) {
            return 0xFF;
        }
        const unsigned index = command_[0] & 0x3Fu;
        const std::uint32_t argument = static_cast<std::uint32_t>(command_[1]) << 24
            | static_cast<std::uint32_t>(command_[2]) << 16 | static_cast<std::uint32_t>(command_[3]) << 8 | command_[4];
        command_.clear();
        respond(index, argument);
        return 0xFF; // the response follows after one byte (NCR)
    }

private:
    enum class Write { None, Token, Data, Crc, Done };

    std::uint8_t next_out()
    {
        if (out_.empty()) {
            if (reading_) {
                queue_block(); // CMD18 goes on until CMD12
            } else {
                return 0xFF;
            }
        }
        const std::uint8_t b = out_.front();
        out_.pop_front();
        return b;
    }

    void respond(unsigned index, std::uint32_t argument)
    {
        const bool acmd = app_;
        app_ = false;
        out_.clear();
        const std::uint8_t r1 = idle_ ? 0x01 : 0x00;
        if (index == 12) {
            reading_ = false;
            out_ = {0xFF, 0x00}; // a stuff byte, then R1
            return;
        }
        if (acmd && index == 41) {
            idle_ = false;
            out_ = {0x00};
            return;
        }
        switch (index) {
        case 0:
            idle_ = true;
            out_ = {0x01};
            break;
        case 8:
            out_ = {r1, 0x00, 0x00, static_cast<std::uint8_t>((argument >> 8) & 0x0Fu),
                static_cast<std::uint8_t>(argument)};
            break;
        case 55:
            app_ = true;
            out_ = {r1};
            break;
        case 58: out_ = {r1, 0xC0, 0xFF, 0x80, 0x00}; break; // powered up, CCS: block addresses
        case 9: out_ = {r1, 0xFE}; append(csd()); out_.push_back(0xFF); out_.push_back(0xFF); break;
        case 10: out_ = {r1, 0xFE}; append(cid()); out_.push_back(0xFF); out_.push_back(0xFF); break;
        case 16: out_ = {r1}; break;
        case 17:
        case 18:
            if (idle_ || static_cast<std::size_t>(argument) * 512u >= memory_.size()) {
                out_ = {static_cast<std::uint8_t>(r1 | 0x40u)}; // parameter error
                break;
            }
            block_ = argument;
            out_ = {r1};
            queue_block();
            reading_ = index == 18;
            break;
        case 24:
        case 25:
            if (idle_ || static_cast<std::size_t>(argument) * 512u >= memory_.size()) {
                out_ = {static_cast<std::uint8_t>(r1 | 0x40u)};
                break;
            }
            block_ = argument;
            multiple_ = index == 25;
            write_state_ = Write::Token;
            out_ = {r1};
            break;
        default: out_ = {static_cast<std::uint8_t>(r1 | 0x04u)}; break; // illegal command
        }
    }

    void queue_block()
    {
        if (static_cast<std::size_t>(block_) * 512u >= memory_.size()) {
            reading_ = false;
            return;
        }
        out_.push_back(0xFF);
        out_.push_back(0xFE);
        const auto first = memory_.begin() + static_cast<std::ptrdiff_t>(block_) * 512;
        out_.insert(out_.end(), first, first + 512);
        out_.push_back(0xFF); // CRC, not checked
        out_.push_back(0xFF);
        ++block_;
    }

    std::uint8_t write_byte(std::uint8_t in)
    {
        switch (write_state_) {
        case Write::Token:
            if (in == 0xFE || (multiple_ && in == 0xFC)) {
                write_state_ = Write::Data;
                data_.clear();
            } else if (multiple_ && in == 0xFD) {
                write_state_ = Write::None; // stop tran
                out_ = {0x00, 0xFF};        // a short busy time
            }
            return next_out();
        case Write::Data:
            data_.push_back(in);
            if (data_.size() == 512) {
                write_state_ = Write::Crc;
                crc_bytes_ = 0;
            }
            return 0xFF;
        case Write::Crc:
            if (++crc_bytes_ < 2) {
                return 0xFF;
            }
            std::copy(data_.begin(), data_.end(), memory_.begin() + static_cast<std::ptrdiff_t>(block_) * 512);
            ++block_;
            ++version_;
            write_state_ = multiple_ ? Write::Token : Write::None;
            out_ = {0x00, 0x00, 0xFF}; // busy for two bytes, then ready
            return 0x05;               // data accepted
        default: write_state_ = Write::None; return 0xFF;
        }
    }

    void append(const std::array<std::uint8_t, 16>& bytes) { out_.insert(out_.end(), bytes.begin(), bytes.end()); }

    std::array<std::uint8_t, 16> csd() const
    {
        // CSD version 2.0: C_SIZE = blocks / 1024 - 1.
        const std::uint32_t c_size = static_cast<std::uint32_t>(memory_.size() / (512u * 1024u)) - 1u;
        return {0x40, 0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00, static_cast<std::uint8_t>((c_size >> 16) & 0x3Fu),
            static_cast<std::uint8_t>(c_size >> 8), static_cast<std::uint8_t>(c_size), 0x7F, 0x80, 0x0A, 0x40, 0x00,
            0x01};
    }

    static std::array<std::uint8_t, 16> cid()
    {
        return {0x53, 'S', 'N', 'S', 'I', 'M', 'S', 'D', 0x10, 0x00, 0x00, 0x00, 0x01, 0x01, 0xA0, 0x01};
    }

    std::vector<std::uint8_t> memory_;
    std::uint64_t version_ = 1;
    std::vector<std::uint8_t> command_;
    std::deque<std::uint8_t> out_;
    bool idle_ = false, app_ = false, reading_ = false, multiple_ = false;
    std::uint32_t block_ = 0;
    Write write_state_ = Write::None;
    std::vector<std::uint8_t> data_;
    int crc_bytes_ = 0;
};

// HX711 (Avia Semiconductor datasheet): 24-bit A/D converter for bridge
// sensors. DOUT goes low when a conversion is ready (10 per second); 24 pulses
// on PD_SCK shift the result out MSB first (DOUT changes after each rising
// edge), 1 to 3 more pulses choose the next input and gain (25: A x128, 26: B
// x32, 27: A x64). The code is the differential input (A+ - A-) x gain /
// (0.5 x AVDD) x 2^23, two's complement, clamped. AVDD is the voltage at E+
// (the excitation the module gives the bridge). PD_SCK high for over 60 us
// powers it down; the first conversion after comes 400 ms later... here at
// the next 100 ms period.
class Hx711 : public Device {
public:
    static constexpr std::array<const char*, 7> names{"SCK", "DT", "E+", "A+", "A-", "B+", "B-"};
    enum { SCK, DT, EP, AP, AN, BP, BN };
    static constexpr std::uint64_t period_ns = 100000000; // 10 samples per second

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "HX711", error)) {
            return false;
        }
        supply_ = supply_of(parameters);
        const auto rate = parameters.find("rate");
        period_ns_ = rate != parameters.end() && rate->second == "80" ? period_ns / 8 : period_ns;
        next_ = period_ns_;
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != index_[SCK]) {
            return;
        }
        if (level && !sck_) {
            high_since_ = time_ns;
            if (pulses_ < 24 && ready_) {
                ++pulses_; // the next bit appears after this edge
            } else if (pulses_ >= 24 && pulses_ < 27) {
                ++pulses_;
                gain_pulses_ = pulses_ - 24;
            }
        } else if (!level && sck_) {
            if (time_ns - high_since_ > 60000) {
                pulses_ = 0; // power-down and reset
                ready_ = false;
                next_ = time_ns + period_ns_;
            } else if (pulses_ >= 25) {
                ready_ = false; // read: DOUT high until the next conversion
                gain_ = gain_pulses_ == 2 ? 32 : gain_pulses_ == 3 ? 64 : 128;
                channel_b_ = gain_pulses_ == 2;
            }
        }
        sck_ = level;
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == index_[SCK]) {
            sck_ = level;
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin != index_[DT]) {
            return {};
        }
        if (!ready_ || pulses_ == 0) {
            return push_pull(!ready_); // high while converting, low when ready
        }
        if (pulses_ <= 24) {
            return push_pull(((code_ >> (24 - pulses_)) & 1u) != 0);
        }
        return push_pull(true);
    }

    std::uint64_t next_event_ns() const override { return next_; }

    void advance(std::uint64_t time_ns) override
    {
        if (time_ns < next_) {
            return;
        }
        next_ = time_ns + period_ns_;
        if (sck_) {
            return; // powered down
        }
        const double avdd = analog(index_[EP], time_ns, supply_);
        const double plus = analog(index_[channel_b_ ? BP : AP], time_ns, avdd / 2);
        const double minus = analog(index_[channel_b_ ? BN : AN], time_ns, avdd / 2);
        const double full = 0.5 * avdd / gain_;
        const double scaled = full > 0 ? (plus - minus) / full * 8388608.0 : 0.0;
        const auto value = static_cast<std::int32_t>(std::clamp(std::lround(scaled), -8388608L, 8388607L));
        code_ = static_cast<std::uint32_t>(value) & 0xFFFFFFu;
        ready_ = true;
        pulses_ = 0;
    }

private:
    std::array<std::size_t, names.size()> index_{};
    double supply_ = 5.0;
    std::uint64_t period_ns_ = period_ns;
    std::uint64_t next_ = period_ns;
    std::uint64_t high_since_ = 0;
    bool sck_ = false, ready_ = false, channel_b_ = false;
    int pulses_ = 0, gain_pulses_ = 1, gain_ = 128;
    std::uint32_t code_ = 0;
};

// Dallas/Maxim 1-Wire CRC-8 (polynomial x^8 + x^5 + x^4 + 1), LSB first.
std::uint8_t crc8(const std::uint8_t* bytes, std::size_t count)
{
    std::uint8_t crc = 0;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint8_t b = bytes[i];
        for (int k = 0; k < 8; ++k) {
            const bool mix = ((crc ^ b) & 1u) != 0;
            crc = static_cast<std::uint8_t>(crc >> 1);
            if (mix) {
                crc ^= 0x8C;
            }
            b = static_cast<std::uint8_t>(b >> 1);
        }
    }
    return crc;
}

// DS18B20 (Maxim datasheet): 1-Wire digital thermometer on DQ (open drain,
// 4.7 kOhm pull-up). A reset pulse (low for at least 480 us) gets a presence
// pulse; then a ROM command (READ 33, MATCH 55, SKIP CC, SEARCH F0, ALARM
// SEARCH EC) and a function command (CONVERT 44, READ SCRATCHPAD BE, WRITE
// SCRATCHPAD 4E, COPY 48, RECALL B8, READ POWER SUPPLY B4). Write slots shorter
// than 15 us are ones; in read slots the chip holds DQ low for 30 us to send a
// zero. A conversion takes 94 to 750 ms by resolution (read slots return 0
// meanwhile); the temperature register holds +85 C until the first one. The
// ROM code is family 0x28, a serial number from the "serial" parameter (or the
// part's designator) and its CRC, so several sensors share a bus (search ROM).
class Ds18b20 : public Device {
public:
    static constexpr std::array<const char*, 1> names{"DQ"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        std::array<std::size_t, 1> index{};
        if (!find_pins(pins, names, index, "DS18B20", error)) {
            return false;
        }
        dq_ = index[0];
        if (!number_parameter(parameters, "temperature", celsius_)) {
            error = "DS18B20: the temperature must be a number";
            return false;
        }
        // The serial number: the "serial" parameter in hex, or a hash of the designator.
        std::uint64_t serial = 0;
        const auto s = parameters.find("serial");
        if (s != parameters.end() && !s->second.empty() && s->second != "auto") {
            serial = std::strtoull(s->second.c_str(), nullptr, 16);
        } else {
            const auto d = parameters.find("@designator");
            const std::string text = d == parameters.end() ? std::string("U1") : d->second;
            serial = 1469598103934665603ull;
            for (const char c : text) {
                serial = (serial ^ static_cast<unsigned char>(c)) * 1099511628211ull;
            }
        }
        rom_[0] = 0x28;
        for (int i = 0; i < 6; ++i) {
            rom_[static_cast<std::size_t>(1 + i)] = static_cast<std::uint8_t>(serial >> (8 * i));
        }
        rom_[7] = crc8(rom_.data(), 7);
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        number_parameter({{name, value}}, "temperature", celsius_);
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != dq_ || level == line_) {
            return;
        }
        line_ = level;
        if (!level && low_) {
            own_ = true; // our own pull (presence pulse)
            return;
        }
        if (level && own_) {
            own_ = false;
            reading_ = false;
            return;
        }
        if (!level) {
            fell_ns_ = time_ns;
            reading_ = mode_ == Mode::Send;
            if (reading_) {
                read_slot(time_ns);
            }
            return;
        }
        if (reading_) {
            reading_ = false; // the end of a read slot, even if the next slot is a write
            if (time_ns - fell_ns_ < 480000) {
                return;
            }
        }
        const std::uint64_t width = time_ns - fell_ns_;
        if (width >= 480000) {
            reset(time_ns);
        } else if (mode_ == Mode::Receive) {
            receive_bit(width < 15000, time_ns);
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == dq_) {
            line_ = level;
        }
    }

    Output output(std::size_t pin) const override { return pin == dq_ && low_ ? open_drain_low() : Output{}; }

    std::uint64_t next_event_ns() const override
    {
        std::uint64_t next = no_event;
        if (release_ns_ > 0) {
            next = release_ns_;
        }
        if (presence_ns_ > 0) {
            next = std::min(next, presence_ns_);
        }
        if (converting_until_ > 0) {
            next = std::min(next, converting_until_);
        }
        return next;
    }

    void advance(std::uint64_t time_ns) override
    {
        if (presence_ns_ > 0 && time_ns >= presence_ns_) {
            presence_ns_ = 0;
            low_ = true;
            release_ns_ = time_ns + 120000; // presence pulse
        }
        if (release_ns_ > 0 && time_ns >= release_ns_) {
            release_ns_ = 0;
            low_ = false;
        }
        if (converting_until_ > 0 && time_ns >= converting_until_) {
            converting_until_ = 0;
            const int shift = 3 - ((scratchpad_[4] >> 5) & 3); // 12 bits: 0, 9 bits: 3
            const int sixteenths = static_cast<int>(std::lround(std::clamp(celsius_, -55.0, 125.0) * 16.0));
            const auto value = static_cast<std::uint16_t>(static_cast<std::int16_t>((sixteenths >> shift) << shift));
            scratchpad_[0] = static_cast<std::uint8_t>(value);
            scratchpad_[1] = static_cast<std::uint8_t>(value >> 8);
        }
    }

private:
    enum class Mode { Idle, Receive, Send };
    enum class Step { Rom, Match, Search, Function, Scratchpad, Done };

    void reset(std::uint64_t time_ns)
    {
        presence_ns_ = time_ns + 30000;
        mode_ = Mode::Receive;
        step_ = Step::Rom;
        bits_ = 0;
        byte_ = 0;
        count_ = 0;
    }

    void receive_bit(bool bit, std::uint64_t time_ns)
    {
        if (step_ == Step::Search) {
            // The master's choice after a bit and its complement: stay only if it is ours.
            if (bit != rom_bit(search_bit_)) {
                mode_ = Mode::Idle;
                return;
            }
            if (++search_bit_ == 64) {
                step_ = Step::Function;
                bits_ = 0;
                byte_ = 0;
                return;
            }
            mode_ = Mode::Send;
            search_phase_ = 0;
            return;
        }
        byte_ = static_cast<std::uint8_t>((byte_ >> 1) | (bit ? 0x80u : 0u));
        if (++bits_ < 8) {
            return;
        }
        bits_ = 0;
        received(byte_, time_ns);
        byte_ = 0;
    }

    void received(std::uint8_t byte, std::uint64_t time_ns)
    {
        switch (step_) {
        case Step::Rom:
            if (byte == 0x33) { // READ ROM
                send(std::vector<std::uint8_t>(rom_.begin(), rom_.end()));
                step_ = Step::Function;
            } else if (byte == 0xCC) {
                step_ = Step::Function;
            } else if (byte == 0x55) {
                step_ = Step::Match;
                count_ = 0;
            } else if (byte == 0xF0 || (byte == 0xEC && alarm())) {
                step_ = Step::Search;
                search_bit_ = 0;
                search_phase_ = 0;
                mode_ = Mode::Send;
            } else {
                mode_ = Mode::Idle;
            }
            break;
        case Step::Match:
            if (byte != rom_[static_cast<std::size_t>(count_)]) {
                mode_ = Mode::Idle;
            } else if (++count_ == 8) {
                step_ = Step::Function;
            }
            break;
        case Step::Function:
            function(byte, time_ns);
            break;
        case Step::Scratchpad:
            scratchpad_[static_cast<std::size_t>(2 + count_)] = byte; // TH, TL, configuration
            if (++count_ == 3) {
                scratchpad_[4] = static_cast<std::uint8_t>((scratchpad_[4] & 0x60u) | 0x1Fu);
                step_ = Step::Done;
            }
            break;
        default: break;
        }
    }

    void function(std::uint8_t byte, std::uint64_t time_ns)
    {
        switch (byte) {
        case 0x44: { // CONVERT T: 93.75 ms x 2^(resolution - 9)
            const int resolution = (scratchpad_[4] >> 5) & 3;
            converting_until_ = time_ns + (93750000ull << resolution);
            mode_ = Mode::Send;
            pending_.clear();
            busy_reads_ = true;
            break;
        }
        case 0xBE: {
            std::vector<std::uint8_t> bytes(scratchpad_.begin(), scratchpad_.end());
            bytes.push_back(crc8(scratchpad_.data(), scratchpad_.size()));
            send(bytes);
            break;
        }
        case 0x4E:
            step_ = Step::Scratchpad;
            count_ = 0;
            return;
        case 0x48:
        case 0xB8:
        case 0xB4: // copy, recall, power supply: done at once, externally powered
            send({0xFF});
            break;
        default: mode_ = Mode::Idle; break;
        }
        step_ = Step::Done;
    }

    void send(const std::vector<std::uint8_t>& bytes)
    {
        pending_.clear();
        for (const auto b : bytes) {
            for (int k = 0; k < 8; ++k) {
                pending_.push_back(((b >> k) & 1u) != 0);
            }
        }
        busy_reads_ = false;
        mode_ = Mode::Send;
    }

    // A read slot began: a zero holds DQ low for 30 us.
    void read_slot(std::uint64_t time_ns)
    {
        bool bit = true;
        if (step_ == Step::Search) {
            bit = search_phase_ == 0 ? rom_bit(search_bit_) : !rom_bit(search_bit_);
            if (++search_phase_ == 2) {
                mode_ = Mode::Receive; // the master's direction comes next
            }
        } else if (busy_reads_) {
            bit = converting_until_ == 0; // 0 while converting
        } else if (!pending_.empty()) {
            bit = pending_.front();
            pending_.pop_front();
            if (pending_.empty() && step_ == Step::Function) {
                mode_ = Mode::Receive; // READ ROM sent: the function command comes next
            }
        }
        if (!bit) {
            low_ = true;
            own_ = true; // the rising edge that ends this slot is ours
            release_ns_ = time_ns + 30000;
        }
    }

    bool rom_bit(int bit) const { return ((rom_[static_cast<std::size_t>(bit / 8)] >> (bit % 8)) & 1u) != 0; }

    bool alarm() const
    {
        const auto t = static_cast<std::int16_t>(scratchpad_[0] | (scratchpad_[1] << 8)) / 16;
        return t >= static_cast<std::int8_t>(scratchpad_[2]) || t <= static_cast<std::int8_t>(scratchpad_[3]);
    }

    std::size_t dq_ = 0;
    double celsius_ = 25.0;
    std::array<std::uint8_t, 8> rom_{};
    std::array<std::uint8_t, 8> scratchpad_{0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10}; // +85 C, 12 bits
    bool line_ = true, low_ = false, own_ = false, reading_ = false, busy_reads_ = false;
    std::uint64_t fell_ns_ = 0, presence_ns_ = 0, release_ns_ = 0, converting_until_ = 0;
    Mode mode_ = Mode::Idle;
    Step step_ = Step::Rom;
    int bits_ = 0, count_ = 0, search_bit_ = 0, search_phase_ = 0;
    std::uint8_t byte_ = 0;
    std::deque<bool> pending_;
};

// DHT22/AM2302 and DHT11 (Aosong datasheets): humidity and temperature sensors
// on one wire (DATA, open drain with a pull-up). The microcontroller pulls DATA
// low for at least 1 ms (18 ms for the DHT11) and releases it; the sensor
// answers 20-40 us later with 80 us low and 80 us high, then 40 bits, each 50 us
// low and 26 us (0) or 70 us (1) high: humidity, temperature (DHT22: tenths,
// the sign in bit 15; DHT11: whole and tenths bytes) and a checksum.
class Dht22 : public Device {
public:
    static constexpr std::array<const char*, 1> names{"DATA"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        std::array<std::size_t, 1> index{};
        if (!find_pins(pins, names, index, "DHT22", error)) {
            return false;
        }
        data_ = index[0];
        const auto model = parameters.find("model");
        dht11_ = model != parameters.end() && model->second == "DHT11";
        if (!number_parameter(parameters, "temperature", celsius_) || !number_parameter(parameters, "humidity", humidity_)) {
            error = "DHT22: the temperature and the humidity must be numbers";
            return false;
        }
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        number_parameter({{name, value}}, "temperature", celsius_);
        number_parameter({{name, value}}, "humidity", humidity_);
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != data_ || level == line_) {
            return;
        }
        line_ = level;
        if (low_ || !events_.empty()) {
            return; // answering
        }
        if (!level) {
            fell_ns_ = time_ns;
            armed_ = true;
        } else if (armed_ && time_ns - fell_ns_ >= (dht11_ ? 18000000ull : 800000ull)) {
            armed_ = false;
            answer(time_ns);
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == data_) {
            line_ = level;
        }
    }

    Output output(std::size_t pin) const override { return pin == data_ && low_ ? open_drain_low() : Output{}; }

    std::uint64_t next_event_ns() const override { return events_.empty() ? no_event : events_.front().first; }

    void advance(std::uint64_t time_ns) override
    {
        while (!events_.empty() && events_.front().first <= time_ns) {
            low_ = events_.front().second;
            events_.pop_front();
        }
    }

private:
    void answer(std::uint64_t time_ns)
    {
        std::array<std::uint8_t, 5> b{};
        const double rh = std::clamp(humidity_, 0.0, 100.0);
        const double t = std::clamp(celsius_, dht11_ ? 0.0 : -40.0, dht11_ ? 50.0 : 80.0);
        if (dht11_) {
            b[0] = static_cast<std::uint8_t>(rh);
            b[1] = static_cast<std::uint8_t>(std::lround((rh - b[0]) * 10) % 10);
            b[2] = static_cast<std::uint8_t>(t);
            b[3] = static_cast<std::uint8_t>(std::lround((t - b[2]) * 10) % 10);
        } else {
            const auto h = static_cast<std::uint16_t>(std::lround(rh * 10));
            const auto magnitude = static_cast<std::uint16_t>(std::lround(std::abs(t) * 10));
            const auto tt = static_cast<std::uint16_t>(magnitude | (t < 0 ? 0x8000u : 0u));
            b = {static_cast<std::uint8_t>(h >> 8), static_cast<std::uint8_t>(h), static_cast<std::uint8_t>(tt >> 8),
                static_cast<std::uint8_t>(tt), 0};
        }
        b[4] = static_cast<std::uint8_t>(b[0] + b[1] + b[2] + b[3]);
        std::uint64_t at = time_ns + 30000;
        auto pulse = [&](std::uint64_t low_ns, std::uint64_t high_ns) {
            events_.push_back({at, true});
            at += low_ns;
            events_.push_back({at, false});
            at += high_ns;
        };
        pulse(80000, 80000);
        for (const auto byte : b) {
            for (int k = 7; k >= 0; --k) {
                pulse(50000, ((byte >> k) & 1u) ? 70000 : 26000);
            }
        }
        events_.push_back({at, true}); // the end: 50 us low, then released
        events_.push_back({at + 50000, false});
    }

    std::size_t data_ = 0;
    bool dht11_ = false;
    double celsius_ = 25.0, humidity_ = 50.0;
    bool line_ = true, low_ = false, armed_ = false;
    std::uint64_t fell_ns_ = 0;
    std::deque<std::pair<std::uint64_t, bool>> events_; // time, DATA held low from then on
};

// HC-SR04 ultrasonic ranging module: a pulse of at least 10 us on TRIG starts
// a measurement; about 0.25 ms after its end ECHO goes high for the sound's
// round trip, 58 us per centimetre of the distance parameter (2 to 400 cm);
// beyond 400 cm it stays high for 38 ms (no echo). Triggers during a
// measurement are ignored.
class HcSr04 : public Device {
public:
    static constexpr std::array<const char*, 2> names{"TRIG", "ECHO"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "HC-SR04", error)) {
            return false;
        }
        if (!number_parameter(parameters, "distance", centimetres_)) {
            error = "HC-SR04: the distance must be a number";
            return false;
        }
        return true;
    }

    void set_parameter(const std::string& name, const std::string& value) override
    {
        number_parameter({{name, value}}, "distance", centimetres_);
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != index_[0] || level == trig_) {
            return;
        }
        trig_ = level;
        if (level) {
            rose_ns_ = time_ns;
        } else if (time_ns - rose_ns_ >= 10000 && start_ns_ == 0 && end_ns_ == 0) {
            const double round_trip = centimetres_ > 400 ? 38000000.0 : std::max(centimetres_, 2.0) * 58309.0;
            start_ns_ = time_ns + 250000;
            end_ns_ = start_ns_ + static_cast<std::uint64_t>(round_trip);
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == index_[0]) {
            trig_ = level;
        }
    }

    Output output(std::size_t pin) const override { return pin == index_[1] ? push_pull(echo_) : Output{}; }

    std::uint64_t next_event_ns() const override
    {
        return start_ns_ > 0 ? start_ns_ : end_ns_ > 0 ? end_ns_ : no_event;
    }

    void advance(std::uint64_t time_ns) override
    {
        if (start_ns_ > 0 && time_ns >= start_ns_) {
            start_ns_ = 0;
            echo_ = true;
        }
        if (start_ns_ == 0 && end_ns_ > 0 && time_ns >= end_ns_) {
            end_ns_ = 0;
            echo_ = false;
        }
    }

private:
    std::array<std::size_t, names.size()> index_{};
    double centimetres_ = 100.0;
    bool trig_ = false, echo_ = false;
    std::uint64_t rose_ns_ = 0, start_ns_ = 0, end_ns_ = 0;
};

// WS2812B addressable RGB LEDs (Worldsemi datasheet): DIN carries 24 bits per
// LED, green, red, blue, MSB first, at about 800 kHz; a high pulse longer than
// 0.6 us is a 1. The first LED keeps the first 24 bits, the next LED the next
// ones; bits beyond the strip go out on DOUT. Low for more than 50 us latches
// the colors and the next bits start again at the first LED. text() gives the
// strip as "Nx1 rgb" and a row of RRGGBB, one for each LED.
class Ws2812 : public Device {
public:
    static constexpr std::array<const char*, 2> names{"DIN", "DOUT"};

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "WS2812B", error)) {
            return false;
        }
        const auto count = parameters.find("count");
        const long n = count == parameters.end() ? 8 : std::strtol(count->second.c_str(), nullptr, 10);
        shown_.assign(static_cast<std::size_t>(std::clamp(n, 1L, 256L)), 0);
        incoming_ = shown_;
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != index_[0] || level == din_) {
            return;
        }
        din_ = level;
        if (level) {
            if (latch_ns_ > 0) {
                latch_ns_ = 0; // shorter than a reset: the frame goes on
            }
            rose_ns_ = time_ns;
            if (led_ >= incoming_.size()) {
                dout_ = true; // passed on
            }
            return;
        }
        latch_ns_ = time_ns + 50000;
        if (led_ >= incoming_.size()) {
            dout_ = false;
            return;
        }
        const bool bit = time_ns - rose_ns_ > 600;
        bits_ = (bits_ << 1) | (bit ? 1u : 0u);
        if (++count_ == 24) {
            // GRB on the wire: kept as RGB.
            incoming_[led_++] = ((bits_ << 8) & 0xFF0000u) | ((bits_ >> 8) & 0x00FF00u) | (bits_ & 0xFFu);
            bits_ = 0;
            count_ = 0;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == index_[0]) {
            din_ = level;
        }
    }

    Output output(std::size_t pin) const override { return pin == index_[1] ? push_pull(dout_) : Output{}; }

    std::uint64_t next_event_ns() const override { return latch_ns_ > 0 ? latch_ns_ : no_event; }

    void advance(std::uint64_t time_ns) override
    {
        if (latch_ns_ > 0 && time_ns >= latch_ns_) {
            latch_ns_ = 0;
            if (led_ > 0) {
                std::copy(incoming_.begin(), incoming_.begin() + static_cast<std::ptrdiff_t>(led_), shown_.begin());
                ++version_;
            }
            led_ = 0;
            count_ = 0;
            bits_ = 0;
        }
    }

    std::string text() const override
    {
        if (text_version_ != version_ || text_.empty()) {
            static const char* hex = "0123456789ABCDEF";
            text_ = std::to_string(shown_.size()) + "x1 rgb\n";
            for (const auto c : shown_) {
                for (int k = 20; k >= 0; k -= 4) {
                    text_ += hex[(c >> k) & 0xFu];
                }
            }
            text_version_ = version_;
        }
        return text_;
    }

private:
    std::array<std::size_t, names.size()> index_{};
    std::vector<std::uint32_t> shown_, incoming_;
    bool din_ = false, dout_ = false;
    std::uint64_t rose_ns_ = 0, latch_ns_ = 0;
    std::size_t led_ = 0;
    unsigned bits_ = 0;
    int count_ = 0;
    std::uint64_t version_ = 0;
    mutable std::uint64_t text_version_ = 0;
    mutable std::string text_;
};

// Hobby servo (SG90, MG90S): the width of each pulse on SIG sets the angle,
// from the "min" pulse (0 degrees) to the "max" pulse (180 degrees); pulses
// outside 0.3-3 ms are ignored. The horn turns at 600 degrees a second (0.1 s
// for 60 degrees) and holds its angle without pulses. text() gives the angle
// as "dial <degrees>".
class Servo : public Device {
public:
    static constexpr std::array<const char*, 1> names{"SIG"};
    static constexpr std::uint64_t step_ns = 2000000; // the horn moves in 2 ms steps

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        std::array<std::size_t, 1> index{};
        if (!find_pins(pins, names, index, "Servo", error)) {
            return false;
        }
        sig_ = index[0];
        if (!number_parameter(parameters, "min", min_us_) || !number_parameter(parameters, "max", max_us_)
            || max_us_ <= min_us_) {
            error = "Servo: the pulse widths must be numbers, the maximum above the minimum";
            return false;
        }
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != sig_ || level == sig_level_) {
            return;
        }
        sig_level_ = level;
        if (level) {
            rose_ns_ = time_ns;
            return;
        }
        const double us = static_cast<double>(time_ns - rose_ns_) / 1000.0;
        if (us < 300.0 || us > 3000.0) {
            return;
        }
        target_ = std::clamp((us - min_us_) / (max_us_ - min_us_) * 180.0, 0.0, 180.0);
        if (next_ns_ == no_event && std::abs(target_ - angle_) > 0.05) {
            next_ns_ = time_ns + step_ns;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == sig_) {
            sig_level_ = level;
        }
    }

    Output output(std::size_t) const override { return {}; }

    std::uint64_t next_event_ns() const override { return next_ns_; }

    void advance(std::uint64_t time_ns) override
    {
        if (next_ns_ == no_event || time_ns < next_ns_) {
            return;
        }
        const double step = 600.0 * static_cast<double>(step_ns) / 1e9;
        angle_ = target_ > angle_ ? std::min(target_, angle_ + step) : std::max(target_, angle_ - step);
        next_ns_ = std::abs(target_ - angle_) > 0.05 ? time_ns + step_ns : no_event;
    }

    std::string text() const override
    {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "dial %.1f", angle_);
        return buffer;
    }

private:
    std::size_t sig_ = 0;
    double min_us_ = 500.0, max_us_ = 2500.0;
    bool sig_level_ = false;
    std::uint64_t rose_ns_ = 0, next_ns_ = no_event;
    double angle_ = 90.0, target_ = 90.0;
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

// HD44780 (Hitachi) character LCD controller with its display, for 1, 2 or 4
// rows of 8 to 40 characters. Writes are taken at the falling edge of E (RS 0
// instruction, 1 data); with RW high the chip drives D0-D7 (or D4-D7 twice in
// 4-bit mode) while E is high: busy flag and address, or data. It starts in
// 8-bit mode; a function set with DL 0 switches to 4-bit transfers (high
// nibble first), so the usual 0x3, 0x3, 0x3, 0x2 start works. Instructions take
// 37 us (clear and home 1.52 ms); one written while the chip is busy is
// ignored, as on the chip, and reported once. The text shows the display RAM
// through the display shift: rows start at 0x00, 0x40, then 0x00 + columns and
// 0x40 + columns, as 20x4 and 16x4 modules map them.
class Hd44780 : public Device {
public:
    static constexpr std::array<const char*, 11> names{"RS", "RW", "E", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};
    enum { RS, RW, E, D0 };
    static constexpr std::uint64_t short_ns = 37000;
    static constexpr std::uint64_t long_ns = 1520000;

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "HD44780", error)) {
            return false;
        }
        role_.assign(pins.size(), -1);
        for (std::size_t i = 0; i < names.size(); ++i) {
            role_[index_[i]] = static_cast<int>(i);
        }
        auto number = [&](const char* name, int fallback) {
            const auto it = parameters.find(name);
            return it == parameters.end() ? fallback : std::atoi(it->second.c_str());
        };
        rows_ = std::clamp(number("rows", 2), 1, 4);
        columns_ = std::clamp(number("columns", 16), 1, 40);
        ddram_.fill(0x20);
        return true;
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin < role_.size() && role_[pin] >= 0) {
            inputs_[static_cast<std::size_t>(role_[pin])] = level;
        }
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin >= role_.size() || role_[pin] < 0) {
            return;
        }
        const auto role = static_cast<std::size_t>(role_[pin]);
        const bool was = inputs_[role];
        inputs_[role] = level;
        if (role != E || was == level) {
            return;
        }
        if (level) {
            if (inputs_[RW]) {
                begin_read(time_ns); // the chip drives D while E is high
            }
            return;
        }
        // E falls.
        if (reading_) {
            end_read();
        } else if (!inputs_[RW]) {
            latch(time_ns);
        }
    }

    Output output(std::size_t pin) const override
    {
        if (!reading_ || pin >= role_.size() || role_[pin] < D0) {
            return {};
        }
        const int bit = role_[pin] - D0;
        if (!eight_bit_ && bit < 4) {
            return {};
        }
        return push_pull(((out_ >> bit) & 1u) != 0);
    }

    std::string text() const override
    {
        std::string result;
        for (int r = 0; r < rows_; ++r) {
            if (r > 0) {
                result += '\n';
            }
            const int base = (r % 2 ? 0x40 : 0x00) + (r >= 2 ? columns_ : 0);
            for (int c = 0; c < columns_; ++c) {
                if (!display_on_) {
                    result += ' ';
                    continue;
                }
                const int line_length = two_lines_ ? 40 : 80;
                const int offset = ((base % 0x40) + c + shift_) % line_length;
                result += glyph(ddram_[static_cast<std::size_t>((base >= 0x40 ? 40 : 0) + offset)]);
            }
        }
        return result;
    }

private:
    // UTF-8 text for a character code of the A00 character ROM.
    static std::string glyph(std::uint8_t code)
    {
        if (code >= 0x20 && code <= 0x7D && code != 0x5C) {
            return std::string(1, static_cast<char>(code));
        }
        switch (code) {
        case 0x5C: return "\xC2\xA5";     // yen sign
        case 0x7E: return "\xE2\x86\x92"; // right arrow
        case 0x7F: return "\xE2\x86\x90"; // left arrow
        case 0xDF: return "\xC2\xB0";     // degree sign
        case 0xE4: return "\xC2\xB5";     // micro
        case 0xF4: return "\xCE\xA9";     // ohm
        case 0xFF: return "\xE2\x96\x88"; // full block
        default: break;
        }
        if (code < 0x10) {
            return "\xE2\x96\xAE"; // a custom character (CGRAM): shown as a block
        }
        return " ";
    }

    std::uint8_t bus_nibble(int first) const
    {
        std::uint8_t value = 0;
        for (int b = 0; b < 4; ++b) {
            value |= static_cast<std::uint8_t>((inputs_[static_cast<std::size_t>(D0 + first + b)] ? 1u : 0u) << b);
        }
        return value;
    }

    void latch(std::uint64_t time_ns)
    {
        std::uint8_t value = 0;
        if (eight_bit_) {
            value = static_cast<std::uint8_t>(bus_nibble(0) | (bus_nibble(4) << 4));
        } else if (!low_nibble_next_) {
            high_ = bus_nibble(4);
            low_nibble_next_ = true;
            return;
        } else {
            value = static_cast<std::uint8_t>((high_ << 4) | bus_nibble(4));
            low_nibble_next_ = false;
        }
        if (time_ns < busy_until_) {
            if (!warned_busy_) {
                warned_busy_ = true;
                message("a byte was written while the LCD was busy and was ignored; wait 37 us after an "
                        "instruction and 1.52 ms after clear or home (or poll the busy flag)");
            }
            return;
        }
        if (inputs_[RS]) {
            write_data(value);
            busy_until_ = time_ns + short_ns;
        } else {
            busy_until_ = time_ns + instruction(value);
        }
    }

    std::uint64_t instruction(std::uint8_t value)
    {
        if (value & 0x80u) {
            address_ = value & 0x7Fu;
            in_cgram_ = false;
        } else if (value & 0x40u) {
            address_ = value & 0x3Fu;
            in_cgram_ = true;
        } else if (value & 0x20u) {
            eight_bit_ = (value & 0x10u) != 0;
            two_lines_ = (value & 0x08u) != 0;
            low_nibble_next_ = false;
        } else if (value & 0x10u) {
            const bool display = (value & 0x08u) != 0;
            const bool right = (value & 0x04u) != 0;
            if (display) {
                shift_ = (shift_ + (right ? 39 : 1)) % 40; // the text moves the other way
            } else {
                step(right);
            }
        } else if (value & 0x08u) {
            display_on_ = (value & 0x04u) != 0;
        } else if (value & 0x04u) {
            increment_ = (value & 0x02u) != 0;
            shift_with_write_ = (value & 0x01u) != 0;
        } else if (value & 0x02u) {
            address_ = 0;
            in_cgram_ = false;
            shift_ = 0;
            return long_ns;
        } else if (value & 0x01u) {
            ddram_.fill(0x20);
            address_ = 0;
            in_cgram_ = false;
            shift_ = 0;
            increment_ = true;
            return long_ns;
        }
        return short_ns;
    }

    // Display RAM index of an address: 0x00-0x27 and 0x40-0x67 in two-line mode, 0x00-0x4F in one-line mode.
    std::size_t ram_index(std::uint8_t address) const
    {
        if (!two_lines_) {
            return std::min<std::size_t>(address, 79);
        }
        return (address >= 0x40 ? 40u : 0u) + std::min<std::size_t>(address & 0x3Fu, 39);
    }

    void step(bool forward)
    {
        if (in_cgram_) {
            address_ = static_cast<std::uint8_t>((address_ + (forward ? 1 : 63)) & 0x3Fu);
            return;
        }
        int a = address_;
        if (two_lines_) {
            // 0x27 is followed by 0x40, 0x67 by 0x00.
            if (forward) {
                a = a == 0x27 ? 0x40 : (a == 0x67 ? 0x00 : a + 1);
            } else {
                a = a == 0x40 ? 0x27 : (a == 0x00 ? 0x67 : a - 1);
            }
        } else {
            a = forward ? (a + 1) % 80 : (a + 79) % 80;
        }
        address_ = static_cast<std::uint8_t>(a);
    }

    void write_data(std::uint8_t value)
    {
        if (in_cgram_) {
            cgram_[address_ & 0x3Fu] = value;
        } else {
            ddram_[ram_index(address_)] = value;
            if (shift_with_write_) {
                shift_ = (shift_ + (increment_ ? 1 : 39)) % 40;
            }
        }
        step(increment_);
    }

    void begin_read(std::uint64_t time_ns)
    {
        if (eight_bit_ || !low_nibble_next_) {
            if (inputs_[RS]) {
                read_value_ = in_cgram_ ? cgram_[address_ & 0x3Fu] : ddram_[ram_index(address_)];
            } else {
                read_value_ = static_cast<std::uint8_t>((time_ns < busy_until_ ? 0x80u : 0u) | (address_ & 0x7Fu));
            }
        }
        if (eight_bit_) {
            out_ = read_value_;
        } else {
            out_ = static_cast<std::uint8_t>(low_nibble_next_ ? (read_value_ << 4) : (read_value_ & 0xF0u));
        }
        reading_ = true;
    }

    void end_read()
    {
        reading_ = false;
        if (!eight_bit_) {
            low_nibble_next_ = !low_nibble_next_;
            if (low_nibble_next_) {
                return; // the second nibble follows
            }
        }
        if (inputs_[RS]) {
            step(increment_); // reading data moves the address on
        }
    }

    std::array<std::size_t, names.size()> index_{};
    std::vector<int> role_;
    std::array<bool, names.size()> inputs_{};
    int rows_ = 2;
    int columns_ = 16;
    std::array<std::uint8_t, 80> ddram_{};
    std::array<std::uint8_t, 64> cgram_{};
    std::uint8_t address_ = 0;
    bool in_cgram_ = false;
    bool eight_bit_ = true;
    bool two_lines_ = false;
    bool display_on_ = false;
    bool increment_ = true;
    bool shift_with_write_ = false;
    int shift_ = 0;
    bool low_nibble_next_ = false;
    std::uint8_t high_ = 0;
    std::uint64_t busy_until_ = 0;
    bool reading_ = false;
    std::uint8_t read_value_ = 0;
    std::uint8_t out_ = 0;
    bool warned_busy_ = false;
};

// Parallel memories (SN-212), generic asynchronous ROM and static RAM of
// 16 words to 64 Ki words of 8 bits, like a 27C256 or a 62256: A0-A15 select a
// word (address pins beyond the size are ignored), D0-D7 carry it. With CE and
// OE low (and WE high) the memory drives D0-D7 with the addressed word; a RAM
// stores D0-D7 at the end of a write, the rising edge of WE or CE while the
// other is low. Ideal: no access or write times.
class ParallelMemory : public Device {
public:
    bool init(bool ram, const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        ram_ = ram;
        const auto size = parameters.find("size");
        words_ = memory_words(size == parameters.end() ? std::string() : size->second);
        if (words_ == 0) {
            error = "memory size must be a power of two from 16 to 64K words";
            return false;
        }
        while ((std::size_t{1} << address_bits_) < words_) {
            ++address_bits_;
        }
        memory_.assign(words_, static_cast<std::uint8_t>(ram ? 0x00 : 0xFF));
        role_.assign(pins.size(), -1);
        for (std::size_t i = 0; i < pins.size(); ++i) {
            const std::string& name = pins[i];
            if (name.size() >= 2 && (name[0] == 'A' || name[0] == 'D') && std::isdigit(static_cast<unsigned char>(name[1]))) {
                const int bit = std::atoi(name.c_str() + 1);
                role_[i] = (name[0] == 'A' ? A0 : D0) + bit;
            } else if (name == "CE") {
                role_[i] = CE;
            } else if (name == "OE") {
                role_[i] = OE;
            } else if (name == "WE") {
                role_[i] = WE;
            }
        }
        for (const int role : {CE, OE}) {
            if (std::find(role_.begin(), role_.end(), role) == role_.end()) {
                error = std::string(ram ? "RAM" : "ROM") + ": pin " + (role == CE ? "CE" : "OE") + " is missing";
                return false;
            }
        }
        if (ram && std::find(role_.begin(), role_.end(), static_cast<int>(WE)) == role_.end()) {
            error = "RAM: pin WE is missing";
            return false;
        }
        return true;
    }

    // "2K" -> 2048; 0 unless a power of two from 16 to 65536.
    static std::size_t memory_words(const std::string& text)
    {
        if (text.empty()) {
            return 256;
        }
        char* end = nullptr;
        unsigned long value = std::strtoul(text.c_str(), &end, 10);
        if (end && (*end == 'K' || *end == 'k')) {
            value *= 1024;
        }
        const bool power = value >= 16 && value <= 65536 && (value & (value - 1)) == 0;
        return power ? static_cast<std::size_t>(value) : 0;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t) override
    {
        if (pin >= role_.size() || role_[pin] < 0) {
            return;
        }
        const auto role = static_cast<std::size_t>(role_[pin]);
        const bool was = inputs_[role];
        inputs_[role] = level;
        // A write ends at the rising edge of WE or CE while the other one is low.
        if (ram_ && level && !was && ((role == WE && !inputs_[CE]) || (role == CE && !inputs_[WE]))) {
            std::uint8_t value = 0;
            for (int b = 0; b < 8; ++b) {
                value = static_cast<std::uint8_t>(value | (inputs_[D0 + static_cast<std::size_t>(b)] ? 1u << b : 0u));
            }
            auto& word = memory_[address()];
            if (word != value) {
                word = value;
                ++version_;
            }
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
        if (pin >= role_.size() || role_[pin] < D0 || role_[pin] > D7) {
            return {};
        }
        const bool reading = !inputs_[CE] && !inputs_[OE] && (!ram_ || inputs_[WE]);
        if (!reading) {
            return {}; // high impedance: the data pins are inputs
        }
        return push_pull(((memory_[address()] >> (role_[pin] - D0)) & 1u) != 0);
    }

    std::vector<std::uint8_t> memory() const override { return memory_; }
    std::uint64_t memory_version() const override { return version_; }
    void load_memory(const std::vector<std::uint8_t>& bytes) override
    {
        std::fill(memory_.begin(), memory_.end(), static_cast<std::uint8_t>(ram_ ? 0x00 : 0xFF));
        std::copy_n(bytes.begin(), std::min(bytes.size(), memory_.size()), memory_.begin());
        ++version_;
    }

private:
    enum : int { A0 = 0, D0 = 16, D7 = D0 + 7, CE = 24, OE, WE, roles };

    std::size_t address() const
    {
        std::size_t a = 0;
        for (std::size_t b = 0; b < address_bits_; ++b) {
            a |= inputs_[A0 + b] ? std::size_t{1} << b : 0;
        }
        return a;
    }

    bool ram_ = false;
    std::size_t words_ = 0;
    std::size_t address_bits_ = 0;
    std::vector<int> role_;
    // Control inputs high (deselected) until the run gives the first levels.
    std::array<bool, roles> inputs_ = [] {
        std::array<bool, roles> a{};
        a[CE] = a[OE] = a[WE] = true;
        return a;
    }();
    std::vector<std::uint8_t> memory_;
    std::uint64_t version_ = 0;
};

// Serial Port (SN-237): a serial port of the computer on the circuit, like a
// USB-serial adapter at TTL levels. Frames on RX (a low start bit, the data
// bits LSB first, an optional parity bit, stop bits high) are read at the
// middle of each bit and written to the port; bytes from the port go out on
// TX with the same settings, one after the other. The port is read every
// millisecond of simulated time. The coupling is in the run's time: at real
// time both sides keep pace; a slower run sends the computer's bytes later
// (up to 64 KB wait), a faster one passes the circuit's bytes on sooner.
class SerialPortDevice : public Device {
public:
    static constexpr std::array<const char*, 2> names{"RX", "TX"};
    static constexpr std::uint64_t poll_ns = 1000000;
    static constexpr std::size_t waiting_limit = 65536;

    bool init(const std::vector<std::string>& pins, const std::map<std::string, std::string>& parameters,
        std::string& error)
    {
        if (!find_pins(pins, names, index_, "Serial port", error)) {
            return false;
        }
        const auto text = [&parameters](const char* name, const char* fallback) {
            const auto it = parameters.find(name);
            return it == parameters.end() ? std::string(fallback) : it->second;
        };
        SerialSettings settings;
        settings.name = text("port", "");
        while (!settings.name.empty() && std::isspace(static_cast<unsigned char>(settings.name.back()))) {
            settings.name.pop_back();
        }
        while (!settings.name.empty() && std::isspace(static_cast<unsigned char>(settings.name.front()))) {
            settings.name.erase(settings.name.begin());
        }
        double baud = 115200;
        if (!number_parameter(parameters, "baud", baud) || baud < 50 || baud > 4000000) {
            error = "Serial port: the baud rate must be a number from 50 to 4000000";
            return false;
        }
        settings.baud = static_cast<unsigned>(std::lround(baud));
        settings.data_bits = std::atoi(text("data_bits", "8").c_str());
        settings.stop_bits = std::atoi(text("stop_bits", "1").c_str());
        const std::string parity = text("parity", "None");
        settings.parity = parity.empty() ? 'N' : static_cast<char>(std::toupper(static_cast<unsigned char>(parity[0])));
        if (settings.data_bits < 5 || settings.data_bits > 8 || (settings.stop_bits != 1 && settings.stop_bits != 2)
            || (settings.parity != 'N' && settings.parity != 'E' && settings.parity != 'O')) {
            error = "Serial port: data bits 5 to 8, parity None, Even or Odd, 1 or 2 stop bits";
            return false;
        }
        settings_ = settings;
        bit_ns_ = 1e9 / settings.baud;
        if (settings.name.empty()) {
            message("choose the computer's port in its properties; the part only keeps TX high.");
            return true;
        }
        std::string why;
        port_ = open_serial_port(settings, why);
        if (!port_) {
            // The run goes on without the port (a project from another computer).
            message("the port " + settings.name + " cannot be opened (" + why
                + "); the part only keeps TX high. Choose another port in its properties.");
        }
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override
    {
        if (pin != index_[0]) {
            return;
        }
        receive_until(time_ns, false); // the bits sampled before this edge saw the old level
        if (!receiving_ && rx_ && !level) {
            receiving_ = true; // the start bit's falling edge
            rx_start_ = time_ns;
            rx_bit_ = 0;
            rx_byte_ = 0;
            rx_ones_ = 0;
        }
        rx_ = level;
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin == index_[0]) {
            rx_ = level;
        }
    }

    Output output(std::size_t pin) const override { return pin == index_[1] ? push_pull(tx_) : Output{}; }

    std::uint64_t next_event_ns() const override
    {
        std::uint64_t next = port_ ? next_poll_ : no_event;
        if (receiving_) {
            next = std::min(next, sample_time());
        }
        if (sending_) {
            next = std::min(next, bit_time(tx_start_, static_cast<double>(tx_index_)));
        }
        return next;
    }

    void advance(std::uint64_t time_ns) override
    {
        receive_until(time_ns, true);
        send_until(time_ns);
        if (port_ && time_ns >= next_poll_) {
            next_poll_ = time_ns + poll_ns;
            std::uint8_t buffer[512];
            for (std::size_t count = port_->read(buffer, sizeof(buffer)); count > 0;
                 count = port_ ? port_->read(buffer, sizeof(buffer)) : 0) {
                if (waiting_.size() + count > waiting_limit) {
                    if (!overflow_reported_) {
                        message("the computer sends faster than the simulation passes its bytes on; bytes were "
                                "dropped (run at real-time speed or slow the sender).");
                        overflow_reported_ = true;
                    }
                    break;
                }
                waiting_.insert(waiting_.end(), buffer, buffer + count);
            }
            if (!sending_ && !waiting_.empty()) {
                start_frame(time_ns);
            }
        }
    }

private:
    std::uint64_t bit_time(std::uint64_t start, double bits) const
    {
        return start + static_cast<std::uint64_t>(std::llround(bits * bit_ns_));
    }

    std::uint64_t sample_time() const { return bit_time(rx_start_, rx_bit_ + 0.5); }

    int parity_bits() const { return settings_.parity == 'N' ? 0 : 1; }

    // Samples the bits whose middle is before `time_ns` (or at it, when `inclusive`).
    void receive_until(std::uint64_t time_ns, bool inclusive)
    {
        while (receiving_ && (sample_time() < time_ns || (inclusive && sample_time() == time_ns))) {
            sample(rx_);
        }
    }

    void sample(bool level)
    {
        const int data = settings_.data_bits;
        if (rx_bit_ == 0) {
            if (level) {
                receiving_ = false; // a glitch, not a start bit
                return;
            }
        } else if (rx_bit_ <= data) {
            if (level) {
                rx_byte_ |= static_cast<std::uint8_t>(1u << (rx_bit_ - 1));
                ++rx_ones_;
            }
        } else if (rx_bit_ == data + 1 && parity_bits() == 1) {
            rx_ones_ += level ? 1 : 0;
        } else {
            receiving_ = false; // the first stop bit ends the frame
            const bool parity_ok = parity_bits() == 0 || (rx_ones_ % 2 == (settings_.parity == 'O' ? 1 : 0));
            if (!level || !parity_ok) {
                if (!frame_error_reported_) {
                    message(!level ? "a frame on RX has no stop bit: check that the baud rate and format match the "
                                     "sender's; such frames are dropped."
                                   : "a frame on RX has a wrong parity bit: check the parity setting; such frames are "
                                     "dropped.");
                    frame_error_reported_ = true;
                }
                return;
            }
            if (port_ && !port_->write(&rx_byte_, 1)) {
                message("the port " + settings_.name + " stopped answering (unplugged?); the part only keeps TX high.");
                port_.reset();
            }
            return;
        }
        ++rx_bit_;
    }

    void start_frame(std::uint64_t time_ns)
    {
        const std::uint8_t byte = waiting_.front();
        waiting_.pop_front();
        tx_bits_.clear();
        tx_bits_.push_back(false); // start bit
        int ones = 0;
        for (int k = 0; k < settings_.data_bits; ++k) {
            const bool bit = ((byte >> k) & 1u) != 0;
            tx_bits_.push_back(bit);
            ones += bit ? 1 : 0;
        }
        if (parity_bits() == 1) {
            tx_bits_.push_back((ones % 2 == 1) != (settings_.parity == 'O'));
        }
        tx_bits_.insert(tx_bits_.end(), static_cast<std::size_t>(settings_.stop_bits), true);
        sending_ = true;
        tx_start_ = time_ns;
        tx_ = tx_bits_[0];
        tx_index_ = 1;
    }

    void send_until(std::uint64_t time_ns)
    {
        while (sending_ && bit_time(tx_start_, static_cast<double>(tx_index_)) <= time_ns) {
            if (tx_index_ < tx_bits_.size()) {
                tx_ = tx_bits_[tx_index_++];
                continue;
            }
            sending_ = false; // the last stop bit is over
            if (!waiting_.empty()) {
                start_frame(bit_time(tx_start_, static_cast<double>(tx_index_)));
            }
        }
    }

    std::array<std::size_t, names.size()> index_{};
    SerialSettings settings_;
    std::unique_ptr<SerialPort> port_;
    double bit_ns_ = 1e9 / 115200;
    std::uint64_t next_poll_ = 0;
    // RX: the circuit's frames.
    bool rx_ = true;
    bool receiving_ = false;
    std::uint64_t rx_start_ = 0;
    int rx_bit_ = 0;
    std::uint8_t rx_byte_ = 0;
    int rx_ones_ = 0;
    bool frame_error_reported_ = false;
    // TX: the computer's bytes.
    std::deque<std::uint8_t> waiting_;
    std::vector<bool> tx_bits_;
    std::size_t tx_index_ = 0;
    std::uint64_t tx_start_ = 0;
    bool sending_ = false;
    bool tx_ = true; // idle high
    bool overflow_reported_ = false;
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
    if (kind == "74hc161" || kind == "74hc193" || kind == "74hc165" || kind == "cd4017") {
        std::unique_ptr<Device> device;
        bool ok = false;
        if (kind == "74hc161") {
            auto d = std::make_unique<Hc161>();
            ok = d->bind(pins, Hc161::names, "74HC161", error);
            device = std::move(d);
        } else if (kind == "74hc193") {
            auto d = std::make_unique<Hc193>();
            ok = d->bind(pins, Hc193::names, "74HC193", error);
            device = std::move(d);
        } else if (kind == "74hc165") {
            auto d = std::make_unique<Hc165>();
            ok = d->bind(pins, Hc165::names, "74HC165", error);
            device = std::move(d);
        } else {
            auto d = std::make_unique<Cd4017>();
            ok = d->bind(pins, Cd4017::names, "CD4017", error);
            device = std::move(d);
        }
        return ok ? std::move(device) : nullptr;
    }
    if (kind == "24c02") {
        auto device = std::make_unique<Eeprom24c02>();
        return device->init(pins, error) ? std::move(device) : nullptr;
    }
    if (kind == "lm75") {
        auto device = std::make_unique<Lm75>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "bmp280") {
        auto device = std::make_unique<Bmp280>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "mpu6050") {
        auto device = std::make_unique<Mpu6050>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "ds3231") {
        auto device = std::make_unique<Ds3231>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "tcs34725") {
        auto device = std::make_unique<Tcs34725>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "ssd1306") {
        auto device = std::make_unique<Ssd1306>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "mcp3208") {
        auto device = std::make_unique<Mcp3208>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "mcp41010") {
        auto device = std::make_unique<Mcp41010>();
        return device->init(pins, error) ? std::move(device) : nullptr;
    }
    if (kind == "w25q32") {
        auto device = std::make_unique<W25q32>();
        return device->init(pins, error) ? std::move(device) : nullptr;
    }
    if (kind == "sdcard") {
        auto device = std::make_unique<SdCard>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "hx711") {
        auto device = std::make_unique<Hx711>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "ds18b20") {
        auto device = std::make_unique<Ds18b20>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "dht22") {
        auto device = std::make_unique<Dht22>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "hcsr04") {
        auto device = std::make_unique<HcSr04>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "ws2812") {
        auto device = std::make_unique<Ws2812>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "servo") {
        auto device = std::make_unique<Servo>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "pcf8574") {
        auto device = std::make_unique<Pcf8574>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "hd44780") {
        auto device = std::make_unique<Hd44780>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "max7219") {
        auto device = std::make_unique<Max7219>();
        if (!device->init(pins, error)) {
            return nullptr;
        }
        return device;
    }
    if (kind == "serial_port") {
        auto device = std::make_unique<SerialPortDevice>();
        return device->init(pins, parameters, error) ? std::move(device) : nullptr;
    }
    if (kind == "rom" || kind == "ram") {
        auto device = std::make_unique<ParallelMemory>();
        return device->init(kind == "ram", pins, parameters, error) ? std::move(device) : nullptr;
    }
    error = "unknown device kind \"" + kind + "\"";
    return nullptr;
}

} // namespace simnodus::devices
