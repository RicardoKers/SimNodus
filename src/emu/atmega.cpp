// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "atmega.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <utility>

namespace simnodus::emu {

namespace {

// ATmega8 data addresses (I/O address + 0x20), from the register summary of
// the ATmega8A datasheet.
enum : std::uint16_t {
    TWBR = 0x20, TWSR = 0x21, TWAR = 0x22, TWDR = 0x23, ADCL = 0x24, ADCH = 0x25, ADCSRA = 0x26, ADMUX = 0x27,
    ACSR = 0x28, UBRRL = 0x29, UCSRB = 0x2A, UCSRA = 0x2B, UDR = 0x2C, SPCR = 0x2D, SPSR = 0x2E, SPDR = 0x2F,
    PIND = 0x30, DDRD = 0x31, PORTD = 0x32, PINC = 0x33, DDRC = 0x34, PORTC = 0x35, PINB = 0x36, DDRB = 0x37,
    PORTB = 0x38, EECR = 0x3C, EEDR = 0x3D, EEARL = 0x3E, EEARH = 0x3F, UBRRH_UCSRC = 0x40, WDTCR = 0x41,
    ASSR = 0x42, OCR2 = 0x43, TCNT2 = 0x44, TCCR2 = 0x45, ICR1L = 0x46, ICR1H = 0x47, OCR1BL = 0x48,
    OCR1BH = 0x49, OCR1AL = 0x4A, OCR1AH = 0x4B, TCNT1L = 0x4C, TCNT1H = 0x4D, TCCR1B = 0x4E, TCCR1A = 0x4F,
    SFIOR = 0x50, OSCCAL = 0x51, TCNT0 = 0x52, TCCR0 = 0x53, MCUCSR = 0x54, MCUCR = 0x55, TWCR = 0x56,
    SPMCR = 0x57, TIFR = 0x58, TIMSK = 0x59, GIFR = 0x5A, GICR = 0x5B,
};

// Interrupt vectors of the ATmega8.
enum : int {
    V_INT0 = 1, V_INT1 = 2, V_T2_COMP = 3, V_T2_OVF = 4, V_T1_CAPT = 5, V_T1_COMPA = 6, V_T1_COMPB = 7,
    V_T1_OVF = 8, V_T0_OVF = 9, V_SPI = 10, V_RXC = 11, V_UDRE = 12, V_TXC = 13, V_ADC = 14, V_EE_RDY = 15,
    V_ANA_COMP = 16, V_TWI = 17, V_SPM_RDY = 18, V_COUNT = 19
};

// Pins by function (index = port * 8 + bit; B = 0, C = 1, D = 2).
constexpr int PB0 = 0, PB1 = 1, PB2 = 2, PB3 = 3, PB4 = 4, PB5 = 5, PB6 = 6, PB7 = 7, PC4 = 12, PC5 = 13, PC6 = 14, PD0 = 16, PD1 = 17, PD2 = 18,
              PD3 = 19, PD4 = 20, PD5 = 21;

constexpr std::uint32_t flash_bytes = 8192;
constexpr std::uint32_t page_words = 32;
constexpr std::size_t eeprom_bytes = 512;
// The watchdog and the EEPROM and flash write timers run on the chip's
// 1 MHz watchdog oscillator (nominal at 5 V; it varies with the supply and
// the temperature, so these times are labeled approximate).
constexpr std::uint64_t eeprom_write_ns = 8'448'000; // 8448 cycles
constexpr std::uint64_t flash_write_ns = 4'100'000;  // 3.7-4.5 ms in the datasheet

bool bit(std::uint8_t value, int n) { return ((value >> n) & 1u) != 0; }

} // namespace

// ---------------------------------------------------------------------------
// Peripheral base.

class Atmega::Peripheral : public TimedDevice {
public:
    explicit Peripheral(Atmega& machine) : m_(machine) {}
    virtual std::vector<std::uint16_t> addresses() const = 0;
    virtual std::uint8_t read(std::uint16_t address) = 0;
    virtual void write(std::uint16_t address, std::uint8_t value) = 0;
    virtual void reset() = 0;
    std::uint64_t next_event() const override { return no_event; }
    void on_time(std::uint64_t) override {}
    // A pin this peripheral drives, with its level.
    virtual bool drives(int, bool&) const { return false; }
    // A pin this peripheral makes an input (RXD with the receiver on).
    virtual bool takes_input(int) const { return false; }
    virtual void pad_changed(int, bool, std::uint64_t) {}

protected:
    std::uint64_t now() const { return m_.cpu_.cycles(); }
    Atmega& m_;
};

// ---------------------------------------------------------------------------
// External interrupts INT0 (PD2) and INT1 (PD3); MCUCR's ISCx1:0 select low
// level (00), any change (01), falling (10) or rising edge (11).

class Atmega::ExternalInterrupts : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {}; }
    std::uint8_t read(std::uint16_t) override { return 0; }
    void write(std::uint16_t, std::uint8_t) override {}
    void reset() override {}
    void pad_changed(int pin, bool level, std::uint64_t) override
    {
        const int n = pin == PD2 ? 0 : pin == PD3 ? 1 : -1;
        if (n < 0) {
            return;
        }
        const int sense = (m_.mcucr_ >> (2 * n)) & 3;
        if ((sense == 1) || (sense == 2 && !level) || (sense == 3 && level)) {
            m_.set_flag(n == 0 ? V_INT0 : V_INT1, true);
        } else {
            m_.update_interrupts(); // a level interrupt follows the pin
        }
    }
    // Low-level mode: pending while the pin is low; no flag.
    bool level_pending(int n) const
    {
        return ((m_.mcucr_ >> (2 * n)) & 3) == 0 && !m_.pad_level(n == 0 ? PD2 : PD3);
    }
};

// ---------------------------------------------------------------------------
// Ports B, C and D.

class Atmega::Ports : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override
    {
        return {PINB, DDRB, PORTB, PINC, DDRC, PORTC, PIND, DDRD, PORTD};
    }
    std::uint8_t read(std::uint16_t address) override
    {
        const int port = address >= PINB ? 0 : address >= PINC ? 1 : 2;
        const int reg = (address - (port == 0 ? PINB : port == 1 ? PINC : PIND));
        if (reg == 0) { // PINx: the pin levels (the input synchronizer's 1.5-cycle delay is not modelled)
            std::uint8_t value = 0;
            for (int b = 0; b < 8; ++b) {
                const int p = port * 8 + b;
                if (m_.pin_exists(p) && m_.pin_usable(p) && m_.pad_level(p)) {
                    value = static_cast<std::uint8_t>(value | (1u << b));
                }
            }
            return value;
        }
        return reg == 1 ? m_.ddr_[static_cast<std::size_t>(port)] : m_.port_[static_cast<std::size_t>(port)];
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        const int port = address >= PINB ? 0 : address >= PINC ? 1 : 2;
        const int reg = (address - (port == 0 ? PINB : port == 1 ? PINC : PIND));
        if (port == 1) {
            value &= 0x7F; // port C has PC0..PC6
        }
        if (reg == 0) {
            return; // PINx is read-only on the ATmega8 (no toggle by writing 1)
        }
        (reg == 1 ? m_.ddr_ : m_.port_)[static_cast<std::size_t>(port)] = value;
        m_.refresh_pins();
    }
    void reset() override {}
};

// ---------------------------------------------------------------------------
// The counter shared by the three timers: counts on the ticks of a prescaled
// clock (or an external pin), with the waveform modes, the compare channels
// and their output pins. Events (a compare match, TOP, BOTTOM, an overflow)
// happen at the tick that leaves the counter value they belong to, as in the
// datasheet's timing diagrams: a CTC counter at TOP is cleared, and the flag
// set, on the next tick.

namespace {

struct Channel {
    std::uint16_t ocr = 0;    // the compare value in use
    std::uint16_t buffer = 0; // OCR as written (double-buffered in PWM modes)
    std::uint8_t com = 0;
    int pin = -1;
    int vector = 0;
    bool level = false;       // the output compare's level
    bool connected = false;   // the output overrides the port (with DDR set)
};

enum class Shape { Up, UpDown };
enum class Update { Immediate, Top, Bottom };

} // namespace

class TimerCore {
public:
    explicit TimerCore(int bits) : max_(static_cast<std::uint16_t>((1u << bits) - 1)) {}

    // Mode description, set by the timer from its control registers.
    Shape shape = Shape::Up;
    Update update = Update::Immediate;
    bool pwm = false;        // fast or phase-correct PWM
    bool tov_at_top = false; // fast PWM: TOV at TOP; otherwise at MAX (up) or BOTTOM (up-down)
    bool clear_at_top = false; // CTC and fast PWM: TOP goes to BOTTOM
    bool toggle_a = false;   // COMx = 01 toggles channel A in PWM modes with OCRA as TOP
    int top_source = 0;      // 0: fixed, 1: channel A's OCR, 2: ICR
    std::uint16_t fixed_top = 0;
    std::uint16_t icr = 0;
    std::array<Channel, 2> ch{};
    int channels = 0;
    int tov_vector = 0;
    int icf_vector = 0;      // flag set at TOP when ICR is TOP

    std::uint16_t count = 0;
    bool down = false;
    bool blocked = false;    // a compare match is blocked on the next tick after a TCNT write

    std::uint16_t top() const
    {
        return top_source == 1 ? ch[0].ocr : top_source == 2 ? icr : fixed_top;
    }
    std::uint16_t max() const { return max_; }

    // Ticks until the next one that leaves a value with an event.
    std::uint64_t ticks_to_event() const
    {
        const std::uint16_t t = top();
        std::uint64_t best = no_event;
        auto consider = [&](std::uint16_t v) {
            std::uint64_t d = no_event;
            if (!down) {
                if (v >= count) {
                    d = static_cast<std::uint64_t>(v - count) + 1;
                }
            } else if (v <= count) {
                d = static_cast<std::uint64_t>(count - v) + 1;
            }
            best = std::min(best, d);
        };
        for (int i = 0; i < channels; ++i) {
            consider(ch[static_cast<std::size_t>(i)].ocr);
        }
        if (count > t && !down) {
            consider(max_);
        } else {
            consider(t);
            consider(0);
            consider(max_);
        }
        return best;
    }

    // One tick, leaving the present value: compare matches, then TOP, BOTTOM
    // or MAX. `act` receives (event, channel, up): 0 = compare match,
    // 1 = BOTTOM reached in fast PWM (outputs set or cleared).
    template <typename Act> void tick(Act&& act)
    {
        const std::uint16_t v = count;
        const std::uint16_t t = top();
        // Direction of this tick.
        bool up = !down;
        if (shape == Shape::UpDown) {
            if (v >= t) {
                up = false; // turning at TOP
            } else if (v == 0) {
                up = true; // turning at BOTTOM
            }
        }
        if (!blocked) {
            for (int i = 0; i < channels; ++i) {
                if (ch[static_cast<std::size_t>(i)].ocr == v && v <= t) {
                    act(0, i, up);
                }
            }
        }
        blocked = false;
        if (shape == Shape::UpDown) {
            if (v >= t) {
                if (update == Update::Top) {
                    load_buffers();
                }
                if (icf_vector && top_source == 2) {
                    act(2, icf_vector, false);
                }
                count = static_cast<std::uint16_t>(t == 0 ? 0 : v - 1);
                if (t == 0) {
                    count = 0;
                }
                down = true;
            } else if (v == 0) {
                if (update == Update::Bottom) {
                    load_buffers();
                }
                act(2, tov_vector, false); // TOV at BOTTOM
                count = 1;
                down = false;
            } else {
                count = static_cast<std::uint16_t>(up ? v + 1 : v - 1);
            }
            return;
        }
        if (v == t && clear_at_top) {
            if (update == Update::Top || update == Update::Bottom) {
                load_buffers();
            }
            if (tov_at_top) {
                act(2, tov_vector, false);
            }
            if (icf_vector && top_source == 2) {
                act(2, icf_vector, false);
            }
            count = 0;
            if (pwm) {
                act(1, 0, true); // BOTTOM: fast PWM outputs set (or cleared)
            }
            return;
        }
        if (v == max_) {
            act(2, tov_vector, false); // overflow
            count = 0;
            return;
        }
        count = static_cast<std::uint16_t>(v + 1);
    }

    void load_buffers()
    {
        for (int i = 0; i < channels; ++i) {
            ch[static_cast<std::size_t>(i)].ocr = ch[static_cast<std::size_t>(i)].buffer;
        }
    }

private:
    std::uint16_t max_;
};

