// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 machine (experiment E-12, SN-294). Register descriptions follow
// RM0090; the alternate functions of the serial ports follow the
// STM32F405xx/407xx datasheet's alternate function mapping table.
#include "stm32f407.hpp"

#include "reg_device.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <utility>

namespace simnodus::emu {

namespace {

constexpr std::uint32_t hsi_hz = 16000000;
constexpr std::uint64_t ns_per_s = 1000000000ull;

// Multiplies `value` by `num` / `den` without overflow for simulation times.
std::uint64_t scale(std::uint64_t value, std::uint64_t num, std::uint64_t den, bool round_up)
{
    const std::uint64_t q = value / den;
    const std::uint64_t r = value % den;
    const std::uint64_t part = r * num;
    return q * num + part / den + (round_up && part % den ? 1 : 0);
}

// A serial port: its bus, interrupt, alternate function number and the pins
// that can carry TX and RX (port, pin; port -1 ends the list).
struct UsartPins {
    const char* name;
    std::uint32_t base;
    bool apb2;
    int irq;
    int function;
    std::pair<int, int> tx[3];
    std::pair<int, int> rx[3];
};

constexpr int A = 0, B = 1, C = 2, D = 3;
constexpr std::pair<int, int> none{-1, 0};

const UsartPins usart_pins[Stm32F407::num_usarts] = {
    {"USART1", 0x40011000u, true, 37, 7, {{A, 9}, {B, 6}, none}, {{A, 10}, {B, 7}, none}},
    {"USART2", 0x40004400u, false, 38, 7, {{A, 2}, {D, 5}, none}, {{A, 3}, {D, 6}, none}},
    {"USART3", 0x40004800u, false, 39, 7, {{B, 10}, {C, 10}, {D, 8}}, {{B, 11}, {C, 11}, {D, 9}}},
    {"UART4", 0x40004C00u, false, 52, 8, {{A, 0}, {C, 10}, none}, {{A, 1}, {C, 11}, none}},
    {"UART5", 0x40005000u, false, 53, 8, {{C, 12}, none, none}, {{D, 2}, none, none}},
    {"USART6", 0x40011400u, true, 71, 8, {{C, 6}, none, none}, {{C, 7}, none, none}},
};

// Sources of alternate-function outputs: a serial port is its number; a
// timer's channels are 100 + 10 * timer + channel (stm32f1_timer.cpp).
constexpr int usart_source = 0;

// The timers: what each has (RM0090 chapters 17 to 20) and its interrupt
// lines (break, update, trigger and commutation, capture/compare; the
// others have one line, some shared with TIM1 and TIM8).
struct TimerInfo {
    int number;
    std::uint32_t base;
    bool apb2;
    bool advanced;
    int channels;
    bool wide;
    int irq[4];
};

const TimerInfo timer_info[Stm32F407::num_timers] = {
    {1, 0x40010000u, true, true, 4, false, {24, 25, 26, 27}},
    {2, 0x40000000u, false, false, 4, true, {-1, 28, -1, -1}},
    {3, 0x40000400u, false, false, 4, false, {-1, 29, -1, -1}},
    {4, 0x40000800u, false, false, 4, false, {-1, 30, -1, -1}},
    {5, 0x40000C00u, false, false, 4, true, {-1, 50, -1, -1}},
    {6, 0x40001000u, false, false, 0, false, {-1, 54, -1, -1}},
    {7, 0x40001400u, false, false, 0, false, {-1, 55, -1, -1}},
    {8, 0x40010400u, true, true, 4, false, {43, 44, 45, 46}},
    {9, 0x40014000u, true, false, 2, false, {-1, 24, -1, -1}},
    {10, 0x40014400u, true, false, 1, false, {-1, 25, -1, -1}},
    {11, 0x40014800u, true, false, 1, false, {-1, 26, -1, -1}},
    {12, 0x40001800u, false, false, 2, false, {-1, 43, -1, -1}},
    {13, 0x40001C00u, false, false, 1, false, {-1, 44, -1, -1}},
    {14, 0x40002000u, false, false, 1, false, {-1, 45, -1, -1}},
};

// The alternate function number of a timer's pins.
int timer_function(int number)
{
    return number <= 2 ? 1 : number <= 5 ? 2 : number <= 11 ? 3 : 9;
}

// The pins that can carry a timer's channels on the LQFP-100 package
// (datasheet, alternate function mapping): channel 0 to 3, or 4 to 6 for the
// complementary outputs of channels 1 to 3.
struct TimerPin {
    std::uint8_t timer, channel, port, pin;
};

constexpr int E = 4;

const TimerPin timer_pins[] = {
    {1, 0, A, 8}, {1, 0, E, 9}, {1, 1, A, 9}, {1, 1, E, 11}, {1, 2, A, 10}, {1, 2, E, 13}, {1, 3, A, 11}, {1, 3, E, 14},
    {1, 4, A, 7}, {1, 4, B, 13}, {1, 4, E, 8}, {1, 5, B, 0}, {1, 5, B, 14}, {1, 5, E, 10},
    {1, 6, B, 1}, {1, 6, B, 15}, {1, 6, E, 12},
    {2, 0, A, 0}, {2, 0, A, 5}, {2, 0, A, 15}, {2, 1, A, 1}, {2, 1, B, 3}, {2, 2, A, 2}, {2, 2, B, 10},
    {2, 3, A, 3}, {2, 3, B, 11},
    {3, 0, A, 6}, {3, 0, B, 4}, {3, 0, C, 6}, {3, 1, A, 7}, {3, 1, B, 5}, {3, 1, C, 7}, {3, 2, B, 0}, {3, 2, C, 8},
    {3, 3, B, 1}, {3, 3, C, 9},
    {4, 0, B, 6}, {4, 0, D, 12}, {4, 1, B, 7}, {4, 1, D, 13}, {4, 2, B, 8}, {4, 2, D, 14}, {4, 3, B, 9}, {4, 3, D, 15},
    {5, 0, A, 0}, {5, 1, A, 1}, {5, 2, A, 2}, {5, 3, A, 3},
    {8, 0, C, 6}, {8, 1, C, 7}, {8, 2, C, 8}, {8, 3, C, 9},
    {8, 4, A, 5}, {8, 4, A, 7}, {8, 5, B, 0}, {8, 5, B, 14}, {8, 6, B, 1}, {8, 6, B, 15},
    {9, 0, A, 2}, {9, 0, E, 5}, {9, 1, A, 3}, {9, 1, E, 6},
    {10, 0, B, 8}, {11, 0, B, 9},
    {12, 0, B, 14}, {12, 1, B, 15}, {13, 0, A, 6}, {14, 0, A, 7},
};

} // namespace

// ---------------------------------------------------------------------------
// RCC (RM0090 6.3 for the STM32F405/407): ready flags follow the enables (the
// HSE only with a crystal on the diagram), SWS follows SW, HCLK drives the CPU
// and SysTick.

class Stm32F407::Rcc : public RegDevice {
public:
    explicit Rcc(Stm32F407& m) : m_(m) { reset(); }
    const char* name() const override { return "RCC"; }