// Clock of a timer: a prescaler dividing the CPU clock (its phase starts at
// `origin`, which a prescaler reset moves), or an external pin.
struct TickClock {
    unsigned divide = 0;      // 0: stopped
    bool external = false;
    bool rising = false;
    std::uint64_t* origin = nullptr;

    std::uint64_t ticks_until(std::uint64_t cycle) const
    {
        return cycle <= *origin ? 0 : (cycle - *origin) / divide;
    }
    std::uint64_t ticks_between(std::uint64_t after, std::uint64_t upto) const
    {
        if (divide == 0 || external || upto <= after) {
            return 0;
        }
        return ticks_until(upto) - ticks_until(after);
    }
    // Cycle of the k-th tick after `after` (k >= 1).
    std::uint64_t tick_cycle(std::uint64_t after, std::uint64_t k) const
    {
        const std::uint64_t base = after < *origin ? 0 : (after - *origin) / divide;
        return *origin + (base + k) * divide;
    }
};

// Common part of the three timers.
class TimerBase : public Atmega::Peripheral {
public:
    TimerBase(Atmega& m, int bits) : Atmega::Peripheral(m), core(bits) {}

    std::uint64_t next_event() const override
    {
        if (clock.divide == 0 || clock.external) {
            return no_event;
        }
        const std::uint64_t ticks = core.ticks_to_event();
        if (ticks == no_event) {
            return no_event;
        }
        return clock.tick_cycle(last_, ticks);
    }
    void on_time(std::uint64_t cycle) override { sync(cycle); }

    // Brings the counter to `cycle`, handling the events on the way.
    void sync(std::uint64_t cycle)
    {
        if (cycle <= last_) {
            return;
        }
        std::uint64_t ticks = clock.ticks_between(last_, cycle);
        std::uint64_t from = last_;
        while (ticks > 0) {
            const std::uint64_t to_event = core.ticks_to_event();
            if (to_event > ticks) {
                advance_plain(ticks);
                break;
            }
            advance_plain(to_event - 1);
            const std::uint64_t at = clock.tick_cycle(from, to_event);
            single_tick(at);
            from = at;
            ticks -= to_event;
        }
        last_ = cycle;
    }
    void external_edge(bool level, std::uint64_t cycle)
    {
        if (clock.external && level == clock.rising) {
            sync(cycle);
            single_tick(cycle);
        }
    }

    bool drives(int pin, bool& level) const override
    {
        for (int i = 0; i < core.channels; ++i) {
            const Channel& c = core.ch[static_cast<std::size_t>(i)];
            if (c.pin == pin && c.connected) {
                level = c.level;
                return true;
            }
        }
        return false;
    }

    TimerCore core;
    TickClock clock;

protected:
    void advance_plain(std::uint64_t ticks)
    {
        if (ticks == 0) {
            return;
        }
        core.blocked = false;
        core.count = static_cast<std::uint16_t>(core.down ? core.count - ticks : core.count + ticks);
    }
    void single_tick(std::uint64_t cycle)
    {
        bool outputs = false;
        core.tick([&](int event, int index, bool up) {
            if (event == 2) {
                if (index) {
                    m_.set_flag(index, true);
                }
                return;
            }
            if (event == 1) { // BOTTOM in fast PWM
                for (int i = 0; i < core.channels; ++i) {
                    Channel& c = core.ch[static_cast<std::size_t>(i)];
                    if (c.com == 2 || c.com == 3) {
                        outputs |= set_level(c, c.com == 2);
                    }
                }
                return;
            }
            Channel& c = core.ch[static_cast<std::size_t>(index)];
            m_.set_flag(c.vector, true);
            outputs |= compare_action(c, index, up);
        });
        if (outputs) {
            m_.refresh_pins_at(cycle);
        }
    }
    bool set_level(Channel& c, bool level)
    {
        const bool changed = c.level != level;
        c.level = level;
        return changed && c.connected;
    }
    // The output's action on a compare match (datasheet, Compare Output Mode tables).
    bool compare_action(Channel& c, int index, bool up)
    {
        if (c.com == 0) {
            return false;
        }
        if (!core.pwm) {
            return set_level(c, c.com == 1 ? !c.level : c.com == 3);
        }
        if (c.com == 1) {
            return index == 0 && core.toggle_a ? set_level(c, !c.level) : false;
        }
        if (core.shape == Shape::Up) { // fast PWM
            return set_level(c, c.com == 3);
        }
        // Phase correct: COM 10 clears counting up and sets counting down.
        return set_level(c, (c.com == 2) != up);
    }
    // Connects the outputs per COM and the mode.
    void connect_outputs()
    {
        for (int i = 0; i < core.channels; ++i) {
            Channel& c = core.ch[static_cast<std::size_t>(i)];
            c.connected = c.com != 0 && !(core.pwm && c.com == 1 && !(i == 0 && core.toggle_a));
        }
        m_.refresh_pins();
    }
    void write_count(std::uint16_t value)
    {
        sync(now());
        core.count = value;
        core.blocked = true;
        m_.timed_changed();
    }
    std::uint64_t last_ = 0;
};

// Timer/Counter0: 8 bits, counting up, overflow only (the ATmega8 has no
// compare unit on Timer0). Clock: the prescaler shared with Timer1, or T0.
class Atmega::Timer0 : public TimerBase {
public:
    explicit Timer0(Atmega& m) : TimerBase(m, 8)
    {
        core.tov_vector = V_T0_OVF;
        clock.origin = &m.prescaler10_;
    }
    std::vector<std::uint16_t> addresses() const override { return {TCCR0, TCNT0}; }
    std::uint8_t read(std::uint16_t address) override
    {
        if (address == TCCR0) {
            return tccr_;
        }
        sync(now());
        return static_cast<std::uint8_t>(core.count);
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        if (address == TCNT0) {
            write_count(value);
            return;
        }
        sync(now());
        tccr_ = value & 7;
        static const unsigned divide[8] = {0, 1, 8, 64, 256, 1024, 1, 1};
        clock.divide = divide[tccr_];
        clock.external = tccr_ >= 6;
        clock.rising = tccr_ == 7;
        m_.timed_changed();
    }
    void reset() override
    {
        tccr_ = 0;
        clock.divide = 0;
        clock.external = false;
        core.count = 0;
        core.down = false;
        core.fixed_top = 0xFF;
        last_ = now();
    }
    void pad_changed(int pin, bool level, std::uint64_t cycle) override
    {
        if (pin == PD4) {
            external_edge(level, cycle);
        }
    }

private:
    std::uint8_t tccr_ = 0;
};

// Timer/Counter1: 16 bits, sixteen waveform modes, two compare channels on
// OC1A (PB1) and OC1B (PB2), input capture on ICP1 (PB0), and the TEMP
// register for 16-bit access.
class Atmega::Timer1 : public TimerBase {
public:
    explicit Timer1(Atmega& m) : TimerBase(m, 16)
    {
        core.channels = 2;
        core.ch[0].pin = PB1;
        core.ch[0].vector = V_T1_COMPA;
        core.ch[1].pin = PB2;
        core.ch[1].vector = V_T1_COMPB;
        core.tov_vector = V_T1_OVF;
        core.icf_vector = V_T1_CAPT;
        clock.origin = &m.prescaler10_;
    }
    std::vector<std::uint16_t> addresses() const override
    {
        return {TCCR1A, TCCR1B, TCNT1H, TCNT1L, OCR1AH, OCR1AL, OCR1BH, OCR1BL, ICR1H, ICR1L};
    }
    std::uint8_t read(std::uint16_t address) override
    {
        switch (address) {
        case TCCR1A: return tccr1a_;
        case TCCR1B: return tccr1b_;
        case TCNT1L:
            sync(now());
            return low_and_temp(core.count);
        case OCR1AL: return low_and_temp(core.ch[0].buffer);
        case OCR1BL: return low_and_temp(core.ch[1].buffer);
        case ICR1L: return low_and_temp(core.icr);
        default: return temp_; // the high bytes read TEMP
        }
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        switch (address) {
        case TCCR1A:
            sync(now());
            tccr1a_ = value & 0xF3;
            if (value & 0x0C) { // FOC1A, FOC1B in non-PWM modes
                force(value);
            }
            configure();
            return;
        case TCCR1B:
            sync(now());
            tccr1b_ = value & 0xDF;
            configure();
            return;
        case TCNT1L: write_count(word(value)); return;
        case OCR1AL: write_ocr(0, word(value)); return;
        case OCR1BL: write_ocr(1, word(value)); return;
        case ICR1L:
            sync(now());
            core.icr = word(value);
            m_.timed_changed();
            return;
        default: temp_ = value; return; // the high bytes write TEMP
        }
    }
    void reset() override
    {
        tccr1a_ = tccr1b_ = 0;
        temp_ = 0;
        core.count = 0;
        core.down = false;
        core.icr = 0;
        for (auto& c : core.ch) {
            c.ocr = c.buffer = 0;
            c.com = 0;
            c.level = false;
            c.connected = false;
        }
        last_ = now();
        configure();
    }
    void pad_changed(int pin, bool level, std::uint64_t cycle) override
    {
        if (pin == PD5) {
            external_edge(level, cycle);
        } else if (pin == PB0 && !m_.ac_capture_) {
            capture(level, cycle);
        }
    }
    // An edge of the input capture's source: ICP1, or the analog comparator's
    // output with ACIC.
    void capture(bool level, std::uint64_t cycle)
    {
        if (level != bit(tccr1b_, 6) || core.top_source == 2) {
            return;
        }
        // The noise canceler delays it by four cycles.
        const std::uint64_t at = cycle + (bit(tccr1b_, 7) ? 4 : 0);
        sync(at);
        core.icr = core.count;
        m_.set_flag(V_T1_CAPT, true);
    }

private:
    std::uint8_t low_and_temp(std::uint16_t value)
    {
        temp_ = static_cast<std::uint8_t>(value >> 8);
        return static_cast<std::uint8_t>(value);
    }
    std::uint16_t word(std::uint8_t low) const { return static_cast<std::uint16_t>((temp_ << 8) | low); }
    void write_ocr(int n, std::uint16_t value)
    {
        sync(now());
        Channel& c = core.ch[static_cast<std::size_t>(n)];
        c.buffer = value;
        if (core.update == Update::Immediate) {
            c.ocr = value;
        }
        m_.timed_changed();
    }
    void force(std::uint8_t value)
    {
        if (core.pwm) {
            return;
        }
        bool changed = false;
        for (int i = 0; i < 2; ++i) {
            if (bit(value, 3 - i)) {
                changed |= compare_action(core.ch[static_cast<std::size_t>(i)], i, true);
            }
        }
        if (changed) {
            m_.refresh_pins();
        }
    }
    void configure()
    {
        const int wgm = (tccr1a_ & 3) | ((tccr1b_ >> 1) & 0x0C);
        core.ch[0].com = static_cast<std::uint8_t>(tccr1a_ >> 6);
        core.ch[1].com = static_cast<std::uint8_t>((tccr1a_ >> 4) & 3);
        static const std::uint16_t fixed[16] = {0xFFFF, 0xFF, 0x1FF, 0x3FF, 0, 0xFF, 0x1FF, 0x3FF, 0, 0, 0, 0, 0, 0, 0, 0};
        core.fixed_top = fixed[wgm];
        core.top_source = (wgm == 4 || wgm == 9 || wgm == 11 || wgm == 15) ? 1 : (wgm == 8 || wgm == 10 || wgm == 12 || wgm == 14) ? 2 : 0;
        const bool fast = wgm == 5 || wgm == 6 || wgm == 7 || wgm == 14 || wgm == 15;
        const bool phase = wgm == 1 || wgm == 2 || wgm == 3 || (wgm >= 8 && wgm <= 11);
        core.pwm = fast || phase;
        core.shape = phase ? Shape::UpDown : Shape::Up;
        core.clear_at_top = wgm == 4 || wgm == 12 || fast;
        core.tov_at_top = fast;
        core.update = fast || wgm == 1 || wgm == 2 || wgm == 3 || wgm == 10 || wgm == 11 ? Update::Top
                      : (wgm == 8 || wgm == 9)                                          ? Update::Bottom
                                                                                        : Update::Immediate;
        core.toggle_a = wgm == 9 || wgm == 11 || wgm == 14 || wgm == 15;
        if (core.update == Update::Immediate) {
            core.load_buffers();
        }
        if (wgm == 13) {
            m_.note_once("t1-mode13", "Timer1 mode 13 is reserved on the ATmega8");
        }
        static const unsigned divide[8] = {0, 1, 8, 64, 256, 1024, 1, 1};
        const int cs = tccr1b_ & 7;
        clock.divide = divide[cs];
        clock.external = cs >= 6;
        clock.rising = cs == 7;
        connect_outputs();
        m_.timed_changed();
    }
    std::uint8_t tccr1a_ = 0;
    std::uint8_t tccr1b_ = 0;
    std::uint8_t temp_ = 0;
};

// Timer/Counter2: 8 bits with one compare channel on OC2 (PB3), its own
// prescaler (1, 8, 32, 64, 128, 256, 1024). The asynchronous mode with a
// 32.768 kHz crystal comes in SN-259.
class Atmega::Timer2 : public TimerBase {
public:
    explicit Timer2(Atmega& m) : TimerBase(m, 8)
    {
        core.channels = 1;
        core.ch[0].pin = PB3;
        core.ch[0].vector = V_T2_COMP;
        core.tov_vector = V_T2_OVF;
        clock.origin = &m.prescaler2_;
    }
    std::vector<std::uint16_t> addresses() const override { return {TCCR2, TCNT2, OCR2, ASSR}; }
    std::uint8_t read(std::uint16_t address) override
    {
        switch (address) {
        case TCCR2: return tccr2_;
        case TCNT2: sync(now()); return static_cast<std::uint8_t>(core.count);
        case OCR2: return static_cast<std::uint8_t>(core.ch[0].buffer);
        default: return assr_;
        }
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        switch (address) {
        case TCCR2:
            sync(now());
            tccr2_ = value & 0x7F;
            if ((value & 0x80) && !core.pwm) { // FOC2
                if (compare_action(core.ch[0], 0, true)) {
                    m_.refresh_pins();
                }
            }
            configure();
            return;
        case TCNT2: write_count(value); return;
        case OCR2:
            sync(now());
            core.ch[0].buffer = value;
            if (core.update == Update::Immediate) {
                core.ch[0].ocr = value;
            }
            m_.timed_changed();
            return;
        default:
            assr_ = value & 0x08;
            if (assr_) {
                m_.note_once("t2-async", "Timer2's asynchronous mode (AS2) is not simulated yet; Timer2 keeps counting the CPU clock");
            }
            return;
        }
    }
    void reset() override
    {
        tccr2_ = 0;
        assr_ = 0;
        core.count = 0;
        core.down = false;
        core.ch[0].ocr = core.ch[0].buffer = 0;
        core.ch[0].level = false;
        last_ = now();
        configure();
    }

private:
    void configure()
    {
        const int wgm = (bit(tccr2_, 6) ? 1 : 0) | (bit(tccr2_, 3) ? 2 : 0);
        core.ch[0].com = static_cast<std::uint8_t>((tccr2_ >> 4) & 3);
        core.fixed_top = 0xFF;
        core.top_source = wgm == 2 ? 1 : 0;
        core.pwm = wgm == 1 || wgm == 3;
        core.shape = wgm == 1 ? Shape::UpDown : Shape::Up;
        core.clear_at_top = wgm == 2 || wgm == 3;
        core.tov_at_top = wgm == 3;
        core.update = core.pwm ? Update::Top : Update::Immediate;
        core.toggle_a = false;
        if (core.update == Update::Immediate) {
            core.load_buffers();
        }
        static const unsigned divide[8] = {0, 1, 8, 32, 64, 128, 256, 1024};
        clock.divide = divide[tccr2_ & 7];
        connect_outputs();
        m_.timed_changed();
    }
    std::uint8_t tccr2_ = 0;
    std::uint8_t assr_ = 0;
};

// ---------------------------------------------------------------------------
// USART: frames bit by bit on TXD (PD1); RXD (PD0) decoded from the pin, or
// bytes from the computer. UBRRH and UCSRC share an address (URSEL).

class Atmega::Usart : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {UDR, UCSRA, UCSRB, UBRRL, UBRRH_UCSRC}; }
    std::uint8_t read(std::uint16_t address) override
    {
        switch (address) {
        case UDR: {
            if (fifo_.empty()) {
                return 0;
            }
            const Frame f = fifo_.front();
            fifo_.erase(fifo_.begin());
            rxb8_ = f.ninth;
            update_flags();
            return f.value;
        }
        case UCSRA: return ucsra();
        case UCSRB: return static_cast<std::uint8_t>(ucsrb_ | (rxb8_ ? 0x02 : 0));
        case UBRRL: return static_cast<std::uint8_t>(ubrr_);
        default: {
            // Two reads in a row: UBRRH, then UCSRC.
            const bool second = last_read_ + 1 == now();
            last_read_ = now();
            return second ? static_cast<std::uint8_t>(ucsrc_ | 0x80) : static_cast<std::uint8_t>(ubrr_ >> 8);
        }
        }
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        switch (address) {
        case UDR:
            if (tx_full_) {
                return; // written while UDRE is 0: ignored
            }
            tx_buffer_ = value;
            tx_ninth_ = bit(ucsrb_, 0);
            tx_full_ = true;
            if (!tx_busy_) {
                start_frame(now());
            }
            update_flags();
            return;
        case UCSRA:
            u2x_ = bit(value, 1);
            if (bit(value, 6)) {
                txc_ = false; // writing 1 clears TXC
            }
            update_flags();
            return;
        case UCSRB: {
            const bool tx_was = bit(ucsrb_, 3);
            ucsrb_ = value & 0xFD;
            if (!bit(ucsrb_, 4)) {
                fifo_.clear();
                rx_active_ = false;
            }
            if (tx_was != bit(ucsrb_, 3)) {
                m_.refresh_pins();
            }
            update_flags();
            m_.timed_changed();
            return;
        }
        case UBRRL:
            ubrr_ = static_cast<std::uint16_t>((ubrr_ & 0x0F00) | value);
            return;
        default:
            if (bit(value, 7)) {
                ucsrc_ = value & 0x7F;
                if (bit(ucsrc_, 6)) {
                    m_.note_once("usart-sync", "The USART's synchronous mode (UMSEL) is not simulated; it runs asynchronous");
                }
            } else {
                ubrr_ = static_cast<std::uint16_t>((ubrr_ & 0x00FF) | ((value & 0x0F) << 8));
            }
            return;
        }
    }
    void reset() override
    {
        ucsrb_ = 0;
        ucsrc_ = 0x06; // 8 data bits
        ubrr_ = 0;
        u2x_ = false;
        txc_ = false;
        tx_full_ = tx_busy_ = false;
        tx_level_ = true;
        fifo_.clear();
        rx_active_ = false;
        host_.clear();
        host_next_ = 0;
        fe_ = dor_ = pe_ = false;
        update_flags();
    }

    std::uint64_t bit_cycles() const { return static_cast<std::uint64_t>(u2x_ ? 8 : 16) * (ubrr_ + 1u); }
    int data_bits() const
    {
        const int size = ((ucsrc_ >> 1) & 3) | (bit(ucsrb_, 2) ? 4 : 0);
        return size == 7 ? 9 : size + 5;
    }

    std::uint64_t next_event() const override
    {
        std::uint64_t next = no_event;
        if (tx_busy_) {
            next = std::min(next, tx_edges_[tx_index_]);
        }
        if (rx_active_) {
            next = std::min(next, rx_sample_);
        }
        if (!host_.empty() && bit(ucsrb_, 4)) {
            next = std::min(next, host_next_);
        }
        return next;
    }
    void on_time(std::uint64_t cycle) override
    {
        while (tx_busy_ && tx_edges_[tx_index_] <= cycle) {
            tx_step();
        }
        while (rx_active_ && rx_sample_ <= cycle) {
            rx_step(rx_sample_);
        }
        if (!host_.empty() && bit(ucsrb_, 4) && host_next_ <= cycle) {
            receive(host_.front(), false, false, false);
            host_.erase(host_.begin());
            host_next_ = cycle + frame_cycles();
            m_.timed_changed();
        }
    }
    bool drives(int pin, bool& level) const override
    {
        if (pin == PD1 && bit(ucsrb_, 3)) {
            level = tx_level_;
            return true;
        }
        return false;
    }
    bool takes_input(int pin) const override { return pin == PD0 && bit(ucsrb_, 4); }
    void pad_changed(int pin, bool level, std::uint64_t cycle) override
    {
        if (pin != PD0 || !bit(ucsrb_, 4) || !m_.uart_pin_rx_ || rx_active_ || level) {
            return;
        }
        // A start bit: sample its middle, then each bit's.
        rx_active_ = true;
        rx_bit_ = -1;
        rx_value_ = 0;
        rx_parity_ok_ = true;
        rx_sample_ = cycle + bit_cycles() / 2;
        m_.timed_changed();
    }
    void host_byte(std::uint8_t value)
    {
        if (host_.empty()) {
            host_next_ = std::max(now(), host_next_);
        }
        host_.push_back(value);
        m_.timed_changed();
    }

private:
    struct Frame {
        std::uint8_t value = 0;
        bool ninth = false;
    };
    std::uint64_t frame_cycles() const
    {
        const int parity = ((ucsrc_ >> 4) & 3) >= 2 ? 1 : 0;
        const int stop = bit(ucsrc_, 3) ? 2 : 1;
        return bit_cycles() * static_cast<std::uint64_t>(1 + data_bits() + parity + stop);
    }
    std::uint8_t ucsra() const
    {
        std::uint8_t v = 0;
        v |= fifo_.empty() ? 0 : 0x80;
        v |= txc_ ? 0x40 : 0;
        v |= tx_full_ ? 0 : 0x20;
        v |= fe_ ? 0x10 : 0;
        v |= dor_ ? 0x08 : 0;
        v |= pe_ ? 0x04 : 0;
        v |= u2x_ ? 0x02 : 0;
        return v;
    }
    void update_flags()
    {
        m_.flags_[V_RXC] = !fifo_.empty();
        m_.flags_[V_UDRE] = !tx_full_;
        m_.flags_[V_TXC] = txc_;
        m_.update_interrupts();
    }
    // Builds the frame's level changes: start bit, data LSB first, parity,
    // stop bits.
    void start_frame(std::uint64_t at)
    {
        tx_shift_ = tx_buffer_;
        tx_shift_ninth_ = tx_ninth_;
        tx_full_ = false;
        tx_busy_ = true;
        std::vector<bool> bits{false};
        int ones = 0;
        const int n = data_bits();
        for (int i = 0; i < n; ++i) {
            const bool b = i < 8 ? bit(tx_shift_, i) : tx_shift_ninth_;
            bits.push_back(b);
            ones += b ? 1 : 0;
        }
        const int upm = (ucsrc_ >> 4) & 3;
        if (upm >= 2) { // UPM 10: even parity, 11: odd
            bits.push_back(upm == 2 ? (ones & 1) != 0 : (ones & 1) == 0);
        }
        bits.push_back(true);
        if (bit(ucsrc_, 3)) {
            bits.push_back(true);
        }
        tx_bits_ = bits;
        tx_edges_.clear();
        for (std::size_t i = 0; i <= bits.size(); ++i) {
            tx_edges_.push_back(at + i * bit_cycles());
        }
        tx_index_ = 0;
        m_.timed_changed();
        update_flags();
    }
    void tx_step()
    {
        const std::uint64_t at = tx_edges_[tx_index_];
        if (tx_index_ < tx_bits_.size()) {
            if (tx_level_ != tx_bits_[tx_index_]) {
                tx_level_ = tx_bits_[tx_index_];
                if (bit(ucsrb_, 3)) {
                    m_.refresh_pins_at(at);
                }
            }
            ++tx_index_;
            return;
        }
        // The stop bits ended.
        m_.uart_byte(tx_shift_, at);
        tx_busy_ = false;
        if (tx_full_) {
            start_frame(at);
        } else {
            txc_ = true;
        }
        update_flags();
    }
    void rx_step(std::uint64_t at)
    {
        const bool level = m_.pad_level(PD0);
        const int n = data_bits();
        const int upm = (ucsrc_ >> 4) & 3;
        if (rx_bit_ < 0) {
            if (level) {
                rx_active_ = false; // a glitch, not a start bit
                return;
            }
        } else if (rx_bit_ < n) {
            if (level) {
                rx_value_ |= 1u << rx_bit_;
            }
        } else if (upm >= 2 && rx_bit_ == n) {
            int ones = level ? 1 : 0;
            for (int i = 0; i < n; ++i) {
                ones += (rx_value_ >> i) & 1u;
            }
            rx_parity_ok_ = upm == 2 ? (ones & 1) == 0 : (ones & 1) == 1;
        } else {
            receive(static_cast<std::uint8_t>(rx_value_), n == 9 && ((rx_value_ >> 8) & 1u), !level, !rx_parity_ok_);
            rx_active_ = false;
            return;
        }
        ++rx_bit_;
        rx_sample_ = at + bit_cycles();
    }
    void receive(std::uint8_t value, bool ninth, bool frame_error, bool parity_error)
    {
        if (fifo_.size() >= 2) {
            dor_ = true; // the shift register's byte is lost
            update_flags();
            return;
        }
        fifo_.push_back({value, ninth});
        fe_ = frame_error;
        pe_ = parity_error;
        dor_ = false;
        update_flags();
    }

    std::uint8_t ucsrb_ = 0;
    std::uint8_t ucsrc_ = 0x06;
    std::uint16_t ubrr_ = 0;
    bool u2x_ = false;
    bool txc_ = false;
    bool fe_ = false, dor_ = false, pe_ = false;
    bool rxb8_ = false;
    std::uint64_t last_read_ = 0;
    // Transmitter.
    std::uint8_t tx_buffer_ = 0;
    bool tx_ninth_ = false;
    bool tx_full_ = false;
    bool tx_busy_ = false;
    std::uint8_t tx_shift_ = 0;
    bool tx_shift_ninth_ = false;
    bool tx_level_ = true;
    std::vector<bool> tx_bits_;
    std::vector<std::uint64_t> tx_edges_;
    std::size_t tx_index_ = 0;
    // Receiver.
    std::vector<Frame> fifo_;
    bool rx_active_ = false;
    int rx_bit_ = -1;
    unsigned rx_value_ = 0;
    bool rx_parity_ok_ = true;
    std::uint64_t rx_sample_ = 0;
    std::vector<std::uint8_t> host_;
    std::uint64_t host_next_ = 0;
};