    void reset() override
    {
        cr_ = 0x83; // HSION, HSIRDY, HSITRIM 16
        pllcfgr_ = 0x24003010u;
        cfgr_ = 0;
        cir_flags_ = cir_ie_ = 0;
        ahb1enr_ = 0x00100000u; // CCMDATARAMEN
        others_.clear();
        sws_ = 0;
        previous_ready_ = 4;
        update_clocks();
    }

    // The backup domain (BDCR) survives system resets.
    void reset_backup_domain() { bdcr_ = 0; }

    std::uint32_t pll_hz() const
    {
        const std::uint32_t m = pllcfgr_ & 0x3Fu;
        const std::uint32_t n = (pllcfgr_ >> 6) & 0x1FFu;
        const std::uint32_t p = (((pllcfgr_ >> 16) & 3u) + 1u) * 2u;
        const std::uint32_t input = (pllcfgr_ & (1u << 22)) ? m_.options_.hse_hz : hsi_hz;
        if (m == 0) {
            return 0;
        }
        return static_cast<std::uint32_t>(std::uint64_t{input} * n / m / p);
    }

    std::uint32_t sysclk() const
    {
        switch (sws_) {
        case 1: return m_.options_.hse_hz;
        case 2: return pll_hz();
        default: return hsi_hz;
        }
    }

    std::uint32_t hclk() const
    {
        const std::uint32_t hpre = (cfgr_ >> 4) & 15u;
        const unsigned shift = hpre < 8 ? 0 : (hpre < 12 ? hpre - 7 : hpre - 6);
        return sysclk() >> shift;
    }

    // APB divider (1, 2, 4, 8, 16): HCLK cycles per PCLK cycle.
    std::uint32_t apb_divider(bool apb2) const
    {
        const std::uint32_t ppre = (cfgr_ >> (apb2 ? 13 : 10)) & 7u;
        return ppre < 4 ? 1u : 2u << (ppre - 4);
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: {
            std::uint32_t v = cr_ & ~((1u << 1) | (1u << 17) | (1u << 25) | (1u << 27));
            v |= ready(0) ? (1u << 1) : 0;
            v |= ready(1) ? (1u << 17) : 0;
            v |= ready(2) ? (1u << 25) : 0;
            v |= (cr_ & (1u << 26)) ? (1u << 27) : 0; // PLLI2S: ready when on
            value = v;
            return true;
        }
        case 0x04: value = pllcfgr_; return true;
        case 0x08: value = (cfgr_ & ~0xCu) | (sws_ << 2); return true;
        case 0x0C: value = cir_flags_ | (cir_ie_ << 8); return true;
        case 0x30: value = ahb1enr_; return true;
        case 0x70: value = bdcr_ | ((bdcr_ & 1u) && m_.options_.lse_crystal ? 2u : 0u); return true;
        case 0x74: value = (csr_ & 1u) | ((csr_ & 1u) ? 2u : 0u) | m_.reset_flags(); return true;
        default: {
            const auto it = others_.find(offset);
            value = it == others_.end() ? 0 : it->second;
            return true;
        }
        }
    }

    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        switch (offset) {
        case 0x00:
            cr_ = merge(cr_, value, mask) & 0x050D00F9u;
            if ((cr_ & (1u << 16)) && m_.options_.hse_hz == 0 && !reported_no_crystal_) {
                reported_no_crystal_ = true;
                m_.diagnostic("The firmware turned the HSE oscillator on, but no crystal is connected between PH0 "
                              "(OSC_IN) and PH1 (OSC_OUT): HSERDY never sets. Firmware that waits for it stops there, "
                              "as on a board without the crystal.");
            }
            update_ready();
            break;
        case 0x04: pllcfgr_ = merge(pllcfgr_, value, mask) & 0x0F437FFFu; update_clocks(); break;
        case 0x08: cfgr_ = merge(cfgr_, value, mask) & 0xFFFFFCF3u; switch_clock(); break;
        case 0x0C:
            cir_ie_ = merge(cir_ie_ << 8, value, mask & 0x3F00u) >> 8;
            cir_flags_ &= ~((value & mask & 0xBF0000u) >> 16);
            break;
        case 0x30: ahb1enr_ = merge(ahb1enr_, value, mask); break;
        case 0x70: {
            const std::uint32_t old = bdcr_;
            bdcr_ = merge(bdcr_, value, mask) & 0x18305u;
            if ((bdcr_ & 0x10000u) && !(old & 0x10000u)) {
                bdcr_ = 0x10000u; // BDRST resets the whole backup domain, BDCR included
            }
            break;
        }
        case 0x74:
            csr_ = merge(csr_, value, mask) & 1u;
            if (value & mask & (1u << 24)) {
                m_.clear_reset_flags(); // RMVF
            }
            break;
        default:
            others_[offset] = merge(others_[offset], value, mask); // resets, enables, low-power enables: kept
            break;
        }
        return true;
    }

private:
    // HSI, HSE and the main PLL. The HSE needs the crystal; the PLL its source.
    bool ready(std::uint32_t source) const
    {
        switch (source) {
        case 0: return (cr_ & 1u) != 0;
        case 1: return (cr_ & (1u << 16)) != 0 && m_.options_.hse_hz != 0;
        case 2: return (cr_ & (1u << 24)) != 0 && ((pllcfgr_ & (1u << 22)) ? ready(1) : ready(0));
        default: return false;
        }
    }

    void update_ready()
    {
        const std::uint32_t now = (ready(0) ? 4u : 0u) | (ready(1) ? 8u : 0u) | (ready(2) ? 16u : 0u);
        cir_flags_ |= now & ~previous_ready_;
        previous_ready_ = now;
        if (!ready(sws_)) {
            m_.diagnostic("The clock source that drives SYSCLK was switched off; SimNodus falls back to HSI.");
            sws_ = 0;
        }
        switch_clock();
    }

    void switch_clock()
    {
        const std::uint32_t sw = cfgr_ & 3u;
        if (sw != 3 && ready(sw)) {
            sws_ = sw;
        }
        update_clocks();
    }

    void update_clocks()
    {
        m_.clocks_changed();
        const std::uint32_t hz = hclk();
        if (hz != 0 && hz != applied_) {
            applied_ = hz;
            m_.set_hclk(hz);
            // Wait states at 2.7 to 3.6 V (RM0090 table 10): one more for every 30 MHz.
            const std::uint32_t needed = (hz - 1) / 30000000u;
            if (m_.flash_latency() < needed) {
                char text[220];
                std::snprintf(text, sizeof text,
                    "HCLK is %u Hz but FLASH_ACR has %u wait state(s); the STM32F407 needs %u at this frequency. "
                    "A real chip may crash; set the flash latency before switching the clock.",
                    static_cast<unsigned>(hz), static_cast<unsigned>(m_.flash_latency()), static_cast<unsigned>(needed));
                m_.diagnostic(text);
            }
        }
    }

    Stm32F407& m_;
    std::uint32_t cr_ = 0, pllcfgr_ = 0, cfgr_ = 0, cir_flags_ = 0, cir_ie_ = 0, ahb1enr_ = 0, bdcr_ = 0, csr_ = 0;
    std::map<std::uint32_t, std::uint32_t> others_;
    std::uint32_t sws_ = 0, previous_ready_ = 0, applied_ = 0;
    bool reported_no_crystal_ = false;
};