// ---------------------------------------------------------------------------
// EEPROM: 512 bytes, the EEMWE window of four cycles, the write time.

class Atmega::Eeprom : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {EECR, EEDR, EEARL, EEARH}; }
    std::uint8_t read(std::uint16_t address) override
    {
        switch (address) {
        case EECR: {
            std::uint8_t v = static_cast<std::uint8_t>(eecr_ & 0x08);
            v |= now() < mwe_until_ ? 0x04 : 0;
            v |= busy() ? 0x02 : 0;
            return v;
        }
        case EEDR: return eedr_;
        case EEARL: return static_cast<std::uint8_t>(eear_);
        default: return static_cast<std::uint8_t>(eear_ >> 8);
        }
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        switch (address) {
        case EECR:
            eecr_ = value & 0x08;
            if (bit(value, 2)) {
                mwe_until_ = now() + 4; // EEMWE: EEWE must follow within four cycles
            }
            if (bit(value, 1) && now() < mwe_until_ && !busy()) {
                pending_address_ = eear_ % eeprom_bytes;
                pending_value_ = eedr_;
                done_ = now() + m_.cycles_for_ns(eeprom_write_ns);
                m_.halt_cpu(2);
                m_.timed_changed();
            }
            if (bit(value, 0) && !busy()) {
                eedr_ = m_.eeprom_[eear_ % eeprom_bytes];
                m_.halt_cpu(4);
            }
            update();
            return;
        case EEDR: eedr_ = value; return;
        case EEARL: eear_ = static_cast<std::uint16_t>((eear_ & 0x100) | value); return;
        default: eear_ = static_cast<std::uint16_t>((eear_ & 0xFF) | ((value & 1) << 8)); return;
        }
    }
    void reset() override
    {
        eecr_ = 0;
        mwe_until_ = 0;
        update();
    }
    std::uint64_t next_event() const override { return busy() ? done_ : no_event; }
    void on_time(std::uint64_t cycle) override
    {
        if (busy() && done_ <= cycle) {
            m_.eeprom_[pending_address_] = pending_value_;
            ++m_.eeprom_writes_;
            done_ = 0;
            update();
        }
    }
    bool busy() const { return done_ != 0; }

private:
    void update()
    {
        m_.flags_[V_EE_RDY] = !busy(); // a level: pending while ready and enabled
        m_.update_interrupts();
    }
    std::uint8_t eecr_ = 0;
    std::uint8_t eedr_ = 0;
    std::uint16_t eear_ = 0;
    std::uint64_t mwe_until_ = 0;
    std::uint64_t done_ = 0;
    std::size_t pending_address_ = 0;
    std::uint8_t pending_value_ = 0;

public:
    bool ready_enabled() const { return bit(eecr_, 3); }
};

// ---------------------------------------------------------------------------
// Watchdog: 16K to 2048K cycles of its 1 MHz oscillator; disabling it or
// changing the period needs the WDCE sequence; WDTON keeps it on.

class Atmega::Watchdog : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {WDTCR}; }
    std::uint8_t read(std::uint16_t) override
    {
        return static_cast<std::uint8_t>((now() < wdce_until_ ? 0x10 : 0) | (wde_ ? 0x08 : 0) | wdp_);
    }
    void write(std::uint16_t, std::uint8_t value) override
    {
        const bool wdce = bit(value, 4);
        const bool wde = bit(value, 3);
        const auto wdp = static_cast<std::uint8_t>(value & 7);
        if (wdce && wde) {
            wdce_until_ = now() + 4;
            wde_ = true;
            restart();
            return;
        }
        if (now() < wdce_until_ || !enabled()) {
            wde_ = wde;
            wdp_ = wdp;
        } else if (wde) {
            wde_ = true; // enabling needs no sequence
        }
        restart();
    }
    void reset() override
    {
        wde_ = false;
        wdp_ = 0;
        wdce_until_ = 0;
        restart();
    }
    void wdr() { restart(); }
    bool enabled() const { return wde_ || !bit(m_.options_.fuse_high, 6); }
    std::uint64_t next_event() const override
    {
        return enabled() ? start_ + m_.cycles_for_ns((16'384ull << wdp_) * 1000) : no_event;
    }
    void on_time(std::uint64_t cycle) override
    {
        if (enabled() && next_event() <= cycle) {
            m_.request_reset(ResetKind::Watchdog);
        }
    }

private:
    void restart()
    {
        start_ = now();
        m_.timed_changed();
    }
    bool wde_ = false;
    std::uint8_t wdp_ = 0;
    std::uint64_t wdce_until_ = 0;
    std::uint64_t start_ = 0;
};

// ---------------------------------------------------------------------------
// ADC: 10 bits, ADC0..ADC5 on PC0..PC5 (ADC6/ADC7 are not on the PDIP), the
// 1.30 V bandgap and GND, the AVCC, internal 2.56 V or AREF reference;
// single conversions or free running; 13 ADC clocks (25 for the first after
// ADEN), the input sampled 1.5 ADC clocks after the start (13.5 for the
// first). The voltage is the circuit's at the sampling instant. AVCC and
// AREF are taken as 5 V (the supply is not read yet).

class Atmega::Adc : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {ADMUX, ADCSRA, ADCH, ADCL}; }
    std::uint8_t read(std::uint16_t address) override
    {
        switch (address) {
        case ADMUX: return admux_;
        case ADCSRA: return static_cast<std::uint8_t>(adcsra_ | (busy() ? 0x40 : 0) | (m_.flags_[V_ADC] ? 0x10 : 0));
        case ADCL:
            locked_ = true; // the data register keeps its value until ADCH is read
            return static_cast<std::uint8_t>(data_);
        default:
            locked_ = false;
            return static_cast<std::uint8_t>(data_ >> 8);
        }
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        if (address == ADMUX) {
            admux_ = value & 0xEF;
            if ((admux_ & 0x0F) == 6 || (admux_ & 0x0F) == 7) {
                m_.note_once("adc67", "ADC6 and ADC7 are not on the PDIP package; they read 0");
            } else if ((admux_ & 0xC0) == 0x00) {
                m_.note_once("aref", "The ADC's AREF reference is taken as 5 V (the AREF pin's voltage is not read yet)");
            } else if ((admux_ & 0xC0) == 0x80) {
                m_.note_once("refs10", "REFS 10 is reserved on the ATmega8; the ADC uses AREF");
            }
            return;
        }
        if (address != ADCSRA) {
            return;
        }
        const bool was_enabled = bit(adcsra_, 7);
        if (bit(value, 4)) {
            m_.set_flag(V_ADC, false); // writing 1 clears ADIF
        }
        adcsra_ = value & 0xAF; // ADSC and ADIF are kept apart
        if (!bit(adcsra_, 7)) {
            end_ = 0; // ADEN off stops a conversion
            first_ = true;
        } else if (!was_enabled) {
            first_ = true;
        }
        if (bit(value, 6) && bit(adcsra_, 7) && !busy()) {
            start(now());
        }
        m_.update_interrupts();
        m_.timed_changed();
    }
    void reset() override
    {
        admux_ = adcsra_ = 0;
        data_ = 0;
        locked_ = false;
        end_ = 0;
        first_ = true;
    }
    std::uint64_t next_event() const override { return busy() ? end_ : no_event; }
    void on_time(std::uint64_t cycle) override
    {
        if (!busy() || end_ > cycle) {
            return;
        }
        const std::uint64_t at = end_;
        end_ = 0;
        const unsigned result = convert(sample_);
        if (!locked_) {
            data_ = static_cast<std::uint16_t>(bit(admux_, 5) ? result << 6 : result);
        }
        m_.set_flag(V_ADC, true);
        if (bit(adcsra_, 5)) { // free running: the next conversion starts at once
            start(at);
        }
        m_.timed_changed();
    }
    bool interrupt_enabled() const { return bit(adcsra_, 3); }
    bool enabled() const { return bit(adcsra_, 7); }
    std::uint8_t mux() const { return admux_ & 0x0F; }

private:
    bool busy() const { return end_ != 0; }
    void start(std::uint64_t at)
    {
        static const unsigned divide[8] = {2, 2, 4, 8, 16, 32, 64, 128};
        const unsigned clock = divide[adcsra_ & 7];
        const unsigned clocks = first_ ? 25 : 13;
        // The sample: 1.5 ADC clocks after the start, 13.5 for the first.
        sample_ = at + (first_ ? 27 : 3) * clock / 2;
        end_ = at + static_cast<std::uint64_t>(clocks) * clock;
        sample_mux_ = admux_;
        first_ = false;
    }
    unsigned convert(std::uint64_t sample_cycle) const
    {
        const int channel = sample_mux_ & 0x0F;
        double volts = 0.0;
        if (channel <= 5) {
            volts = m_.analog_voltage(8 + channel, sample_cycle); // PC0..PC5
        } else if (channel == 14) {
            volts = 1.30; // the bandgap
        }
        const int refs = sample_mux_ >> 6;
        const double reference = refs == 3 ? 2.56 : 5.0;
        const double code = std::floor(volts * 1024.0 / reference);
        return static_cast<unsigned>(std::clamp(code, 0.0, 1023.0));
    }
    std::uint8_t admux_ = 0;
    std::uint8_t adcsra_ = 0;
    std::uint8_t sample_mux_ = 0;
    std::uint16_t data_ = 0;
    bool locked_ = false;
    bool first_ = true;
    std::uint64_t sample_ = 0;
    std::uint64_t end_ = 0;
};

// ---------------------------------------------------------------------------
// Analog comparator: AIN0 (PD6) or the 1.30 V bandgap against AIN1 (PD7) or,
// with ACME and the ADC off, an ADC input. The circuit is sampled every
// 10 us once the firmware has used ACSR (an approximation: an edge shorter
// than that can be missed); ACI on a toggle, falling or rising output;
// Timer1's input capture with ACIC.

class Atmega::AnalogComparator : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {ACSR}; }
    std::uint8_t read(std::uint16_t) override
    {
        activate();
        update(now());
        return static_cast<std::uint8_t>(acsr_ | (output_ ? 0x20 : 0) | (m_.flags_[V_ANA_COMP] ? 0x10 : 0));
    }
    void write(std::uint16_t, std::uint8_t value) override
    {
        activate();
        if (bit(value, 4)) {
            m_.set_flag(V_ANA_COMP, false); // writing 1 clears ACI
        }
        acsr_ = value & 0xCF;
        m_.ac_capture_ = bit(acsr_, 2);
        next_ = now();
        update(now());
        m_.update_interrupts();
        m_.timed_changed();
    }
    void reset() override
    {
        acsr_ = 0;
        output_ = false;
        active_ = false;
        next_ = 0;
        m_.ac_capture_ = false;
    }
    std::uint64_t next_event() const override { return active_ && !bit(acsr_, 7) ? next_ : no_event; }
    void on_time(std::uint64_t cycle) override
    {
        if (next_event() > cycle) {
            return;
        }
        update(cycle);
        next_ = cycle + std::max<std::uint64_t>(1, m_.cycles_for_ns(10'000));
    }
    bool interrupt_enabled() const { return bit(acsr_, 3); }

private:
    // The comparator runs from reset on the chip; here it is sampled once the
    // firmware uses it, starting from the output it has then (no edge).
    void activate()
    {
        if (!active_) {
            active_ = true;
            output_ = compare(now());
            next_ = now();
        }
    }
    bool compare(std::uint64_t cycle) const
    {
        const double plus = bit(acsr_, 6) ? 1.30 : m_.analog_voltage(22, cycle); // PD6
        int minus_pin = 23;                                                     // PD7
        if (bit(m_.sfior_, 3) && !m_.adc_->enabled()) {
            const int channel = m_.adc_->mux() & 7;
            minus_pin = channel <= 5 ? 8 + channel : -1;
        }
        const double minus = minus_pin >= 0 ? m_.analog_voltage(minus_pin, cycle) : 0.0;
        return plus > minus;
    }
    void update(std::uint64_t cycle)
    {
        if (bit(acsr_, 7)) {
            return; // ACD: off
        }
        const bool out = compare(cycle);
        if (out == output_) {
            return;
        }
        output_ = out;
        const int mode = acsr_ & 3;
        if (mode == 0 || (mode == 2 && !out) || (mode == 3 && out)) {
            m_.set_flag(V_ANA_COMP, true);
        }
        if (bit(acsr_, 2)) {
            m_.timer1_->capture(out, cycle);
        }
    }
    std::uint8_t acsr_ = 0;
    bool output_ = false;
    bool active_ = false;
    std::uint64_t next_ = 0;
};

// ---------------------------------------------------------------------------
// SPI master: SCK (PB5) at F_CPU / 4, 16, 64 or 128 (twice as fast with
// SPI2X), MOSI (PB3) and MISO (PB4) bit by bit on the pins in the four modes
// (CPOL, CPHA) and both bit orders (DORD); SPIF with its interrupt and WCOL.
// The firmware sets the DDR of MOSI and SCK as on the chip. SS (PB2) as an
// input taken low ends master mode (MSTR cleared, SPIF set). Slave mode is
// not simulated yet.

class Atmega::Spi : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {SPCR, SPSR, SPDR}; }
    std::uint8_t read(std::uint16_t address) override
    {
        if (address == SPCR) {
            return spcr_;
        }
        if (address == SPSR) {
            armed_ = spif() || wcol_;
            return static_cast<std::uint8_t>((spif() ? 0x80 : 0) | (wcol_ ? 0x40 : 0) | (spi2x_ ? 1 : 0));
        }
        clear_after_status();
        return received_;
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        if (address == SPCR) {
            const bool was = master();
            spcr_ = value;
            if (bit(spcr_, 6) && !bit(spcr_, 4)) {
                m_.note_once("spi-slave", "SPI slave mode is not simulated yet");
            }
            if (!master()) {
                end_ = 0;
            }
            sck_ = bit(spcr_, 3); // CPOL: the idle level
            check_ss();
            if (was != master()) {
                m_.refresh_pins();
            }
            m_.update_interrupts();
            return;
        }
        if (address == SPSR) {
            spi2x_ = bit(value, 0);
            return;
        }
        // SPDR
        clear_after_status();
        if (busy()) {
            wcol_ = true;
            return;
        }
        if (!master()) {
            received_ = value; // a slave's next byte (not shifted without a master)
            return;
        }
        start(value);
    }
    void reset() override
    {
        spcr_ = 0;
        spi2x_ = wcol_ = armed_ = false;
        received_ = 0;
        end_ = 0;
        sck_ = false;
        mosi_ = false;
        m_.flags_[V_SPI] = false;
    }
    std::uint64_t next_event() const override { return busy() ? next_ : no_event; }
    void on_time(std::uint64_t cycle) override
    {
        while (busy() && next_ <= cycle) {
            edge(next_);
        }
    }
    bool drives(int pin, bool& level) const override
    {
        if (!master()) {
            return false;
        }
        if (pin == PB5) {
            level = sck_;
            return true;
        }
        if (pin == PB3) {
            level = mosi_;
            return true;
        }
        return false;
    }
    bool takes_input(int pin) const override { return master() && pin == PB4; }
    void pad_changed(int pin, bool, std::uint64_t) override
    {
        if (pin == PB2) {
            check_ss();
        }
    }
    bool interrupt_enabled() const { return bit(spcr_, 7); }

private:
    bool master() const { return bit(spcr_, 6) && bit(spcr_, 4); }
    bool busy() const { return end_ != 0; }
    bool spif() const { return m_.flags_[V_SPI]; }
    void clear_after_status()
    {
        // SPIF and WCOL clear when SPDR is accessed after SPSR was read with them set.
        if (armed_) {
            m_.set_flag(V_SPI, false);
            wcol_ = false;
            armed_ = false;
        }
    }
    // SS as an input driven low: another master; this one becomes a slave.
    void check_ss()
    {
        if (master() && !bit(m_.ddr_[0], 2) && !m_.pad_level(PB2)) {
            spcr_ = static_cast<std::uint8_t>(spcr_ & ~0x10);
            end_ = 0;
            m_.set_flag(V_SPI, true);
            m_.refresh_pins();
        }
    }
    int bit_index(int n) const { return bit(spcr_, 5) ? n : 7 - n; } // DORD: LSB first
    void start(std::uint8_t value)
    {
        static const unsigned divide[4] = {4, 16, 64, 128};
        unsigned d = divide[spcr_ & 3];
        if (spi2x_) {
            d /= 2;
        }
        half_ = d / 2;
        out_ = value;
        in_ = 0;
        edge_ = 0;
        sck_ = bit(spcr_, 3);
        const std::uint64_t now_cycle = now();
        if (!bit(spcr_, 2)) { // CPHA 0: the first bit is on MOSI before the first edge
            mosi_ = bit(out_, bit_index(0));
        }
        next_ = now_cycle + half_;
        end_ = now_cycle + 16ull * half_;
        m_.refresh_pins();
        m_.timed_changed();
    }
    // The k-th of the 16 clock edges: leading edges (odd k) and trailing edges.
    void edge(std::uint64_t at)
    {
        ++edge_;
        sck_ = !sck_;
        const bool leading = (edge_ & 1) != 0;
        const int n = (edge_ - 1) / 2; // the bit of this clock period
        const bool cpha = bit(spcr_, 2);
        if (leading == !cpha) { // sample: leading with CPHA 0, trailing with CPHA 1
            if (m_.pad_level(PB4)) {
                in_ = static_cast<std::uint8_t>(in_ | (1u << bit_index(n)));
            }
        } else if (cpha) { // CPHA 1: the bit goes out on the leading edge
            mosi_ = bit(out_, bit_index(n));
        } else if (n + 1 < 8) { // CPHA 0: the next bit on the trailing edge
            mosi_ = bit(out_, bit_index(n + 1));
        }
        if (cpha && leading) {
            mosi_ = bit(out_, bit_index(n));
        }
        m_.refresh_pins_at(at);
        if (edge_ >= 16) {
            end_ = 0;
            received_ = in_;
            m_.set_flag(V_SPI, true);
            return;
        }
        next_ = at + half_;
    }
    std::uint8_t spcr_ = 0;
    bool spi2x_ = false;
    bool wcol_ = false;
    bool armed_ = false;
    std::uint8_t received_ = 0;
    std::uint8_t out_ = 0;
    std::uint8_t in_ = 0;
    bool sck_ = false;
    bool mosi_ = false;
    int edge_ = 0;
    unsigned half_ = 2;
    std::uint64_t next_ = 0;
    std::uint64_t end_ = 0;
};

// ---------------------------------------------------------------------------
// TWI (I2C) master: SDA (PC4) and SCL (PC5) open drain, SCL at
// F_CPU / (16 + 2 TWBR 4^TWPS), the START, repeated START, address, data and
// STOP steps with their TWSR status codes, ACK and NACK, clock stretching by
// a slave. TWINT is set after each step; the firmware clears it to start the
// next. Slave mode (TWAR) and arbitration are not simulated yet.

class Atmega::Twi : public Peripheral {
public:
    using Peripheral::Peripheral;
    std::vector<std::uint16_t> addresses() const override { return {TWBR, TWSR, TWAR, TWDR, TWCR}; }
    std::uint8_t read(std::uint16_t address) override
    {
        switch (address) {
        case TWBR: return twbr_;
        case TWSR: return static_cast<std::uint8_t>(status_ | twps_);
        case TWAR: return twar_;
        case TWDR: return twdr_;
        default:
            return static_cast<std::uint8_t>((twint() ? 0x80 : 0) | (twcr_ & 0x75) | (twwc_ ? 0x08 : 0));
        }
    }
    void write(std::uint16_t address, std::uint8_t value) override
    {
        switch (address) {
        case TWBR: twbr_ = value; return;
        case TWSR: twps_ = value & 3; return;
        case TWAR:
            twar_ = value;
            if (bit(twcr_, 2) && bit(twcr_, 6)) {
                m_.note_once("twi-slave", "TWI slave mode is not simulated yet");
            }
            return;
        case TWDR:
            if (twint()) {
                twdr_ = value;
            } else {
                twwc_ = true; // written while a step is running
            }
            return;
        default: break;
        }
        // TWCR
        const bool was_enabled = bit(twcr_, 2);
        twcr_ = static_cast<std::uint8_t>(value & 0x75); // TWEA, TWSTA, TWSTO, TWEN, TWIE
        if (!bit(twcr_, 2)) {
            steps_.clear();
            sda_low_ = scl_low_ = false;
            m_.set_flag(V_TWI, false);
            if (was_enabled) {
                m_.refresh_pins();
            }
            return;
        }
        if (!was_enabled) {
            m_.refresh_pins();
        }
        if (!bit(value, 7) || !steps_.empty()) {
            m_.update_interrupts();
            return; // TWINT not written: nothing starts
        }
        m_.set_flag(V_TWI, false); // writing 1 clears TWINT and starts the step
        twwc_ = false;
        plan();
        m_.update_interrupts();
    }
    void reset() override
    {
        twbr_ = 0;
        twps_ = 0;
        twar_ = 0xFE;
        twdr_ = 0xFF;
        twcr_ = 0;
        status_ = 0xF8;
        twwc_ = false;
        owner_ = false;
        receiving_ = false;
        steps_.clear();
        sda_low_ = scl_low_ = false;
        m_.flags_[V_TWI] = false;
    }
    std::uint64_t next_event() const override { return steps_.empty() ? no_event : next_; }
    void on_time(std::uint64_t cycle) override
    {
        while (!steps_.empty() && next_ <= cycle) {
            step(next_);
        }
    }
    bool drives(int pin, bool& level) const override
    {
        if (!bit(twcr_, 2)) {
            return false;
        }
        if ((pin == PC4 && sda_low_) || (pin == PC5 && scl_low_)) {
            level = false;
            return true;
        }
        return false;
    }
    bool takes_input(int pin) const override
    {
        return bit(twcr_, 2) && ((pin == PC4 && !sda_low_) || (pin == PC5 && !scl_low_));
    }
    bool interrupt_enabled() const { return bit(twcr_, 0); }

private:
    // One action on the lines, then a quarter of an SCL period: drive or
    // release SDA or SCL, sample SDA, or end the step with a status.
    enum class Op { SdaLow, SdaHigh, SclLow, SclHigh, Sample, Done };
    struct Step {
        Op op;
        int value = 0; // Done: the status; Sample: the bit number (8: ACK)
    };
    bool twint() const { return m_.flags_[V_TWI]; }
    std::uint64_t quarter() const
    {
        static const unsigned power[4] = {1, 4, 16, 64};
        const std::uint64_t period = 16 + 2ull * twbr_ * power[twps_];
        return std::max<std::uint64_t>(1, period / 4);
    }
    void plan()
    {
        steps_.clear();
        const bool start = bit(twcr_, 5);
        const bool stop = bit(twcr_, 4);
        if (stop) {
            // STOP: SDA low while SCL is low, SCL high, then SDA high.
            steps_.push_back({Op::SdaLow});
            steps_.push_back({Op::SclHigh});
            steps_.push_back({Op::SdaHigh});
            if (!start) {
                steps_.push_back({Op::Done, -1}); // no TWINT after a STOP
            }
            owner_ = false;
        }
        if (start) {
            // (Repeated) START: SDA high, SCL high, SDA low, SCL low.
            steps_.push_back({Op::SdaHigh});
            steps_.push_back({Op::SclHigh});
            steps_.push_back({Op::SdaLow});
            steps_.push_back({Op::SclLow});
            steps_.push_back({Op::Done, owner_ && !stop ? 0x10 : 0x08});
            owner_ = true;
            address_next_ = true;
        } else if (!stop) {
            byte();
        }
        next_ = now() + quarter();
        m_.timed_changed();
    }
    // A byte after START (the address), or data: eight bits and the ACK.
    void byte()
    {
        if (address_next_) {
            receiving_ = bit(twdr_, 0);
        }
        const bool reading = receiving_ && !address_next_;
        for (int b = 7; b >= 0; --b) {
            if (reading) {
                steps_.push_back({Op::SdaHigh});
            } else {
                steps_.push_back({bit(twdr_, b) ? Op::SdaHigh : Op::SdaLow});
            }
            steps_.push_back({Op::SclHigh});
            steps_.push_back({Op::Sample, b});
            steps_.push_back({Op::SclLow});
        }
        // ACK: the slave's after an address or written data; ours after a read.
        if (reading) {
            steps_.push_back({bit(twcr_, 6) ? Op::SdaLow : Op::SdaHigh});
        } else {
            steps_.push_back({Op::SdaHigh});
        }
        steps_.push_back({Op::SclHigh});
        steps_.push_back({Op::Sample, 8});
        steps_.push_back({Op::SclLow});
        steps_.push_back({Op::Done, 0}); // the status comes from the ACK
        rx_ = 0;
    }
    void step(std::uint64_t at)
    {
        const Step s = steps_.front();
        switch (s.op) {
        case Op::SdaLow: sda_low_ = true; break;
        case Op::SdaHigh: sda_low_ = false; break;
        case Op::SclLow: scl_low_ = true; break;
        case Op::SclHigh:
            scl_low_ = false;
            m_.refresh_pins_at(at);
            if (!m_.pad_level(PC5)) {
                next_ = at + quarter(); // a slave stretches the clock: wait for SCL
                return;
            }
            break;
        case Op::Sample:
            if (s.value == 8) {
                ack_ = !m_.pad_level(PC4);
            } else if (m_.pad_level(PC4)) {
                rx_ = static_cast<std::uint8_t>(rx_ | (1u << s.value));
            }
            break;
        case Op::Done: finish(s.value); break;
        }
        steps_.erase(steps_.begin());
        m_.refresh_pins_at(at);
        next_ = at + quarter();
    }
    void finish(int status)
    {
        twcr_ = static_cast<std::uint8_t>(twcr_ & ~0x30); // TWSTA and TWSTO done
        if (status < 0) {
            status_ = 0xF8;
            return; // after a STOP the bus is free; TWINT stays clear
        }
        if (status == 0) {
            const bool reading = receiving_ && !address_next_;
            if (reading) {
                twdr_ = rx_;
                status_ = bit(twcr_, 6) ? 0x50 : 0x58;
            } else if (address_next_) {
                status_ = receiving_ ? (ack_ ? 0x40 : 0x48) : (ack_ ? 0x18 : 0x20);
            } else {
                status_ = ack_ ? 0x28 : 0x30;
            }
            address_next_ = false;
        } else {
            status_ = static_cast<std::uint8_t>(status);
        }
        m_.set_flag(V_TWI, true);
    }
    std::uint8_t twbr_ = 0, twps_ = 0, twar_ = 0xFE, twdr_ = 0xFF, twcr_ = 0, status_ = 0xF8;
    bool twwc_ = false;
    bool owner_ = false;        // we sent a START and no STOP yet
    bool address_next_ = false; // the next byte is SLA+R/W
    bool receiving_ = false;
    bool ack_ = false;
    std::uint8_t rx_ = 0;
    bool sda_low_ = false, scl_low_ = false;
    std::vector<Step> steps_;
    std::uint64_t next_ = 0;
};