// ---------------------------------------------------------------------------
// Flash memory interface (RM0090 3.9): registers only; programming is not simulated.

class Stm32F407::Flash : public RegDevice {
public:
    explicit Flash(Stm32F407& m) : m_(m) { reset(); }
    const char* name() const override { return "FLASH"; }
    void reset() override
    {
        acr_ = 0;
        cr_ = 0x80000000u; // LOCK
        m_.flash_access_changed();
    }
    std::uint32_t latency() const { return acr_ & 7u; }
    // The ART accelerator: prefetch, or the instruction cache.
    bool accelerated() const { return (acr_ & 0x300u) != 0; }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: value = acr_; return true;
        case 0x10: value = cr_; return true;
        case 0x14: value = 0x0FFFAAEDu; return true; // OPTCR: no protection
        default: value = 0; return true;
        }
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        if (offset == 0x00) {
            acr_ = merge(acr_, value, mask) & 0x1F07u;
            m_.flash_access_changed();
        } else if (offset == 0x10) {
            if (value & mask & 0x00010007u) { // PG, SER, MER, STRT
                if (!reported_) {
                    reported_ = true;
                    m_.diagnostic("Programming the flash from the firmware is not simulated: the flash keeps its contents.");
                }
            }
        }
        return true;
    }

private:
    Stm32F407& m_;
    std::uint32_t acr_ = 0, cr_ = 0;
    bool reported_ = false;
};

// ---------------------------------------------------------------------------
// PWR (RM0090 5.4): the regulator's scale is ready at once.

class Stm32F407::Pwr : public RegDevice {
public:
    const char* name() const override { return "PWR"; }
    void reset() override
    {
        cr_ = 0x4000u; // VOS
        csr_ = 0;
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        value = offset == 0 ? cr_ : offset == 4 ? (csr_ | 0x4000u) : 0; // VOSRDY
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        if (offset == 0) {
            cr_ = merge(cr_, value, mask) & 0x4FFFu & ~0xCu; // CWUF and CSBF are write-only
        } else if (offset == 4) {
            csr_ = merge(csr_, value, mask) & 0x300u;
        }
        return true;
    }

private:
    std::uint32_t cr_ = 0, csr_ = 0;
};

// ---------------------------------------------------------------------------
// GPIO port (RM0090 8.4): mode, output type, speed (kept, not simulated),
// pull-up or pull-down, input and output data, BSRR, lock (not enforced) and
// the alternate function numbers.

class Stm32F407::Gpio : public RegDevice {
public:
    Gpio(Stm32F407& m, int port) : m_(m), port_(port)
    {
        std::snprintf(name_, sizeof name_, "GPIO%c", 'A' + port);
        reset();
    }
    const char* name() const override { return name_; }

    void reset() override
    {
        // PA13, PA14, PA15, PB3 and PB4 are the debug pins after a reset.
        moder_ = port_ == 0 ? 0xA8000000u : port_ == 1 ? 0x00000280u : 0u;
        otyper_ = 0;
        ospeedr_ = port_ == 1 ? 0x000000C0u : 0u;
        pupdr_ = port_ == 0 ? 0x64000000u : port_ == 1 ? 0x00000100u : 0u;
        odr_ = 0;
        lckr_ = 0;
        afr_ = 0;
        for (auto& drivers : af_drivers_) {
            drivers.clear();
        }
        for (auto& claims : af_claims_) {
            claims.clear();
        }
        update();
    }

    std::uint32_t mode(int pin) const { return (moder_ >> (2 * pin)) & 3u; }
    std::uint32_t function(int pin) const { return static_cast<std::uint32_t>((afr_ >> (4 * pin)) & 15u); }
    bool level(int pin) const { return (levels_ >> pin) & 1u; }

    F4PinConfig config(int pin) const
    {
        F4PinConfig c;
        c.mode = static_cast<std::uint8_t>(mode(pin));
        c.open_drain = (otyper_ >> pin) & 1u;
        c.pull = static_cast<std::uint8_t>((pupdr_ >> (2 * pin)) & 3u);
        c.function = static_cast<std::uint8_t>(function(pin));
        c.level = level(pin);
        c.drive_high = c.mode == 2 ? af_level(pin) : ((odr_ >> pin) & 1u) != 0;
        c.af_input = c.mode == 2 && !af_driven(pin);
        return c;
    }

    void set_input(int pin, bool high)
    {
        const std::uint32_t bit = 1u << pin;
        input_set_ |= bit;
        input_ = high ? (input_ | bit) : (input_ & ~bit);
        update();
    }

    // A peripheral's output on the pin: it reaches the pin only while the
    // pin's alternate function number is the peripheral's. A pin whose
    // function no peripheral drives is an input (RM0090 figure 26: the input
    // buffer is always on, the output driver is the peripheral's).
    void set_af(int pin, int source, int function_number, bool high)
    {
        auto& drivers = af_drivers_[static_cast<std::size_t>(pin)];
        auto& claims = af_claims_[static_cast<std::size_t>(pin)];
        const std::pair<int, int> driver{source, function_number};
        const bool claimed = std::find(claims.begin(), claims.end(), driver) == claims.end();
        if (claimed) {
            claims.push_back(driver);
        }
        const auto it = std::find(drivers.begin(), drivers.end(), driver);
        if (claimed && !high && it == drivers.end()) {
            update();
            return;
        }
        if (high && it == drivers.end()) {
            drivers.push_back(driver);
        } else if (!high && it != drivers.end()) {
            drivers.erase(it);
        } else {
            return;
        }
        update();
    }