// ---------------------------------------------------------------------------
// Machine.

Atmega::Atmega(const Options& options)
    : options_(options),
      cpu_(AvrConfig{flash_bytes / 2, 0x460, 0x5F, 0x0000, false, false, 1}),
      eeprom_(eeprom_bytes, std::uint8_t{0xFF}),
      page_buffer_(page_words, std::uint16_t{0xFFFF})
{
    auto add = [this](auto* p) {
        peripherals_.emplace_back(p);
        for (const std::uint16_t a : p->addresses()) {
            io_owner_[a] = p;
        }
        return p;
    };
    add(new Ports(*this));
    extint_ = add(new ExternalInterrupts(*this));
    timer0_ = add(new Timer0(*this));
    timer1_ = add(new Timer1(*this));
    timer2_ = add(new Timer2(*this));
    usart_ = add(new Usart(*this));
    eeprom_block_ = add(new Eeprom(*this));
    watchdog_ = add(new Watchdog(*this));
    adc_ = add(new Adc(*this));
    add(new AnalogComparator(*this));
    spi_ = add(new Spi(*this));
    twi_ = add(new Twi(*this));
    attach_core();
    reset(ResetKind::PowerOn);
}

Atmega::Atmega() : Atmega(Options{}) {}

Atmega::~Atmega() = default;

const char* Atmega::chip_name() const { return options_.model == Model::Atmega8 ? "ATmega8" : "ATmega8A"; }

std::string Atmega::pin_name(int pin)
{
    const char port = static_cast<char>('B' + pin / 8);
    return std::string("P") + port + std::to_string(pin % 8);
}

bool Atmega::pin_exists(int pin) const { return pin >= 0 && pin < pin_count && pin != 15; } // no PC7

bool Atmega::pin_usable(int pin) const
{
    if (pin == PC6) {
        return !bit(options_.fuse_high, 7); // RSTDISBL programmed: an I/O pin
    }
    if (pin == PB6 || pin == PB7) {
        const int cksel = options_.fuse_low & 0x0F;
        if (cksel >= 1 && cksel <= 4) {
            return true; // internal RC: XTAL1/XTAL2 are I/O pins
        }
        if (pin == PB7 && (cksel == 0 || (cksel >= 5 && cksel <= 8))) {
            return true; // an external clock or RC uses XTAL1 only
        }
        return false;
    }
    return true;
}

void Atmega::attach_core()
{
    cpu_.read_io = [this](std::uint16_t a) { return read_io(a); };
    cpu_.write_io = [this](std::uint16_t a, std::uint8_t v) { write_io(a, v); };
    cpu_.pending_vector = [this] { return pending_vector(); };
    cpu_.interrupt_taken = [this](int v) { taken(v); };
    cpu_.on_sleep = [this] {
        if (!bit(mcucr_, 7)) {
            return false;
        }
        const int mode = (mcucr_ >> 4) & 7;
        if (mode != 0) {
            note_once("sleep" + std::to_string(mode),
                "Sleep modes other than Idle are not simulated yet (SN-259); this one behaves as Idle");
        }
        return true;
    };
    cpu_.on_wdr = [this] { watchdog_->wdr(); };
    cpu_.on_spm = [this](std::uint16_t z, std::uint16_t r1r0) { return spm(z, r1r0); };
    cpu_.lpm_read = [this](std::uint16_t z, std::uint8_t& value) { return lpm_special(z, value); };
    cpu_.diagnostic = [this](const std::string& text) { diagnostic(text); };
}

std::uint8_t Atmega::read_io(std::uint16_t address)
{
    if (Peripheral* p = io_owner_[address]) {
        return p->read(address);
    }
    switch (address) {
    case GICR: return gicr_;
    case GIFR: return static_cast<std::uint8_t>((flags_[V_INT1] ? 0x80 : 0) | (flags_[V_INT0] ? 0x40 : 0));
    case TIMSK: return timsk_;
    case TIFR: {
        timer0_->sync(cpu_.cycles());
        timer1_->sync(cpu_.cycles());
        timer2_->sync(cpu_.cycles());
        static const int order[8] = {V_T0_OVF, 0, V_T1_OVF, V_T1_COMPB, V_T1_COMPA, V_T1_CAPT, V_T2_OVF, V_T2_COMP};
        std::uint8_t v = 0;
        for (int b = 0; b < 8; ++b) {
            if (order[b] && flags_[static_cast<std::size_t>(order[b])]) {
                v = static_cast<std::uint8_t>(v | (1u << b));
            }
        }
        return v;
    }
    case MCUCR: return mcucr_;
    case MCUCSR: return mcucsr_;
    case SFIOR: return static_cast<std::uint8_t>(sfior_ & 0x0C);
    case OSCCAL: return osccal_;
    case SPMCR: {
        std::uint8_t v = static_cast<std::uint8_t>(spmcr_ & 0x80);
        if (cpu_.cycles() < spm_until_) {
            v |= 0x01;
        }
        v |= rww_busy_ ? 0x40 : 0;
        return v;
    }
    default: return 0;
    }
}

void Atmega::write_io(std::uint16_t address, std::uint8_t value)
{
    if (Peripheral* p = io_owner_[address]) {
        p->write(address, value);
        return;
    }
    switch (address) {
    case GICR:
        if (bit(value, 0)) { // IVCE: IVSEL may change within four cycles
            ivce_until_ = cpu_.cycles() + 4;
            gicr_ = static_cast<std::uint8_t>((value & 0xC0) | (gicr_ & 0x02) | 0x01);
        } else {
            if (cpu_.cycles() < ivce_until_) {
                gicr_ = static_cast<std::uint8_t>((gicr_ & ~0x02) | (value & 0x02));
                cpu_.set_vector_base(bit(gicr_, 1) ? boot_start_words() : 0);
            }
            gicr_ = static_cast<std::uint8_t>((value & 0xC0) | (gicr_ & 0x02));
        }
        update_interrupts();
        return;
    case GIFR:
        if (bit(value, 7)) {
            flags_[V_INT1] = false;
        }
        if (bit(value, 6)) {
            flags_[V_INT0] = false;
        }
        update_interrupts();
        return;
    case TIMSK:
        timsk_ = value & 0xFD;
        update_interrupts();
        return;
    case TIFR: {
        timer0_->sync(cpu_.cycles());
        timer1_->sync(cpu_.cycles());
        timer2_->sync(cpu_.cycles());
        static const int order[8] = {V_T0_OVF, 0, V_T1_OVF, V_T1_COMPB, V_T1_COMPA, V_T1_CAPT, V_T2_OVF, V_T2_COMP};
        for (int b = 0; b < 8; ++b) {
            if (order[b] && bit(value, b)) {
                flags_[static_cast<std::size_t>(order[b])] = false; // writing 1 clears a flag
            }
        }
        update_interrupts();
        return;
    }
    case MCUCR:
        mcucr_ = value;
        update_interrupts();
        return;
    case MCUCSR: mcucsr_ = static_cast<std::uint8_t>(mcucsr_ & value & 0x0F); return; // writing 0 clears a flag
    case SFIOR:
        if (bit(value, 0)) { // PSR10: restart the prescaler of Timer0 and Timer1
            timer0_->sync(cpu_.cycles());
            timer1_->sync(cpu_.cycles());
            prescaler10_ = cpu_.cycles();
            timed_changed();
        }
        if (bit(value, 1)) { // PSR2
            timer2_->sync(cpu_.cycles());
            prescaler2_ = cpu_.cycles();
            timed_changed();
        }
        sfior_ = static_cast<std::uint8_t>(value & 0x0C);
        refresh_pins(); // PUD
        return;
    case OSCCAL:
        osccal_ = value;
        note_once("osccal", "OSCCAL changes are not simulated: the internal oscillator keeps its nominal frequency");
        return;
    case SPMCR:
        spmcr_ = value;
        spm_window_ = cpu_.cycles() + 4;
        return;
    default: return;
    }
}

bool Atmega::interrupt_enabled(int vector) const
{
    switch (vector) {
    case V_INT0: return bit(gicr_, 6);
    case V_INT1: return bit(gicr_, 7);
    case V_T2_COMP: return bit(timsk_, 7);
    case V_T2_OVF: return bit(timsk_, 6);
    case V_T1_CAPT: return bit(timsk_, 5);
    case V_T1_COMPA: return bit(timsk_, 4);
    case V_T1_COMPB: return bit(timsk_, 3);
    case V_T1_OVF: return bit(timsk_, 2);
    case V_T0_OVF: return bit(timsk_, 0);
    case V_RXC: return bit(usart_->read(UCSRB), 7);
    case V_UDRE: return bit(usart_->read(UCSRB), 5);
    case V_TXC: return bit(usart_->read(UCSRB), 6);
    case V_ADC: return adc_->interrupt_enabled();
    case V_SPI: return spi_->interrupt_enabled();
    case V_TWI: return twi_->interrupt_enabled();
    case V_ANA_COMP: return io_owner_[ACSR] && static_cast<AnalogComparator*>(io_owner_[ACSR])->interrupt_enabled();
    case V_EE_RDY: return eeprom_block_->ready_enabled();
    case V_SPM_RDY: return bit(spmcr_, 7);
    default: return false;
    }
}

int Atmega::pending_vector()
{
    for (int v = 1; v < V_COUNT; ++v) {
        if (!interrupt_enabled(v)) {
            continue;
        }
        if ((v == V_INT0 || v == V_INT1) && extint_->level_pending(v - 1)) {
            return v;
        }
        if (flags_[static_cast<std::size_t>(v)] && (v != V_SPM_RDY || cpu_.cycles() >= spm_until_)) {
            return v;
        }
    }
    return -1;
}

void Atmega::taken(int vector)
{
    // The hardware clears these flags when their vector runs; RXC, UDRE and
    // EE_RDY are levels that the firmware clears.
    switch (vector) {
    case V_INT0:
    case V_INT1:
    case V_T2_COMP:
    case V_T2_OVF:
    case V_T1_CAPT:
    case V_T1_COMPA:
    case V_T1_COMPB:
    case V_T1_OVF:
    case V_T0_OVF:
    case V_SPI:
    case V_ADC:
    case V_ANA_COMP:
    case V_TXC:
        flags_[static_cast<std::size_t>(vector)] = false;
        if (vector == V_TXC) {
            usart_->write(UCSRA, static_cast<std::uint8_t>(usart_->read(UCSRA) | 0x40));
        }
        break;
    default: break;
    }
    update_interrupts();
}

void Atmega::set_flag(int vector, bool on)
{
    flags_[static_cast<std::size_t>(vector)] = on;
    update_interrupts();
}

void Atmega::update_interrupts()
{
    bool any = false;
    for (int v = 1; v < V_COUNT && !any; ++v) {
        if (!interrupt_enabled(v)) {
            continue;
        }
        any = flags_[static_cast<std::size_t>(v)] || ((v == V_INT0 || v == V_INT1) && extint_->level_pending(v - 1));
    }
    cpu_.set_interrupt_pending(any);
    if (any) {
        cpu_.request_stop(); // the run loop wakes a sleeping core
    }
}

void Atmega::diagnostic(const std::string& text) { diagnostics_.push_back(text); }

void Atmega::note_once(const std::string& key, const std::string& text)
{
    if (std::find(noted_.begin(), noted_.end(), key) == noted_.end()) {
        noted_.push_back(key);
        diagnostic(text);
    }
}

std::vector<std::string> Atmega::take_diagnostics() { return std::exchange(diagnostics_, {}); }

// ---- Clock and fuses ----

std::uint64_t Atmega::ns_at(std::uint64_t cycle) const
{
    if (cpu_hz_ == 0) {
        return idle_ns_;
    }
    return cycle / cpu_hz_ * 1'000'000'000ull + cycle % cpu_hz_ * 1'000'000'000ull / cpu_hz_;
}

std::uint64_t Atmega::cycles_at(std::uint64_t ns) const
{
    if (cpu_hz_ == 0) {
        return 0;
    }
    const std::uint64_t whole = ns / 1'000'000'000ull * cpu_hz_;
    const std::uint64_t rest = ns % 1'000'000'000ull * cpu_hz_;
    return whole + (rest + 999'999'999ull) / 1'000'000'000ull;
}

std::uint64_t Atmega::cycles_for_ns(std::uint64_t ns) const { return cycles_at(ns); }

void Atmega::apply_fuses()
{
    const int cksel = options_.fuse_low & 0x0F;
    const int sut = (options_.fuse_low >> 4) & 3;
    std::uint64_t clocks = 6;
    std::uint64_t delay_ns = 0;
    static const std::uint64_t tout[3] = {0, 4'100'000, 65'000'000};
    if (cksel >= 1 && cksel <= 4) {
        static const std::uint32_t rc[5] = {0, 1'000'000, 2'000'000, 4'000'000, 8'000'000};
        cpu_hz_ = rc[cksel];
        delay_ns = sut < 3 ? tout[sut] : 0;
    } else if (cksel == 0) { // external clock on XTAL1
        cpu_hz_ = options_.external_hz;
        delay_ns = sut < 3 ? tout[sut] : 0;
    } else if (cksel <= 8) { // external RC oscillator
        cpu_hz_ = options_.external_hz;
        clocks = sut == 3 ? 6 : 18;
        delay_ns = sut == 3 ? tout[1] : tout[sut];
    } else if (cksel == 9) { // low-frequency crystal
        cpu_hz_ = options_.external_hz;
        clocks = sut == 2 ? 32768 : 1024;
        delay_ns = sut == 0 ? tout[1] : tout[2];
    } else { // crystal or ceramic resonator
        cpu_hz_ = options_.external_hz;
        static const std::uint64_t ck[8] = {258, 258, 1024, 1024, 1024, 16384, 16384, 16384};
        static const std::uint64_t ms[8] = {4'100'000, 65'000'000, 0, 4'100'000, 65'000'000, 0, 4'100'000, 65'000'000};
        const int row = ((cksel & 1) << 2) | sut;
        clocks = ck[row];
        delay_ns = ms[row];
        if (cpu_hz_ > 8'000'000 && bit(options_.fuse_high, 4)) {
            note_once("ckopt", "The crystal is above 8 MHz but CKOPT is not programmed: the oscillator may not run reliably on a real chip");
        }
    }
    startup_cycles_ = cpu_hz_ ? clocks + cycles_at(delay_ns) : 0;
    cpu_.set_reset_pc(bit(options_.fuse_high, 0) ? 0 : boot_start_words());
}

std::string Atmega::clock_description() const
{
    const int cksel = options_.fuse_low & 0x0F;
    char text[160];
    if (cksel >= 1 && cksel <= 4) {
        std::snprintf(text, sizeof text, "internal RC oscillator, %u MHz (nominal; the chip's varies a few percent)",
            static_cast<unsigned>(cpu_hz_ / 1'000'000));
    } else if (cpu_hz_ == 0) {
        std::snprintf(text, sizeof text, "%s selected by the fuses, but nothing is connected to XTAL1/XTAL2: the chip does not start",
            cksel == 0 ? "an external clock" : cksel <= 8 ? "an external RC oscillator" : "a crystal");
    } else {
        std::snprintf(text, sizeof text, "%s at %.6g MHz", cksel == 0 ? "external clock" : cksel <= 8 ? "external RC oscillator" : "crystal",
            cpu_hz_ / 1e6);
    }
    return text;
}

std::uint32_t Atmega::boot_start_words() const
{
    static const std::uint32_t size[4] = {1024, 512, 256, 128}; // BOOTSZ 00, 01, 10, 11
    return flash_bytes / 2 - size[(options_.fuse_high >> 1) & 3];
}

void Atmega::set_fuses(std::uint8_t low, std::uint8_t high, std::uint8_t lock)
{
    options_.fuse_low = low;
    options_.fuse_high = high;
    options_.lock = lock;
}

// ---- Memories and firmware ----

bool Atmega::load_flash(const std::vector<std::uint8_t>& bytes, std::string& error, std::uint32_t at)
{
    if (at + bytes.size() > flash_bytes) {
        error = std::string("The firmware needs ") + std::to_string(at + bytes.size()) + " bytes of flash; the " + chip_name()
                + " has 8192.";
        return false;
    }

    std::vector<std::uint16_t> current(flash_bytes / 2);
    for (std::uint32_t w = 0; w < flash_bytes / 2; ++w) {
        current[w] = cpu_.flash_word(w);
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const std::uint32_t a = at + static_cast<std::uint32_t>(i);
        std::uint16_t& w = current[a / 2];
        w = static_cast<std::uint16_t>((a & 1) ? ((w & 0x00FF) | (bytes[i] << 8)) : ((w & 0xFF00) | bytes[i]));
    }
    cpu_.load_flash(current);
    return true;
}

std::uint8_t Atmega::flash_byte(std::uint32_t address) const { return cpu_.flash_byte(address); }

void Atmega::chip_erase()
{
    cpu_.load_flash(std::vector<std::uint16_t>(flash_bytes / 2, 0xFFFF));
    if (bit(options_.fuse_high, 3)) { // EESAVE unprogrammed: the EEPROM is erased too
        std::fill(eeprom_.begin(), eeprom_.end(), std::uint8_t{0xFF});
    }
    options_.lock = 0xFF;
}

void Atmega::set_eeprom(const std::vector<std::uint8_t>& bytes)
{
    for (std::size_t i = 0; i < eeprom_.size(); ++i) {
        eeprom_[i] = i < bytes.size() ? bytes[i] : 0xFF;
    }
}

bool Atmega::load_hex(const std::string& text, std::string& error, bool eeprom)
{
    std::istringstream in(text);
    std::string line;
    std::uint32_t base = 0;
    int number = 0;
    std::vector<std::pair<std::uint32_t, std::uint8_t>> bytes;
    auto hex = [](const std::string& s, std::size_t at, std::size_t n, unsigned& value) {
        value = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const char c = at + i < s.size() ? s[at + i] : 'x';
            const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0) {
                return false;
            }
            value = value * 16 + static_cast<unsigned>(d);
        }
        return true;
    };
    bool ended = false;
    while (std::getline(in, line) && !ended) {
        ++number;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        unsigned count = 0, address = 0, type = 0;
        if (line[0] != ':' || !hex(line, 1, 2, count) || !hex(line, 3, 4, address) || !hex(line, 7, 2, type)
            || line.size() < 11 + 2 * count) {
            error = "Line " + std::to_string(number) + " of the HEX file is not an Intel HEX record.";
            return false;
        }
        unsigned sum = count + (address >> 8) + (address & 0xFF) + type;
        std::vector<std::uint8_t> data(count);
        for (unsigned i = 0; i < count; ++i) {
            unsigned b = 0;
            hex(line, 9 + 2 * i, 2, b);
            data[i] = static_cast<std::uint8_t>(b);
            sum += b;
        }
        unsigned check = 0;
        hex(line, 9 + 2 * count, 2, check);
        if (((sum + check) & 0xFF) != 0) {
            error = "Line " + std::to_string(number) + " of the HEX file has a wrong checksum.";
            return false;
        }
        switch (type) {
        case 0:
            for (unsigned i = 0; i < count; ++i) {
                bytes.emplace_back(base + address + i, data[i]);
            }
            break;
        case 1: ended = true; break;
        case 2: base = ((static_cast<std::uint32_t>(data.size() > 1 ? data[0] : 0) << 8) | (data.size() > 1 ? data[1] : 0)) << 4; break;
        case 4: base = ((static_cast<std::uint32_t>(data.size() > 1 ? data[0] : 0) << 8) | (data.size() > 1 ? data[1] : 0)) << 16; break;
        default: break; // start addresses
        }
    }
    if (bytes.empty()) {
        error = "The HEX file has no data.";
        return false;
    }
    const std::uint32_t limit = eeprom ? static_cast<std::uint32_t>(eeprom_bytes) : flash_bytes;
    for (const auto& [address, value] : bytes) {
        if (address >= limit) {
            error = std::string("The HEX file has data at 0x") + [&] {
                char t[16];
                std::snprintf(t, sizeof t, "%X", static_cast<unsigned>(address));
                return std::string(t);
            }() + ", beyond the " + chip_name() + "'s " + (eeprom ? "512 bytes of EEPROM." : "8 KB of flash.");
            return false;
        }
    }
    if (eeprom) {
        for (const auto& [address, value] : bytes) {
            eeprom_[address] = value;
        }
        return true;
    }
    std::vector<std::uint8_t> image(flash_bytes, std::uint8_t{0xFF});
    for (const auto& [address, value] : bytes) {
        image[address] = value;
    }
    return load_flash(image, error);
}

bool Atmega::load_elf(const ElfImage& image, std::string& error, bool* fuses_set)
{
    std::vector<std::uint8_t> flash(flash_bytes, std::uint8_t{0xFF});
    bool any = false;
    std::optional<std::uint8_t> low, high, lock;
    for (const ElfSegment& s : image.segments) {
        const std::uint32_t a = s.physical;
        if (a < 0x800000) { // flash
            if (a + s.data.size() > flash_bytes) {
                error = std::string("The firmware needs ") + std::to_string(a + s.data.size()) + " bytes of flash; the "
                        + chip_name() + " has 8192.";
                return false;
            }
            std::copy(s.data.begin(), s.data.end(), flash.begin() + a);
            any = any || !s.data.empty();
        }
    }
    for (const ElfSection& s : image.sections) {
        if (s.address >= 0x810000 && s.address < 0x820000) { // .eeprom
            for (std::size_t i = 0; i < s.data.size() && s.address - 0x810000 + i < eeprom_.size(); ++i) {
                eeprom_[s.address - 0x810000 + i] = s.data[i];
            }
        } else if (s.address == 0x820000) { // .fuse: low, high (extended on larger chips)
            if (!s.data.empty()) {
                low = s.data[0];
            }
            if (s.data.size() > 1) {
                high = s.data[1];
            }
        } else if (s.address == 0x830000 && !s.data.empty()) { // .lock
            lock = s.data[0];
        }
    }
    if (!any) {
        error = "The ELF file has nothing to load into the flash.";
        return false;
    }
    if (!load_flash(flash, error)) {
        return false;
    }
    const bool changed = (low && *low != options_.fuse_low) || (high && *high != options_.fuse_high)
                         || (lock && *lock != options_.lock);
    if (low) {
        options_.fuse_low = *low;
    }
    if (high) {
        options_.fuse_high = *high;
    }
    if (lock) {
        options_.lock = *lock;
    }
    if (fuses_set) {
        *fuses_set = changed;
    }
    return true;
}