    void update()
    {
        std::uint32_t af_levels = 0, af_inputs = 0;
        for (int pin = 0; pin < 16; ++pin) {
            af_levels |= af_level(pin) ? 1u << pin : 0u;
            af_inputs |= af_driven(pin) ? 0u : 1u << pin;
        }
        std::uint32_t levels = 0;
        for (int pin = 0; pin < 16; ++pin) {
            const std::uint32_t bit = 1u << pin;
            const std::uint32_t pull = (pupdr_ >> (2 * pin)) & 3u;
            // What the pin sees from outside: the circuit's level, or its own pull resistor.
            const bool external = (input_set_ & bit) ? (input_ & bit) != 0 : pull == 1;
            bool high = false;
            switch (mode(pin)) {
            case 0: high = external; break;
            case 3: high = false; break; // analog: the digital input is off and reads 0
            case 2:
                if (af_inputs & bit) {
                    high = external;
                    break;
                }
                [[fallthrough]];
            default: {
                const bool driven = mode(pin) == 2 ? (af_levels & bit) != 0 : (odr_ & bit) != 0;
                high = (otyper_ & bit) && driven ? external : driven; // open drain: released when high
                break;
            }
            }
            levels |= high ? bit : 0u;
        }
        const std::uint32_t changed = levels ^ levels_;
        levels_ = levels;
        for (int pin = 0; pin < 16; ++pin) {
            const std::uint32_t bit = 1u << pin;
            // The configuration as the circuit sees it, with the level a driver gives.
            const std::uint32_t config = mode(pin) | ((otyper_ >> pin) & 1u) << 2 | ((pupdr_ >> (2 * pin)) & 3u) << 3
                | ((odr_ >> pin) & 1u) << 5 | ((af_levels >> pin) & 1u) << 6 | function(pin) << 7
                | ((af_inputs >> pin) & 1u) << 11;
            const bool config_changed = !updated_ || config != configs_[static_cast<std::size_t>(pin)];
            configs_[static_cast<std::size_t>(pin)] = config;
            if (config_changed || (changed & bit)) {
                m_.pin_output_changed(port_, pin);
            }
            if (changed & bit) {
                m_.pin_input_changed(port_, pin, (levels & bit) != 0);
            }
        }
        updated_ = true;
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: value = moder_; return true;
        case 0x04: value = otyper_; return true;
        case 0x08: value = ospeedr_; return true;
        case 0x0C: value = pupdr_; return true;
        case 0x10: value = levels_ & 0xFFFFu; return true;
        case 0x14: value = odr_; return true;
        case 0x1C: value = lckr_; return true;
        case 0x20: value = static_cast<std::uint32_t>(afr_); return true;
        case 0x24: value = static_cast<std::uint32_t>(afr_ >> 32); return true;
        default: value = 0; return true; // BSRR reads as 0
        }
    }

    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        bool routing = false;
        switch (offset) {
        case 0x00: moder_ = merge(moder_, value, mask); routing = true; break;
        case 0x04: otyper_ = merge(otyper_, value, mask) & 0xFFFFu; break;
        case 0x08: ospeedr_ = merge(ospeedr_, value, mask); break;
        case 0x0C: pupdr_ = merge(pupdr_, value, mask); break;
        case 0x14: odr_ = merge(odr_, value, mask) & 0xFFFFu; break;
        case 0x18: {
            const std::uint32_t v = value & mask;
            odr_ = (odr_ & ~(v >> 16)) | (v & 0xFFFFu); // set wins over reset
            break;
        }
        case 0x1C: lckr_ = merge(lckr_, value, mask) & 0x1FFFFu; break;
        case 0x20:
            afr_ = (afr_ & 0xFFFFFFFF00000000ull) | merge(static_cast<std::uint32_t>(afr_), value, mask);
            routing = true;
            break;
        case 0x24:
            afr_ = (afr_ & 0xFFFFFFFFull) | (std::uint64_t{merge(static_cast<std::uint32_t>(afr_ >> 32), value, mask)} << 32);
            routing = true;
            break;
        default: return true;
        }
        update();
        if (routing) {
            m_.routing_changed(); // the peripherals look for their pins again
        }
        return true;
    }

private:
    bool af_level(int pin) const
    {
        const int wanted = static_cast<int>(function(pin));
        for (const auto& [source, number] : af_drivers_[static_cast<std::size_t>(pin)]) {
            if (number == wanted) {
                return true;
            }
        }
        return false;
    }
    bool af_driven(int pin) const
    {
        const int wanted = static_cast<int>(function(pin));
        for (const auto& [source, number] : af_claims_[static_cast<std::size_t>(pin)]) {
            if (number == wanted) {
                return true;
            }
        }
        return false;
    }

    Stm32F407& m_;
    int port_;
    char name_[8] = {};
    std::uint32_t moder_ = 0, otyper_ = 0, ospeedr_ = 0, pupdr_ = 0, odr_ = 0, lckr_ = 0;
    std::uint64_t afr_ = 0;
    std::uint32_t input_ = 0, input_set_ = 0;
    std::uint32_t levels_ = 0;
    std::array<std::vector<std::pair<int, int>>, 16> af_drivers_; // per pin: (source, function) driving high
    std::array<std::vector<std::pair<int, int>>, 16> af_claims_;  // per pin: (source, function) driving it at all
    std::array<std::uint32_t, 16> configs_{};
    bool updated_ = false;
};

// ---------------------------------------------------------------------------
// SYSCFG (RM0090 9.2): the port of each external interrupt line.

class Stm32F407::Syscfg : public RegDevice {
public:
    explicit Syscfg(Stm32F407& m) : m_(m) { reset(); }
    const char* name() const override { return "SYSCFG"; }
    void reset() override
    {
        memrmp_ = pmc_ = 0;
        exticr_ = {};
    }
    int exti_port(int line) const
    {
        return static_cast<int>((exticr_[static_cast<std::size_t>(line / 4)] >> (4 * (line % 4))) & 15u);
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        if (offset >= 0x08 && offset <= 0x14) {
            value = exticr_[(offset - 0x08) / 4];
        } else {
            value = offset == 0 ? memrmp_ : offset == 4 ? pmc_ : 0;
        }
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        if (offset >= 0x08 && offset <= 0x14) {
            auto& reg = exticr_[(offset - 0x08) / 4];
            reg = merge(reg, value, mask) & 0xFFFFu;
            m_.exti_selection_changed();
        } else if (offset == 0) {
            memrmp_ = merge(memrmp_, value, mask) & 3u;
        } else if (offset == 4) {
            pmc_ = merge(pmc_, value, mask);
        }
        return true;
    }

private:
    Stm32F407& m_;
    std::uint32_t memrmp_ = 0, pmc_ = 0;
    std::array<std::uint32_t, 4> exticr_{};
};

// ---------------------------------------------------------------------------
// EXTI (RM0090 12.3): 23 lines; lines 0 to 15 are pins.

class Stm32F407::Exti : public RegDevice {
public:
    explicit Exti(Stm32F407& m) : m_(m) { reset(); }
    const char* name() const override { return "EXTI"; }
    void reset() override
    {
        imr_ = emr_ = rtsr_ = ftsr_ = swier_ = pr_ = 0;
        update_irqs();
    }

    void input(int line, bool level)
    {
        const std::uint32_t bit = 1u << line;
        const bool old = (levels_ & bit) != 0;
        levels_ = level ? (levels_ | bit) : (levels_ & ~bit);
        const bool edge = (!old && level && (rtsr_ & bit)) || (old && !level && (ftsr_ & bit));
        if (edge && (imr_ & bit)) {
            pr_ |= bit;
            update_irqs();
        }
    }

    void set_level(int line, bool level)
    {
        const std::uint32_t bit = 1u << line;
        levels_ = level ? (levels_ | bit) : (levels_ & ~bit);
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: value = imr_; return true;
        case 0x04: value = emr_; return true;
        case 0x08: value = rtsr_; return true;
        case 0x0C: value = ftsr_; return true;
        case 0x10: value = swier_; return true;
        case 0x14: value = pr_; return true;
        default: value = 0; return true;
        }
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        constexpr std::uint32_t lines = 0x7FFFFFu;
        switch (offset) {
        case 0x00: imr_ = merge(imr_, value, mask) & lines; break;
        case 0x04: emr_ = merge(emr_, value, mask) & lines; break;
        case 0x08: rtsr_ = merge(rtsr_, value, mask) & lines; break;
        case 0x0C: ftsr_ = merge(ftsr_, value, mask) & lines; break;
        case 0x10: {
            const std::uint32_t set = value & mask & lines & ~swier_;
            swier_ |= value & mask & lines;
            pr_ |= set & imr_;
            break;
        }
        case 0x14: {
            const std::uint32_t clear = value & mask & lines;
            pr_ &= ~clear;
            swier_ &= ~clear;
            break;
        }
        default: break;
        }
        update_irqs();
        return true;
    }