// ---- Reset and running ----

void Atmega::reset(ResetKind kind)
{
    apply_fuses();
    switch (kind) {
    case ResetKind::PowerOn: mcucsr_ = 0x01; break;
    case ResetKind::External: mcucsr_ |= 0x02; break;
    case ResetKind::BrownOut: mcucsr_ |= 0x04; break;
    case ResetKind::Watchdog: mcucsr_ |= 0x08; break;
    }
    // The start-up time passes before the first instruction (a second reset
    // at the same moment does not add to it).
    run_from_ = cpu_.cycles() + startup_cycles_;
    cpu_.reset();
    cpu_.set_vector_base(0);
    flags_.fill(false);
    port_.fill(0);
    ddr_.fill(0);
    gicr_ = mcucr_ = sfior_ = timsk_ = spmcr_ = 0;
    osccal_ = 0xA8; // the 1 MHz calibration byte differs per chip; a typical value
    ivce_until_ = spm_until_ = spm_window_ = blbset_until_ = 0;
    rww_busy_ = false;
    prescaler10_ = prescaler2_ = cpu_.cycles();
    for (auto& p : peripherals_) {
        p->reset();
    }
    reset_requested_ = false;
    refresh_pins();
    update_interrupts();
    if (cpu_hz_ == 0) {
        note_once("no-clock", std::string("The ") + chip_name() + " does not start: " + clock_description() + ".");
    }
}

void Atmega::request_reset(ResetKind kind)
{
    reset_requested_ = true;
    requested_kind_ = kind;
    cpu_.request_stop();
}

std::uint64_t Atmega::next_event() const
{
    std::uint64_t next = no_event;
    for (const auto& p : peripherals_) {
        next = std::min(next, p->next_event());
    }
    for (const TimedDevice* d : external_) {
        next = std::min(next, d->next_event());
    }
    return next;
}

void Atmega::run_events(std::uint64_t cycle)
{
    // Repeat while events are due: one may schedule another at the same cycle.
    for (int guard = 0; guard < 64; ++guard) {
        bool any = false;
        for (auto& p : peripherals_) {
            if (p->next_event() <= cycle) {
                p->on_time(cycle);
                any = true;
            }
        }
        for (TimedDevice* d : external_) {
            if (d->next_event() <= cycle) {
                d->on_time(cycle);
                any = true;
            }
        }
        if (!any) {
            return;
        }
    }
}

void Atmega::run_until_ns(std::uint64_t ns)
{
    if (cpu_hz_ == 0) {
        idle_ns_ = std::max(idle_ns_, ns);
        return;
    }
    const std::uint64_t limit = cycles_at(ns);
    while (cpu_.cycles() < limit) {
        if (reset_requested_) {
            reset(requested_kind_);
            continue;
        }
        const std::uint64_t event = next_event();
        const std::uint64_t stop = std::min(limit, event);
        if (reset_pin_low_ || cpu_.cycles() < run_from_) {
            // Held in reset, or starting up: time passes without instructions.
            cpu_.set_cycles(reset_pin_low_ ? stop : std::min(stop, run_from_));
        } else if (cpu_.cycles() < stop) {
            cpu_.run(stop); // asleep, the core moves time to `stop`; an interrupt wakes it
        }
        run_events(cpu_.cycles());
    }
}

void Atmega::step_instruction()
{
    if (reset_requested_) {
        reset(requested_kind_);
    }
    if (cpu_.cycles() < run_from_) {
        cpu_.set_cycles(run_from_);
    }
    cpu_.step_instruction();
    run_events(cpu_.cycles());
}

// ---- Pins ----

void Atmega::refresh_pins() { refresh_pins_at(cpu_.cycles()); }

void Atmega::refresh_pins_at(std::uint64_t cycle)
{
    const bool pud = bit(sfior_, 2);
    for (int p = 0; p < pin_count; ++p) {
        if (!pin_exists(p)) {
            continue;
        }
        PinState s;
        if (p == PC6 && !pin_usable(p)) {
            s.pull_up = true; // RESET has its own pull-up (30-80 kOhm)
        } else if (pin_usable(p)) {
            const int port = p / 8;
            const int b = p % 8;
            bool level = false;
            const bool ddr = bit(ddr_[static_cast<std::size_t>(port)], b);
            const bool out = bit(port_[static_cast<std::size_t>(port)], b);
            bool overridden = false;
            for (const auto& per : peripherals_) {
                if (per->takes_input(p)) {
                    s.pull_up = out && !pud;
                    overridden = true;
                    break;
                }
                if (per->drives(p, level)) {
                    // The USART's TXD drives the pin by itself; output
                    // compare outputs need their DDR bit.
                    // TWI's SDA and SCL drive their pins whatever the DDR.
                    if (p == PD1 || p == PC4 || p == PC5 || ddr) {
                        s.output = true;
                        s.level = level;
                        overridden = true;
                        break;
                    }
                }
            }
            if (!overridden) {
                if (ddr) {
                    s.output = true;
                    s.level = out;
                } else {
                    s.pull_up = out && !pud;
                }
            }
        }
        const PinState before = driven_[static_cast<std::size_t>(p)];
        const bool level_before = level_[static_cast<std::size_t>(p)];
        driven_[static_cast<std::size_t>(p)] = s;
        const bool level_after = pad_level(p);
        level_[static_cast<std::size_t>(p)] = level_after;
        if (before.output != s.output || before.level != s.level || before.pull_up != s.pull_up) {
            pin_events_.push_back({ns_at(cycle), p, s});
            if (pin_hook_) {
                pin_hook_(p, ns_at(cycle));
            }
        }
        if (level_before != level_after) {
            pin_changed(p, level_before, level_after);
        }
    }
}

bool Atmega::pad_level(int pin) const
{
    if (!pin_exists(pin)) {
        return false;
    }
    if (pin == PC6 && !pin_usable(pin)) {
        return !reset_pin_low_; // RESET: its pull-up holds it high
    }
    const PinState& s = driven_[static_cast<std::size_t>(pin)];
    if (s.output) {
        return s.level;
    }
    const auto& in = input_[static_cast<std::size_t>(pin)];
    return in ? *in : s.pull_up;
}

Atmega::PinState Atmega::pin(int pin) const
{
    return pin_exists(pin) ? driven_[static_cast<std::size_t>(pin)] : PinState{};
}

std::vector<Atmega::PinEvent> Atmega::take_pin_events() { return std::exchange(pin_events_, {}); }

void Atmega::set_pin_input(int pin, std::optional<bool> level)
{
    if (!pin_exists(pin)) {
        return;
    }
    if (pin == PC6 && !pin_usable(pin)) {
        const bool low = level && !*level;
        if (low != reset_pin_low_) {
            reset_pin_low_ = low;
            if (!low) {
                request_reset(ResetKind::External); // released: the chip starts again
            }
        }
        return;
    }
    input_[static_cast<std::size_t>(pin)] = level;
    const bool before = level_[static_cast<std::size_t>(pin)];
    const bool after = pad_level(pin);
    level_[static_cast<std::size_t>(pin)] = after;
    if (before != after) {
        pin_changed(pin, before, after);
    }
}

void Atmega::pin_changed(int pin, bool, bool after)
{
    const std::uint64_t cycle = cpu_.cycles();
    for (auto& p : peripherals_) {
        p->pad_changed(pin, after, cycle);
    }
}

void Atmega::output_changed() { refresh_pins(); }

double Atmega::analog_voltage(int pin, std::uint64_t cycle) const
{
    if (analog_source_) {
        if (const auto v = analog_source_(pin, ns_at(cycle)); v && !std::isnan(*v)) {
            return *v;
        }
    }
    return pad_level(pin) ? 5.0 : 0.0; // without the circuit: the pin's logic level
}

// ---- USART bytes ----

void Atmega::uart_byte(std::uint8_t value, std::uint64_t cycle) { uart_bytes_.push_back({ns_at(cycle), value}); }

std::vector<Atmega::UartByte> Atmega::take_uart_bytes() { return std::exchange(uart_bytes_, {}); }

void Atmega::uart_receive(std::uint8_t byte) { usart_->host_byte(byte); }

// ---- Self-programming (SPM) and fuse reads (LPM) ----

unsigned Atmega::spm(std::uint16_t z, std::uint16_t r1r0)
{
    const std::uint64_t now = cpu_.cycles();
    if (now >= spm_window_ || !bit(spmcr_, 0)) {
        return 0; // SPMEN must be written in the four cycles before
    }
    const std::uint32_t boot = boot_start_words();
    if (cpu_.pc() < boot) {
        note_once("spm-app", "SPM runs only from the boot loader section; this one, from the application section, did nothing");
        return 0;
    }
    const std::uint32_t page = (z >> 1) & ~(page_words - 1) & (flash_bytes / 2 - 1);
    const bool to_boot = page >= boot;
    const std::uint8_t lock = options_.lock;
    const bool refused = to_boot ? !bit(lock, 4) : !bit(lock, 2); // BLB11 / BLB01 programmed
    const std::uint8_t op = spmcr_ & 0x1E;
    spmcr_ &= 0x80;
    if (op == 0x00) { // fill the page buffer
        page_buffer_[(z >> 1) & (page_words - 1)] = r1r0;
        return 0;
    }
    if (op == 0x10) { // RWWSRE: the RWW section reads again
        rww_busy_ = false;
        return 0;
    }
    if (op == 0x08) { // BLBSET: write the boot lock bits from r0
        options_.lock = static_cast<std::uint8_t>(options_.lock & (static_cast<std::uint8_t>(r1r0) | 0xC3));
        blbset_until_ = 0;
        return 0;
    }
    if (refused) {
        note_once("spm-lock", "SPM was refused: the boot lock bits protect that section");
        return 0;
    }
    const std::uint64_t duration = cycles_at(flash_write_ns);
    if (op == 0x02) { // page erase
        for (std::uint32_t w = 0; w < page_words; ++w) {
            cpu_.write_flash_word(page + w, 0xFFFF);
        }
    } else if (op == 0x04) { // page write
        for (std::uint32_t w = 0; w < page_words; ++w) {
            cpu_.write_flash_word(page + w, static_cast<std::uint16_t>(cpu_.flash_word(page + w) & page_buffer_[w]));
        }
        std::fill(page_buffer_.begin(), page_buffer_.end(), std::uint16_t{0xFFFF});
    } else {
        return 0;
    }
    if (to_boot) {
        return static_cast<unsigned>(duration); // writing the NRWW section halts the CPU
    }
    rww_busy_ = true;
    spm_until_ = now + duration;
    return 0;
}

bool Atmega::lpm_special(std::uint16_t z, std::uint8_t& value)
{
    // BLBSET with SPMEN, then LPM within three cycles: the fuse and lock bytes.
    if (bit(spmcr_, 3) && bit(spmcr_, 0) && cpu_.cycles() < spm_window_) {
        spmcr_ &= 0x80;
        switch (z) {
        case 0x0000: value = options_.fuse_low; return true;
        case 0x0001: value = options_.lock; return true;
        case 0x0003: value = options_.fuse_high; return true;
        default: value = 0xFF; return true;
        }
    }
    // Lock bits: LPM from the boot section into the application (BLB02) or
    // from the application into the boot section (BLB12) reads 0.
    const std::uint32_t boot = boot_start_words() * 2;
    const bool from_boot = cpu_.pc() * 2 >= boot;
    if ((from_boot && z < boot && !bit(options_.lock, 3)) || (!from_boot && z >= boot && !bit(options_.lock, 5))) {
        value = 0;
        return true;
    }
    if (rww_busy_ && z < boot) {
        value = 0xFF; // the RWW section is busy
        return true;
    }
    return false;
}

} // namespace simnodus::emu