private:
    void update_irqs()
    {
        Cpu& cpu = m_.cpu();
        for (int line = 0; line < 5; ++line) {
            cpu.set_irq_line(6 + line, (pr_ >> line) & 1u); // EXTI0 to EXTI4
        }
        cpu.set_irq_line(23, (pr_ & 0x3E0u) != 0);  // EXTI9_5
        cpu.set_irq_line(40, (pr_ & 0xFC00u) != 0); // EXTI15_10
        cpu.set_irq_line(1, (pr_ >> 16) & 1u);      // PVD
        cpu.set_irq_line(41, (pr_ >> 17) & 1u);     // RTC alarm
        cpu.set_irq_line(42, (pr_ >> 18) & 1u);     // USB OTG FS wakeup
        cpu.set_irq_line(62, (pr_ >> 19) & 1u);     // Ethernet wakeup
        cpu.set_irq_line(76, (pr_ >> 20) & 1u);     // USB OTG HS wakeup
        cpu.set_irq_line(2, (pr_ >> 21) & 1u);      // tamper and time stamp
        cpu.set_irq_line(3, (pr_ >> 22) & 1u);      // RTC wakeup
    }

    Stm32F407& m_;
    std::uint32_t imr_ = 0, emr_ = 0, rtsr_ = 0, ftsr_ = 0, swier_ = 0, pr_ = 0, levels_ = 0;
};

// ---------------------------------------------------------------------------
// Peripherals that are not simulated yet: registers keep what is written.

class Stm32F407::Stub : public RegDevice {
public:
    Stub(Stm32F407& m, const char* name) : m_(m), name_(name) {}
    const char* name() const override { return name_; }
    void reset() override { regs_.clear(); }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        report();
        const auto it = regs_.find(offset);
        value = it == regs_.end() ? 0 : it->second;
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        report();
        regs_[offset] = merge(regs_[offset], value, mask);
        return true;
    }

private:
    void report()
    {
        if (!reported_ && !peeking()) {
            reported_ = true;
            m_.diagnostic(std::string(name_) + " is not simulated yet on the STM32F407: its registers keep what the "
                "firmware writes, but the peripheral does nothing.");
        }
    }

    Stm32F407& m_;
    const char* name_;
    std::map<std::uint32_t, std::uint32_t> regs_;
    bool reported_ = false;
};

// DWT: CYCCNT counts core cycles while CTRL.CYCCNTENA is set.
class Stm32F407::Dwt : public RegDevice {
public:
    explicit Dwt(Stm32F407& m) : m_(m) {}
    const char* name() const override { return "DWT"; }
    void reset() override
    {
        ctrl_ = 0x40000000u; // NUMCOMP = 4
        count_ = 0;
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        value = offset == 0 ? ctrl_ : offset == 4 ? static_cast<std::uint32_t>(current()) : 0;
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        if (offset == 0) {
            count_ = current();
            since_ = m_.cycles();
            ctrl_ = (ctrl_ & 0xF0000000u) | (merge(ctrl_, value, mask) & 0x0FFFFFFFu);
        } else if (offset == 4) {
            count_ = merge(static_cast<std::uint32_t>(current()), value, mask);
            since_ = m_.cycles();
        }
        return true;
    }

private:
    std::uint64_t current() const { return (ctrl_ & 1u) ? count_ + (m_.cycles() - since_) : count_; }
    Stm32F407& m_;
    std::uint32_t ctrl_ = 0x40000000u;
    std::uint64_t count_ = 0;
    std::uint64_t since_ = 0;
};

// DBGMCU: the device (0x413) and its revision; CR keeps what is written.
class Stm32F407::Dbgmcu : public RegDevice {
public:
    const char* name() const override { return "DBGMCU"; }
    void reset() override { cr_ = 0; }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        value = offset == 0 ? 0x10076413u : offset == 4 ? cr_ : 0;
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        if (offset == 4) {
            cr_ = merge(cr_, value, mask);
        }
        return true;
    }

private:
    std::uint32_t cr_ = 0;
};

// Debug components (ITM, FPB, TPIU, ROM table): read as zero, writes ignored.
class Stm32F407::Zero : public Device {
public:
    const char* name() const override { return "debug components"; }
    bool read(std::uint32_t, unsigned, std::uint32_t& value) override
    {
        value = 0;
        return true;
    }
    bool write(std::uint32_t, unsigned, std::uint32_t) override { return true; }
};

// ---------------------------------------------------------------------------
// Machine

Stm32F407::Stm32F407(F4MachineOptions options) : options_(options), cpu_(bus_)
{
    cpu_.set_architecture(Cpu::Architecture::CortexM4F);
    cpu_.set_irq_count(82);
    cpu_.set_code_cache_bytes(options_.flash_size);
    cpu_.set_timing(options_.cortex_timing ? Cpu::Timing::CortexM3 : Cpu::Timing::OneCyclePerInstruction);
    bus_.add_memory("SRAM", 0x20000000u, 0x20000u, true); // SRAM1 (112 KiB) and SRAM2 (16 KiB): checked first
    bus_.add_memory("flash", 0x08000000u, options_.flash_size, false);
    bus_.add_alias("flash (boot alias)", 0x00000000u, "flash");
    bus_.add_memory("CCM data RAM", 0x10000000u, 0x10000u, true);
    bus_.add_memory("system memory", 0x1FFF0000u, 0x7A30u, false);
    bus_.add_memory("option bytes", 0x1FFFC000u, 0x10u, false);
    bus_.add_bit_band(0x22000000u, 0x20000u * 32u, 0x20000000u);
    bus_.add_bit_band(0x42000000u, 0x02000000u, 0x40000000u);

    // The unique device ID and the flash size (KiB) in system memory; option bytes.
    const std::uint8_t uid[12] = {0x53, 0x4E, 0x04, 0x07, 0x11, 0x26, 0x00, 0x00, 0x11, 0x11, 0x26, 0x00};
    bus_.load(0x1FFF7A10u, uid, sizeof uid);
    const std::uint16_t flash_kib = static_cast<std::uint16_t>(options_.flash_size / 1024);
    bus_.load(0x1FFF7A22u, reinterpret_cast<const std::uint8_t*>(&flash_kib), 2);
    const std::uint8_t option_bytes[16] = {0xED, 0xAA, 0x12, 0x55, 0, 0, 0, 0, 0xFF, 0x0F, 0x00, 0xF0, 0, 0, 0, 0};
    bus_.load(0x1FFFC000u, option_bytes, sizeof option_bytes);

    flash_ = std::make_unique<Flash>(*this);
    rcc_ = std::make_unique<Rcc>(*this);
    pwr_ = std::make_unique<Pwr>();
    syscfg_ = std::make_unique<Syscfg>(*this);
    exti_ = std::make_unique<Exti>(*this);
    for (int port = 0; port < num_ports; ++port) {
        gpio_[static_cast<std::size_t>(port)] = std::make_unique<Gpio>(*this, port);
        bus_.add_device(0x40020000u + 0x400u * static_cast<std::uint32_t>(port), 0x400, gpio_[static_cast<std::size_t>(port)].get());
    }
    bus_.add_device(0x40023800u, 0x400, rcc_.get());
    bus_.add_device(0x40023C00u, 0x400, flash_.get());
    bus_.add_device(0x40007000u, 0x400, pwr_.get());
    bus_.add_device(0x40013800u, 0x400, syscfg_.get());
    bus_.add_device(0x40013C00u, 0x400, exti_.get());
    for (int n = 0; n < num_usarts; ++n) {
        const UsartPins& u = usart_pins[n];
        usart_[static_cast<std::size_t>(n)] = std::make_unique<Stm32Usart>(static_cast<UsartHost&>(*this), options_.usart,
            n + 1, u.irq, true, u.name);
        bus_.add_device(u.base, 0x400, usart_[static_cast<std::size_t>(n)].get());
        timed_.push_back(usart_[static_cast<std::size_t>(n)].get());
    }

    for (const TimerInfo& info : timer_info) {
        TimerConfig config;
        config.number = info.number;
        config.advanced = info.advanced;
        config.channels = info.channels;
        config.wide = info.wide;
        config.apb2 = info.apb2;
        config.both_edges = true;
        config.irq_break = info.irq[0];
        config.irq_update = info.irq[1];
        config.irq_trigger = info.irq[2];
        config.irq_compare = info.irq[3];
        auto& timer = timers_[static_cast<std::size_t>(info.number - 1)];
        timer = std::make_unique<Stm32F1Timer>(static_cast<TimerHost&>(*this), config);
        bus_.add_device(info.base, 0x400, timer.get());
        timed_.push_back(timer.get());
    }
    adc_ = std::make_unique<Stm32F4Adc>(static_cast<F4AnalogHost&>(*this));
    bus_.add_device(0x40012000u, 0x400, adc_.get());
    timed_.push_back(adc_.get());
    dac_ = std::make_unique<Stm32F4Dac>(static_cast<F4AnalogHost&>(*this));
    bus_.add_device(0x40007400u, 0x400, dac_.get());

    // The rest of the chip, not simulated yet (SN-298 and later).
    struct Block {
        std::uint32_t base;
        std::uint32_t size;
        const char* name;
    };
    static const Block blocks[] = {
        {0x40002800u, 0x400, "RTC and the backup registers"}, {0x40002C00u, 0x400, "WWDG"},
        {0x40003000u, 0x400, "IWDG"}, {0x40003400u, 0x400, "I2S2ext"}, {0x40003800u, 0x400, "SPI2"},
        {0x40003C00u, 0x400, "SPI3"}, {0x40004000u, 0x400, "I2S3ext"}, {0x40005400u, 0x400, "I2C1"},
        {0x40005800u, 0x400, "I2C2"}, {0x40005C00u, 0x400, "I2C3"}, {0x40006400u, 0x400, "CAN1"},
        {0x40006800u, 0x400, "CAN2"}, {0x40012C00u, 0x400, "SDIO"},
        {0x40013000u, 0x400, "SPI1"}, {0x40023000u, 0x400, "CRC"}, {0x40024000u, 0x1000, "backup SRAM"},
        {0x40026000u, 0x400, "DMA1"}, {0x40026400u, 0x400, "DMA2"}, {0x40028000u, 0x1400, "Ethernet"},
        {0x40040000u, 0x40000, "USB OTG HS"}, {0x50000000u, 0x40000, "USB OTG FS"}, {0x50050000u, 0x400, "DCMI"},
        {0x50060800u, 0x400, "RNG"}, {0xA0000000u, 0x1000, "FSMC"},
    };
    for (const Block& block : blocks) {
        stubs_.push_back(std::make_unique<Stub>(*this, block.name));
        bus_.add_device(block.base, block.size, stubs_.back().get());
    }

    bus_.add_device(0xE000E000u, 0x1000, &cpu_.scs());
    dwt_ = std::make_unique<Dwt>(*this);
    bus_.add_device(0xE0001000u, 0x1000, dwt_.get());
    dbgmcu_ = std::make_unique<Dbgmcu>();
    bus_.add_device(0xE0042000u, 0x400, dbgmcu_.get());
    debug_zero_ = std::make_unique<Zero>();
    bus_.add_device(0xE0000000u, 0x1000, debug_zero_.get()); // ITM
    bus_.add_device(0xE0002000u, 0x1000, debug_zero_.get()); // FPB
    bus_.add_device(0xE0040000u, 0x1000, debug_zero_.get()); // TPIU
    bus_.add_device(0xE00FF000u, 0x1000, debug_zero_.get()); // ROM table

    cpu_.on_system_reset = [this] {
        reset_requested_ = true;
        requested_kind_ = ResetKind::Software;
    };
    cpu_.on_fault = [this](int number, const std::string& why) {
        static const char* const names[] = {"", "", "", "HardFault", "MemManage fault", "BusFault", "UsageFault"};
        diagnostic(std::string(names[number]) + ": " + (why.empty() ? "fault" : why)
            + ". The firmware continues in its fault handler.");
    };
    reset(ResetKind::PowerOn);
}

Stm32F407::~Stm32F407() = default;

bool Stm32F407::load(const ElfImage& image, std::string& error)
{
    // Power-on first (it clears the SRAM), then the image, then the core reads its vectors.
    image_ = std::make_unique<ElfImage>(image);
    reset(ResetKind::PowerOn);
    take_diagnostics();
    for (const auto& segment : image.segments) {
        if (!segment.data.empty()
            && !bus_.load(segment.physical, segment.data.data(), static_cast<std::uint32_t>(segment.data.size()))) {
            char text[220];
            std::snprintf(text, sizeof text,
                "The firmware does not fit the STM32F407VE: %u bytes at 0x%08X are outside its flash (%u KiB), SRAM "
                "(128 KiB) and CCM memory (64 KiB).",
                static_cast<unsigned>(segment.data.size()), static_cast<unsigned>(segment.physical),
                static_cast<unsigned>(options_.flash_size / 1024));
            error = text;
            return false;
        }
        if (segment.memory_size > segment.data.size()) {
            bus_.fill(segment.physical + static_cast<std::uint32_t>(segment.data.size()), 0,
                segment.memory_size - static_cast<std::uint32_t>(segment.data.size()));
        }
    }
    cpu_.invalidate_code_cache();
    cpu_.reset();
    if (cpu_.state() == CoreState::Lockup) {
        diagnostic(cpu_.last_fault());
    }
    return true;
}

void Stm32F407::reset(ResetKind kind)
{
    switch (kind) {
    case ResetKind::PowerOn:
        reset_flags_ = 0x0E000000u; // PORRSTF | PINRSTF | BORRSTF
        rcc_->reset_backup_domain();
        for (const std::uint32_t base : {0x20000000u, 0x10000000u}) {
            const std::uint32_t size = base == 0x20000000u ? 0x20000u : 0x10000u;
            if (std::uint8_t* memory = bus_.memory_at(base, size)) {
                std::fill(memory, memory + size, std::uint8_t{0});
            }
        }
        break;
    case ResetKind::Pin:
        reset_flags_ |= 0x04000000u; // PINRSTF
        break;
    case ResetKind::Software:
        reset_flags_ |= 0x14000000u; // SFTRSTF | PINRSTF
        break;
    case ResetKind::IndependentWatchdog:
        reset_flags_ |= 0x24000000u; // IWDGRSTF | PINRSTF
        break;
    case ResetKind::WindowWatchdog:
        reset_flags_ |= 0x44000000u; // WWDGRSTF | PINRSTF
        break;
    }
    reset_requested_ = false;
    irq_sources_.fill(0);
    dac_pins_ = {};
    bus_.reset_devices();
    for (auto& gpio : gpio_) {
        gpio->update();
    }
    exti_selection_changed();
    cpu_.reset();
    // BOOT0 is read as the reset ends (BOOT1, PB2, is not looked at: with
    // BOOT0 high the chip does not run the flash in either case).
    bootloader_ = boot0_;
    if (bootloader_) {
        if (!reported_bootloader_) {
            reported_bootloader_ = true;
            diagnostic("BOOT0 is high at reset: the chip would start the bootloader of its system memory, which is "
                       "not simulated. Tie BOOT0 to ground to run the firmware in the flash.");
        }
        return;
    }
    if (cpu_.state() == CoreState::Lockup) {
        diagnostic(cpu_.last_fault());
    }
}

void Stm32F407::set_reset_pin(bool low)
{
    if (low == reset_low_) {
        return;
    }
    reset_low_ = low;
    reset(ResetKind::Pin); // into reset, and out of it from the reset vector
}

std::uint64_t Stm32F407::time_ns() const
{
    return base_ns_ + scale(cpu_.cycles() - base_cycles_, ns_per_s, hclk_, false);
}

std::uint64_t Stm32F407::cycles_at_ns(std::uint64_t ns) const
{
    if (ns <= base_ns_) {
        return base_cycles_;
    }
    return base_cycles_ + scale(ns - base_ns_, hclk_, ns_per_s, true);
}

std::uint64_t Stm32F407::ns_at_cycles(std::uint64_t cycles) const
{
    return base_ns_ + (cycles > base_cycles_ ? scale(cycles - base_cycles_, ns_per_s, hclk_, false) : 0);
}

std::uint32_t Stm32F407::sysclk_hz() const
{
    return rcc_->sysclk();
}

void Stm32F407::set_hclk(std::uint32_t hz)
{
    if (hz == 0 || hz == hclk_) {
        return;
    }
    base_ns_ = time_ns();
    base_cycles_ = cpu_.cycles();
    hclk_ = hz;
    cpu_.request_stop(); // the run loop recomputes its cycle target
}

std::uint32_t Stm32F407::flash_latency() const
{
    return flash_ ? flash_->latency() : 0;
}

void Stm32F407::flash_access_changed()
{
    // Until the flash and the ART accelerator are modelled from measurements
    // (SN-296): no wait state with the accelerator on, the latency without it.
    if (flash_) {
        cpu_.set_flash_access(flash_->accelerated() ? 0 : flash_->latency(), true);
    }
}

std::uint64_t Stm32F407::next_timer_event() const
{
    std::uint64_t next = no_event;
    for (const TimedDevice* device : timed_) {
        next = std::min(next, device->next_event());
    }
    return next;
}

void Stm32F407::run_timers()
{
    for (;;) {
        TimedDevice* first = nullptr;
        std::uint64_t at = cpu_.cycles();
        for (TimedDevice* device : timed_) {
            const std::uint64_t event = device->next_event();
            if (event <= at) {
                at = event;
                first = device;
            }
        }
        if (!first) {
            return;
        }
        first->on_time(at);
    }
}

void Stm32F407::run_until_ns(std::uint64_t target_ns)
{
    if (held()) { // in reset, or waiting in the bootloader that is not there: time passes, nothing runs
        cpu_.advance_idle(cycles_at_ns(target_ns));
        return;
    }
    bool reported_lockup = false;
    while (time_ns() < target_ns) {
        if (reset_requested_) {
            reset(requested_kind_);
        }
        // The core stops at the next timed event, which then runs at its cycle.
        const std::uint64_t target = std::max(std::min(cycles_at_ns(target_ns), next_timer_event()), cpu_.cycles() + 1);
        const CoreState state = cpu_.run(target);
        run_timers();
        if (cpu_.debug_halted()) {
            return; // a breakpoint: the debugger decides what happens next
        }
        if (state == CoreState::Lockup && !reported_lockup) {
            reported_lockup = true;
            if (diagnostics_.empty() || diagnostics_.back().text != cpu_.last_fault()) {
                diagnostic(cpu_.last_fault());
            }
        }
        if (reset_requested_) {
            reset(requested_kind_);
        }
    }
}

void Stm32F407::step_instruction()
{
    if (reset_requested_) {
        reset(requested_kind_);
    }
    if (held()) {
        return;
    }
    cpu_.step_instruction();
    run_timers();
    if (reset_requested_) {
        reset(requested_kind_);
    }
}

void Stm32F407::set_pin_input(int port, int pin, bool level)
{
    if (port >= 0 && port < num_ports && pin >= 0 && pin < 16) {
        gpio_[static_cast<std::size_t>(port)]->set_input(pin, level);
    }
}

void Stm32F407::set_pin_input(int port, int pin, bool level, std::uint64_t changed_ns)
{
    input_changed_ns_ = changed_ns;
    set_pin_input(port, pin, level);
    input_changed_ns_.reset();
}

void Stm32F407::uart_receive(int usart, std::uint8_t value)
{
    if (usart >= 1 && usart <= num_usarts) {
        usart_[static_cast<std::size_t>(usart - 1)]->receive(value);
    }
}

std::vector<F4PinEvent> Stm32F407::take_pin_events()
{
    return std::exchange(pin_events_, {});
}

std::vector<UartByte> Stm32F407::take_uart_bytes()
{
    return std::exchange(uart_bytes_, {});
}

std::vector<Diagnostic> Stm32F407::take_diagnostics()
{
    return std::exchange(diagnostics_, {});
}

F4PinConfig Stm32F407::pin_config(int port, int pin) const
{
    F4PinConfig config = gpio_[static_cast<std::size_t>(port)]->config(pin);
    if (port == 0 && (pin == 4 || pin == 5)) {
        const DacPin& dac = dac_pins_[static_cast<std::size_t>(pin - 4)];
        config.dac = dac.enabled;
        config.dac_buffered = dac.buffered;
        config.dac_code = static_cast<std::uint16_t>(dac.code);
    }
    return config;
}

bool Stm32F407::pin_level(int port, int pin) const
{
    return gpio_[static_cast<std::size_t>(port)]->level(pin);
}

void Stm32F407::diagnostic(const std::string& text)
{
    std::string full = text;
    if (image_) {
        if (const ElfSymbol* function = image_->function_at(cpu_.pc())) {
            full += " (in " + function->name + ")";
        }
    }
    diagnostics_.push_back({time_ns(), cpu_.pc(), full});
}

void Stm32F407::pin_output_changed(int port, int pin)
{
    if (!gpio_[static_cast<std::size_t>(port)]) {
        return; // the port is being constructed
    }
    std::uint64_t at = stamping_ ? ns_at_cycles(stamp_cycles_) : time_ns();
    if (!pin_events_.empty()) {
        at = std::max(at, pin_events_.back().time_ns); // a timed event can be found an instruction late
    }
    pin_events_.push_back({at, port, pin, pin_config(port, pin)});
    if (pin_hook_) {
        pin_hook_(port, pin, at);
    }
}

void Stm32F407::pin_input_changed(int port, int pin, bool level)
{
    if (exti_ && syscfg_ && exti_port(pin) == port) {
        exti_->input(pin, level);
    }
    const std::uint64_t now = cpu_.cycles();
    const std::uint64_t at = input_changed_ns_ ? std::min(cycles_at_ns(*input_changed_ns_), now) : now;
    for (const auto& usart : usart_) {
        if (usart) {
            usart->pin_changed(port, pin, level, at);
        }
    }
    for (const auto& timer : timers_) {
        if (timer) {
            timer->pin_changed(port, pin, level);
        }
    }
}

int Stm32F407::exti_port(int line) const
{
    return syscfg_->exti_port(line);
}

void Stm32F407::exti_selection_changed()
{
    if (!exti_ || !syscfg_) {
        return;
    }
    for (int line = 0; line < 16; ++line) {
        const int port = exti_port(line);
        exti_->set_level(line, port < num_ports && gpio_[static_cast<std::size_t>(port)]
            && gpio_[static_cast<std::size_t>(port)]->level(line));
    }
}

void Stm32F407::af_output(int port, int pin, int source, int function, bool level)
{
    if (port < 0 || port >= num_ports || pin < 0 || pin >= 16 || !gpio_[static_cast<std::size_t>(port)]) {
        return;
    }
    gpio_[static_cast<std::size_t>(port)]->set_af(pin, source, function, level);
}

void Stm32F407::routing_changed()
{
    for (const auto& usart : usart_) {
        if (usart) {
            usart->update();
        }
    }
    for (const auto& timer : timers_) {
        if (timer) {
            timer->remap_changed();
        }
    }
}

std::pair<int, int> Stm32F407::usart_pin(int number, bool transmit) const
{
    const UsartPins& u = usart_pins[number - 1];
    for (const auto& [port, pin] : transmit ? u.tx : u.rx) {
        if (port < 0) {
            break;
        }
        const Gpio* gpio = gpio_[static_cast<std::size_t>(port)].get();
        if (gpio && gpio->mode(pin) == 2 && gpio->function(pin) == static_cast<std::uint32_t>(u.function)) {
            return {port, pin};
        }
    }
    return {-1, 0};
}

std::uint32_t Stm32F407::usart_apb_divider(int number) const
{
    return rcc_->apb_divider(usart_pins[number - 1].apb2);
}

void Stm32F407::usart_line(int port, int pin, int number, bool level)
{
    af_output(port, pin, usart_source + number, usart_pins[number - 1].function, level);
}

void Stm32F407::usart_line_at(int port, int pin, int number, bool level, std::uint64_t cycle)
{
    stamping_ = true;
    stamp_cycles_ = cycle;
    usart_line(port, pin, number, level);
    stamping_ = false;
}

void Stm32F407::usart_byte(int number, std::uint8_t value, std::uint64_t cycle)
{
    uart_bytes_.push_back({ns_at_cycles(cycle), number, value});
}

// ---------------------------------------------------------------------------
// Timers

void Stm32F407::clocks_changed()
{
    for (const auto& timer : timers_) {
        if (timer) {
            timer->clocks_changed();
        }
    }
}

std::uint32_t Stm32F407::hclk_per_kernel_tick(bool apb2) const
{
    // RM0090 figure 21 with RCC_DCKCFGR.TIMPRE clear: the timer clock is PCLK
    // with APB divider 1, else twice PCLK.
    const std::uint32_t divider = rcc_ ? rcc_->apb_divider(apb2) : 1u;
    return divider == 1 ? 1u : divider / 2u;
}

std::pair<int, int> Stm32F407::timer_pin(int number, int channel, bool complementary) const
{
    const int key = channel + (complementary ? 4 : 0);
    const auto function = static_cast<std::uint32_t>(timer_function(number));
    for (const TimerPin& p : timer_pins) {
        if (p.timer == number && p.channel == key) {
            const Gpio* gpio = gpio_[p.port].get();
            if (gpio && gpio->mode(p.pin) == 2 && gpio->function(p.pin) == function) {
                return {p.port, p.pin};
            }
        }
    }
    return {-1, -1};
}

void Stm32F407::timer_irq(int number, int irq, bool level)
{
    std::uint32_t& sources = irq_sources_[static_cast<std::size_t>(irq)];
    const std::uint32_t bit = 1u << number;
    sources = level ? sources | bit : sources & ~bit;
    cpu_.set_irq_line(irq, sources != 0);
}

void Stm32F407::timer_output(int port, int pin, int source, bool level, std::uint64_t cycle)
{
    const bool nested = stamping_;
    const std::uint64_t outer = stamp_cycles_;
    stamping_ = true;
    stamp_cycles_ = cycle;
    af_output(port, pin, source, timer_function((source - 100) / 10), level);
    stamping_ = nested;
    stamp_cycles_ = outer;
}

void Stm32F407::timer_trigger(int number, int signal, std::uint64_t cycle)
{
    if (adc_) {
        adc_->timer_event(number, signal, cycle);
    }
    if (dac_ && signal == 0) {
        dac_->timer_event(number, cycle);
    }
}

// ---------------------------------------------------------------------------
// ADC and DAC

std::uint32_t Stm32F407::hclk_per_pclk2() const
{
    return rcc_->apb_divider(true);
}

double Stm32F407::adc_voltage(int adc, int channel, std::uint64_t cycle)
{
    // IN0..IN7: PA0..PA7, IN8, IN9: PB0, PB1, IN10..IN15: PC0..PC5. ADC3 has
    // PA0..PA3 and PC0..PC3 on this package (its other inputs are on port F).
    int port = 0;
    int pin = channel;
    if (channel >= 8 && channel < 10) {
        port = 1;
        pin = channel - 8;
    } else if (channel >= 10) {
        port = 2;
        pin = channel - 10;
    }
    if (adc == 3 && !(channel < 4 || (channel >= 10 && channel < 14))) {
        return 0.0;
    }
    if (analog_source_) {
        return analog_source_(port, pin, ns_at_cycles(cycle));
    }
    if (port == 0 && (pin == 4 || pin == 5) && dac_pins_[static_cast<std::size_t>(pin - 4)].enabled) {
        return vref * dac_pins_[static_cast<std::size_t>(pin - 4)].code / 4096.0;
    }
    return pin_level(port, pin) ? vref : 0.0;
}

void Stm32F407::dac_output(int channel, bool enabled, bool buffered, std::uint32_t code, std::uint64_t cycle)
{
    dac_pins_[static_cast<std::size_t>(channel)] = {enabled, buffered, code};
    const bool nested = stamping_;
    const std::uint64_t outer = stamp_cycles_;
    stamping_ = true;
    stamp_cycles_ = cycle;
    pin_output_changed(0, 4 + channel);
    stamping_ = nested;
    stamp_cycles_ = outer;
}

} // namespace simnodus::emu
