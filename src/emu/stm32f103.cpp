// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F103 devices. Register descriptions follow the reference manual RM0008
// (sections: RCC 7.3, flash interface PM0075, GPIO and AFIO 9, EXTI 10.3,
// USART 27.6).
#include "stm32f103.hpp"

#include "reg_device.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <map>
#include <utility>

namespace simnodus::emu {

namespace {

constexpr std::uint32_t hsi_hz = 8000000;
constexpr std::uint64_t ns_per_s = 1000000000ull;

// Multiplies `value` by `num` / `den` without overflow for simulation times.
std::uint64_t scale(std::uint64_t value, std::uint64_t num, std::uint64_t den, bool round_up)
{
    const std::uint64_t q = value / den;
    const std::uint64_t r = value % den;
    const std::uint64_t part = r * num;
    return q * num + part / den + (round_up && part % den ? 1 : 0);
}

} // namespace

// ---------------------------------------------------------------------------
// RCC (RM0008 7.3): ready flags follow the enables, SWS follows SW, HCLK drives
// the CPU and SysTick (ADR 0088).

class Stm32F103::Rcc : public RegDevice {
public:
    explicit Rcc(Stm32F103& m) : m_(m) { reset(); }
    const char* name() const override { return "RCC"; }

    void reset() override
    {
        cr_ = 0x83; // HSION, HSIRDY, HSITRIM 16
        cfgr_ = 0;
        cir_flags_ = 0;
        cir_ie_ = 0;
        apb2rstr_ = apb1rstr_ = 0;
        ahbenr_ = 0x14;
        apb2enr_ = apb1enr_ = 0;
        sws_ = 0;
        previous_ready_ = 4;
        update_clocks();
        m_.rtc_clock_changed(); // HSE / 128 stops with HSE
    }

    // The backup domain (BDCR) survives system resets.
    void reset_backup_domain() { bdcr_ = 0; }

    // RTCCLK (RM0008 7.2.8): RTCEN and a selected clock that is running.
    std::uint32_t rtc_clock_hz() const
    {
        if (!(bdcr_ & 0x8000u)) {
            return 0;
        }
        switch ((bdcr_ >> 8) & 3u) {
        case 1: return (bdcr_ & 1u) ? 32768u : 0u;                          // LSE
        case 2: return (csr_ & 1u) ? 40000u : 0u;                           // LSI
        case 3: return (cr_ & (1u << 16)) ? m_.options_.hse_hz / 128 : 0u; // HSE / 128
        default: return 0;
        }
    }

    std::uint32_t sysclk() const
    {
        switch (sws_) {
        case 1: return m_.options_.hse_hz;
        case 2: {
            const bool from_hse = (cfgr_ >> 16) & 1u;
            const std::uint32_t input = from_hse ? m_.options_.hse_hz / (((cfgr_ >> 17) & 1u) ? 2u : 1u) : hsi_hz / 2;
            const std::uint32_t mul = std::min<std::uint32_t>(((cfgr_ >> 18) & 15u) + 2u, 16u);
            return input * mul;
        }
        default: return hsi_hz;
        }
    }

    // APB divider (1, 2, 4, 8, 16): HCLK cycles per PCLK cycle.
    std::uint32_t apb_divider(bool apb2) const
    {
        const std::uint32_t ppre = (cfgr_ >> (apb2 ? 11 : 8)) & 7u;
        return ppre < 4 ? 1u : 2u << (ppre - 4);
    }

    // HCLK cycles per ADC clock: APB2 divider times ADCPRE (2, 4, 6, 8).
    std::uint32_t hclk_per_adc_clock() const
    {
        const std::uint32_t ppre2 = (cfgr_ >> 11) & 7u;
        const std::uint32_t apb2 = ppre2 < 4 ? 1u : 2u << (ppre2 - 4);
        return apb2 * 2u * (((cfgr_ >> 14) & 3u) + 1u);
    }

    // HCLK cycles per timer kernel clock tick (RM0008 figure 8: the timer
    // clock is PCLK with APB divider 1, else twice PCLK).
    std::uint32_t hclk_per_timer_tick(bool apb2) const
    {
        const std::uint32_t ppre = (cfgr_ >> (apb2 ? 11 : 8)) & 7u;
        return ppre < 4 ? 1u : (2u << (ppre - 4)) / 2u;
    }

    std::uint32_t hclk() const
    {
        const std::uint32_t hpre = (cfgr_ >> 4) & 15u;
        const unsigned shift = hpre < 8 ? 0 : (hpre < 12 ? hpre - 7 : hpre - 6);
        return sysclk() >> shift;
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: {
            std::uint32_t v = cr_ & ~((1u << 1) | (1u << 17) | (1u << 25));
            v |= (cr_ & 1u) ? (1u << 1) : 0;
            v |= (cr_ & (1u << 16)) ? (1u << 17) : 0;
            v |= (cr_ & (1u << 24)) ? (1u << 25) : 0;
            value = v;
            return true;
        }
        case 0x04: value = (cfgr_ & ~0xCu) | (sws_ << 2); return true;
        case 0x08: value = cir_flags_ | (cir_ie_ << 8); return true;
        case 0x0C: value = apb2rstr_; return true;
        case 0x10: value = apb1rstr_; return true;
        case 0x14: value = ahbenr_; return true;
        case 0x18: value = apb2enr_; return true;
        case 0x1C: value = apb1enr_; return true;
        case 0x20: value = bdcr_ | ((bdcr_ & 1u) ? 2u : 0u); return true;
        case 0x24: value = (csr_ & 1u) | ((csr_ & 1u) ? 2u : 0u) | m_.reset_flags_; return true;
        default: value = 0; return true;
        }
    }

    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        switch (offset) {
        case 0x00: cr_ = merge(cr_, value, mask) & 0x010DFFF9u; update_ready(); break;
        case 0x04: cfgr_ = merge(cfgr_, value, mask) & 0x077FFFF3u; switch_clock(); break;
        case 0x08:
            cir_ie_ = merge(cir_ie_ << 8, value, mask & 0x1F00u) >> 8;
            cir_flags_ &= ~((value & mask & 0x1F0000u) >> 16);
            break;
        case 0x0C: apb2rstr_ = merge(apb2rstr_, value, mask); break;
        case 0x10: apb1rstr_ = merge(apb1rstr_, value, mask); break;
        case 0x14: ahbenr_ = merge(ahbenr_, value, mask); break;
        case 0x18: apb2enr_ = merge(apb2enr_, value, mask); break;
        case 0x1C: apb1enr_ = merge(apb1enr_, value, mask); break;
        case 0x20: {
            const std::uint32_t old = bdcr_;
            bdcr_ = merge(bdcr_, value, mask) & 0x18305u;
            if ((bdcr_ & 0x10000u) && !(old & 0x10000u)) {
                bdcr_ = 0x10000u; // BDRST resets the whole backup domain, BDCR included
                m_.backup_domain_reset();
            }
            update_ready();
            break;
        }
        case 0x24:
            csr_ = merge(csr_, value, mask) & 1u;
            if (value & mask & (1u << 24)) {
                m_.reset_flags_ = 0; // RMVF
            }
            update_ready();
            break;
        default: break;
        }
        return true;
    }

private:
    bool ready(std::uint32_t source) const
    {
        switch (source) {
        case 0: return (cr_ & 1u) != 0;
        case 1: return (cr_ & (1u << 16)) != 0;
        case 2: return (cr_ & (1u << 24)) != 0;
        default: return false;
        }
    }

    void update_ready()
    {
        const std::uint32_t now = ((csr_ & 1u) ? 1u : 0u) | ((bdcr_ & 1u) ? 2u : 0u) | ((cr_ & 1u) ? 4u : 0u)
            | ((cr_ & (1u << 16)) ? 8u : 0u) | ((cr_ & (1u << 24)) ? 16u : 0u);
        cir_flags_ |= now & ~previous_ready_;
        previous_ready_ = now;
        m_.rtc_clock_changed();
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
        const std::uint32_t apb = cfgr_ & 0x3F00u;
        if (apb != applied_apb_) {
            applied_apb_ = apb;
            m_.apb_changed();
        }
        const std::uint32_t hz = hclk();
        if (hz != applied_) {
            applied_ = hz;
            m_.set_hclk(hz);
            const std::uint32_t sys = sysclk();
            const std::uint32_t needed = sys <= 24000000 ? 0 : (sys <= 48000000 ? 1 : 2);
            if (m_.flash_latency() < needed) {
                char text[200];
                std::snprintf(text, sizeof text,
                    "SYSCLK is %u Hz but FLASH_ACR has %u wait state(s); the STM32F103 needs %u above %s MHz. "
                    "A real chip may crash; set the flash latency before switching the clock.",
                    static_cast<unsigned>(sys), static_cast<unsigned>(m_.flash_latency()),
                    static_cast<unsigned>(needed), needed == 2 ? "48" : "24");
                m_.diagnostic(text);
            }
        }
    }

    Stm32F103& m_;
    std::uint32_t cr_ = 0, cfgr_ = 0, cir_flags_ = 0, cir_ie_ = 0, apb2rstr_ = 0, apb1rstr_ = 0;
    std::uint32_t ahbenr_ = 0, apb2enr_ = 0, apb1enr_ = 0, bdcr_ = 0, csr_ = 0;
    std::uint32_t sws_ = 0, previous_ready_ = 0, applied_ = 0, applied_apb_ = 0;
};

// ---------------------------------------------------------------------------
// Flash memory interface (PM0075): registers only; programming is not simulated.

class Stm32F103::Flash : public RegDevice {
public:
    explicit Flash(Stm32F103& m) : m_(m) { reset(); }
    const char* name() const override { return "flash interface"; }
    void reset() override
    {
        regs_.fill(0);
        regs_[0] = 0x30;           // ACR: prefetch enabled and active
        regs_[4] = 0x80;           // CR: LOCK
        regs_[7] = 0x03FFFFFCu;    // OBR
        regs_[8] = 0xFFFFFFFFu;    // WRPR
        update_cpu();
    }
    std::uint32_t latency() const { return regs_[0] & 7u; }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        const std::uint32_t index = offset / 4;
        value = index < regs_.size() ? regs_[index] : 0;
        if (index == 0) {
            value = (value & ~0x20u) | ((value & 0x10u) ? 0x20u : 0u); // PRFTBS follows PRFTBE
        }
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        const std::uint32_t index = offset / 4;
        if (index == 4 && (value & mask & 0x3u) && !warned_) {
            warned_ = true;
            m_.diagnostic("The firmware tried to program the flash memory, which SimNodus does not simulate.");
        }
        if (index < regs_.size() && index != 7 && index != 8) {
            regs_[index] = merge(regs_[index], value, mask);
        }
        if (index == 0) {
            update_cpu();
        }
        return true;
    }

private:
    void update_cpu() { m_.cpu_.set_flash_access(latency(), (regs_[0] & 0x10u) != 0); }

    Stm32F103& m_;
    std::array<std::uint32_t, 9> regs_{};
    bool warned_ = false;
};

// ---------------------------------------------------------------------------
// PWR: CR and CSR keep what is written (DBP for the backup domain).

class Stm32F103::Pwr : public RegDevice {
public:
    const char* name() const override { return "PWR"; }
    void reset() override { cr_ = csr_ = 0; }
    bool dbp() const { return (cr_ & 0x100u) != 0; }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        value = offset == 0 ? cr_ : (offset == 4 ? csr_ : 0);
        return true;
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        if (offset == 0) {
            cr_ = merge(cr_, value, mask) & 0x1FFu;
        } else if (offset == 4) {
            csr_ = merge(csr_, value, mask) & 0x100u;
        }
        return true;
    }

private:
    std::uint32_t cr_ = 0, csr_ = 0;
};

// ---------------------------------------------------------------------------
// GPIO port (RM0008 9.2).

class Stm32F103::Gpio : public RegDevice {
public:
    Gpio(Stm32F103& m, int port) : m_(m), port_(port)
    {
        std::snprintf(name_, sizeof name_, "GPIO%c", 'A' + port);
        reset();
    }
    const char* name() const override { return name_; }

    void reset() override
    {
        crl_ = crh_ = 0x44444444u;
        odr_ = 0;
        lckr_ = 0;
        af_level_ = 0;
        update();
    }

    std::uint32_t nibble(int pin) const
    {
        return ((pin < 8 ? crl_ : crh_) >> (4 * (pin & 7))) & 15u;
    }

    bool level(int pin) const { return (levels_ >> pin) & 1u; }

    PinConfig config(int pin) const
    {
        PinConfig c;
        const std::uint32_t n = nibble(pin);
        c.mode = static_cast<std::uint8_t>(n & 3u);
        c.cnf = static_cast<std::uint8_t>(n >> 2);
        c.output_level = level(pin);
        c.drive_high = ((n >> 2) >= 2 ? (af_level_ >> pin) : (odr_ >> pin)) & 1u;
        c.pull_up = (odr_ >> pin) & 1u;
        return c;
    }

    void set_input(int pin, bool high)
    {
        const std::uint32_t bit = 1u << pin;
        input_set_ |= bit;
        input_ = high ? (input_ | bit) : (input_ & ~bit);
        update();
    }

    void set_af(int pin, bool high)
    {
        const std::uint32_t bit = 1u << pin;
        af_level_ = high ? (af_level_ | bit) : (af_level_ & ~bit);
        update();
    }

    // Recomputes every pin level and reports changes. The levels follow from
    // the registers and the inputs alone, so nothing happens while those stay
    // as they were (a USART sets its TX level again on every status read).
    void update()
    {
        const std::array<std::uint32_t, 6> state{crl_, crh_, odr_, input_, input_set_, af_level_};
        if (updated_ && state == state_) {
            return;
        }
        state_ = state;
        updated_ = true;
        std::uint32_t levels = 0;
        std::uint32_t outputs = 0;
        for (int pin = 0; pin < 16; ++pin) {
            const std::uint32_t n = nibble(pin);
            const std::uint32_t bit = 1u << pin;
            const bool external = (input_set_ & bit) ? (input_ & bit) != 0
                                                      : ((n >> 2) == 2 && (odr_ & bit) != 0); // pull-up if not driven
            bool high;
            if ((n & 3u) == 0) {
                high = external;
            } else {
                outputs |= bit;
                const bool alternate = (n >> 2) >= 2;
                const bool open_drain = (n >> 2) & 1u;
                const bool driven = alternate ? (af_level_ & bit) != 0 : (odr_ & bit) != 0;
                high = open_drain && driven ? external : driven;
            }
            levels |= high ? bit : 0u;
        }
        const std::uint32_t changed = levels ^ levels_;
        const std::uint32_t was_outputs = outputs_;
        levels_ = levels;
        outputs_ = outputs;
        for (int pin = 0; pin < 16; ++pin) {
            const std::uint32_t bit = 1u << pin;
            // Configuration as the pin sees it: CRL/CRH nibble, the ODR bit (pull
            // or output level) and the alternate-function level.
            const std::uint8_t config = static_cast<std::uint8_t>(nibble(pin) | ((odr_ >> pin) & 1u) << 4
                | ((af_level_ >> pin) & 1u) << 5);
            const bool config_changed = config != configs_[static_cast<std::size_t>(pin)];
            configs_[static_cast<std::size_t>(pin)] = config;
            if (config_changed || (((changed & bit) || ((outputs ^ was_outputs) & bit)) && ((outputs | was_outputs) & bit))) {
                m_.pin_output_changed(port_, pin, (levels & bit) != 0);
            }
            if (changed & bit) {
                m_.pin_input_changed(port_, pin, (levels & bit) != 0);
            }
        }
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: value = crl_; return true;
        case 0x04: value = crh_; return true;
        case 0x08: value = levels_ & 0xFFFFu; return true;
        case 0x0C: value = odr_; return true;
        case 0x18: value = lckr_; return true;
        default: value = 0; return true; // BSRR, BRR read as 0
        }
    }

    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        switch (offset) {
        case 0x00: crl_ = merge(crl_, value, mask); break;
        case 0x04: crh_ = merge(crh_, value, mask); break;
        case 0x0C: odr_ = merge(odr_, value, mask) & 0xFFFFu; break;
        case 0x10: {
            const std::uint32_t v = value & mask;
            odr_ = (odr_ & ~(v >> 16)) | (v & 0xFFFFu); // set wins over reset
            break;
        }
        case 0x14: odr_ &= ~(value & mask & 0xFFFFu); break;
        case 0x18: lckr_ = merge(lckr_, value, mask) & 0x1FFFFu; break; // lock not enforced
        default: return true;
        }
        update();
        return true;
    }

private:
    Stm32F103& m_;
    int port_;
    char name_[8] = {};
    std::uint32_t crl_ = 0, crh_ = 0, odr_ = 0, lckr_ = 0;
    std::uint32_t input_ = 0, input_set_ = 0, af_level_ = 0;
    std::uint32_t levels_ = 0, outputs_ = 0;
    std::array<std::uint8_t, 16> configs_{};
    std::array<std::uint32_t, 6> state_{}; // what the last update() saw
    bool updated_ = false;
};

// ---------------------------------------------------------------------------
// AFIO (RM0008 9.4): EXTI port selection and remaps.

class Stm32F103::Afio : public RegDevice {
public:
    explicit Afio(Stm32F103& m) : m_(m) { reset(); }
    const char* name() const override { return "AFIO"; }
    void reset() override
    {
        evcr_ = mapr_ = mapr2_ = 0;
        exticr_.fill(0);
    }
    int exti_port(int line) const { return static_cast<int>((exticr_[line / 4] >> (4 * (line % 4))) & 15u); }
    std::uint32_t mapr() const { return mapr_; }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        switch (offset) {
        case 0x00: value = evcr_; return true;
        case 0x04: value = mapr_ & ~0x07000000u; return true; // SWJ_CFG is write-only
        case 0x08: case 0x0C: case 0x10: case 0x14: value = exticr_[(offset - 8) / 4]; return true;
        case 0x1C: value = mapr2_; return true;
        default: value = 0; return true;
        }
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        switch (offset) {
        case 0x00: evcr_ = merge(evcr_, value, mask) & 0xFFu; break;
        case 0x04: mapr_ = merge(mapr_, value, mask); m_.remap_changed(); break;
        case 0x08: case 0x0C: case 0x10: case 0x14: {
            std::uint32_t& reg = exticr_[(offset - 8) / 4];
            reg = merge(reg, value, mask) & 0xFFFFu;
            m_.exti_selection_changed();
            break;
        }
        case 0x1C: mapr2_ = merge(mapr2_, value, mask); break;
        default: break;
        }
        return true;
    }

private:
    Stm32F103& m_;
    std::uint32_t evcr_ = 0, mapr_ = 0, mapr2_ = 0;
    std::array<std::uint32_t, 4> exticr_{};
};

// ---------------------------------------------------------------------------
// EXTI (RM0008 10.3): lines 0-15 from the pins selected by AFIO, 16 PVD,
// 17 RTC alarm, 18 USB wakeup. A pending bit is set by a selected edge on a
// line whose interrupt is unmasked, or by SWIER.

class Stm32F103::Exti : public RegDevice {
public:
    explicit Exti(Stm32F103& m) : m_(m) { reset(); }
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
        constexpr std::uint32_t lines = 0x7FFFFu;
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
            cpu.set_irq_line(6 + line, (pr_ >> line) & 1u);
        }
        cpu.set_irq_line(23, (pr_ & 0x3E0u) != 0);
        cpu.set_irq_line(40, (pr_ & 0xFC00u) != 0);
        cpu.set_irq_line(1, (pr_ >> 16) & 1u);
        cpu.set_irq_line(41, (pr_ >> 17) & 1u);
        cpu.set_irq_line(42, (pr_ >> 18) & 1u);
    }

    Stm32F103& m_;
    std::uint32_t imr_ = 0, emr_ = 0, rtsr_ = 0, ftsr_ = 0, swier_ = 0, pr_ = 0, levels_ = 0;
};

// ---------------------------------------------------------------------------
// Peripherals that are not simulated yet: registers keep what is written.

class Stm32F103::Stub : public RegDevice {
public:
    Stub(Stm32F103& m, const char* name) : m_(m), name_(name) {}
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
        if (!reported_) {
            reported_ = true;
            m_.diagnostic(std::string(name_) + " is not simulated yet: its registers keep what the firmware writes, "
                "but the peripheral does nothing.");
        }
    }

    Stm32F103& m_;
    const char* name_;
    std::map<std::uint32_t, std::uint32_t> regs_;
    bool reported_ = false;
};

// DWT: CYCCNT counts core cycles while CTRL.CYCCNTENA is set.
class Stm32F103::Dwt : public RegDevice {
public:
    explicit Dwt(Stm32F103& m) : m_(m) {}
    const char* name() const override { return "DWT"; }
    void reset() override
    {
        ctrl_ = 0x40000000u; // NUMCOMP = 4
        count_ = 0;
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        if (offset == 0) {
            value = ctrl_;
        } else if (offset == 4) {
            value = static_cast<std::uint32_t>(current());
        } else {
            value = 0;
        }
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
    Stm32F103& m_;
    std::uint32_t ctrl_ = 0x40000000u;
    std::uint64_t count_ = 0;
    std::uint64_t since_ = 0;
};

// DBGMCU: IDCODE reads 0 without a debugger (STM32F10xx errata, to be
// confirmed on the owner's board); CR keeps what is written.
class Stm32F103::Dbgmcu : public RegDevice {
public:
    const char* name() const override { return "DBGMCU"; }
    void reset() override { cr_ = 0; }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        value = offset == 4 ? cr_ : 0;
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
class Stm32F103::Zero : public Device {
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

Stm32F103::Stm32F103(MachineOptions options) : options_(options), cpu_(bus_)
{
    cpu_.set_timing(options_.cortex_m3_timing ? Cpu::Timing::CortexM3 : Cpu::Timing::OneCyclePerInstruction);
    bus_.add_memory("SRAM", 0x20000000u, options_.sram_size, true); // first: checked first on every access
    bus_.add_memory("flash", 0x08000000u, options_.flash_size, false);
    bus_.add_alias("flash (boot alias)", 0x00000000u, "flash");
    bus_.add_memory("system memory", 0x1FFFF000u, 0x810u, false);
    bus_.add_bit_band(0x22000000u, options_.sram_size * 32u, 0x20000000u);
    bus_.add_bit_band(0x42000000u, 0x02000000u, 0x40000000u);

    // Flash size (KiB) and unique device ID in system memory; option bytes.
    const std::uint16_t flash_kib = static_cast<std::uint16_t>(options_.flash_size / 1024);
    bus_.load(0x1FFFF7E0u, reinterpret_cast<const std::uint8_t*>(&flash_kib), 2);
    const std::uint8_t uid[12] = {0x53, 0x4E, 0x01, 0x00, 0x10, 0x26, 0x00, 0x00, 0x15, 0x10, 0x26, 0x00};
    bus_.load(0x1FFFF7E8u, uid, sizeof uid);
    const std::uint8_t option_bytes[16] = {0xA5, 0x5A, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
        0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    bus_.load(0x1FFFF800u, option_bytes, sizeof option_bytes);

    rcc_ = std::make_unique<Rcc>(*this);
    flash_ = std::make_unique<Flash>(*this);
    pwr_ = std::make_unique<Pwr>();
    afio_ = std::make_unique<Afio>(*this);
    exti_ = std::make_unique<Exti>(*this);
    for (int port = 0; port < num_ports; ++port) {
        gpio_[static_cast<std::size_t>(port)] = std::make_unique<Gpio>(*this, port);
        bus_.add_device(0x40010800u + 0x400u * static_cast<std::uint32_t>(port), 0x400, gpio_[static_cast<std::size_t>(port)].get());
    }
    usart_options_ = {options_.uart_frames, options_.uart_rx_lag_ns, options_.uart_rx_pins, options_.uart_rx_min_bit_ns,
        options_.exact_input_pins};
    usart_[0] = std::make_unique<Stm32Usart>(static_cast<UsartHost&>(*this), usart_options_, 1, 37);
    usart_[1] = std::make_unique<Stm32Usart>(static_cast<UsartHost&>(*this), usart_options_, 2, 38);
    usart_[2] = std::make_unique<Stm32Usart>(static_cast<UsartHost&>(*this), usart_options_, 3, 39);
    bus_.add_device(0x40021000u, 0x400, rcc_.get());
    bus_.add_device(0x40022000u, 0x400, flash_.get());
    bus_.add_device(0x40007000u, 0x400, pwr_.get());
    bus_.add_device(0x40010000u, 0x400, afio_.get());
    bus_.add_device(0x40010400u, 0x400, exti_.get());
    bus_.add_device(0x40013800u, 0x400, usart_[0].get());
    bus_.add_device(0x40004400u, 0x400, usart_[1].get());
    bus_.add_device(0x40004800u, 0x400, usart_[2].get());
    for (int n = 1; n <= 4; ++n) {
        timers_[static_cast<std::size_t>(n - 1)] = std::make_unique<Stm32F1Timer>(static_cast<TimerHost&>(*this), n);
    }
    bus_.add_device(0x40012C00u, 0x400, timers_[0].get());
    bus_.add_device(0x40000000u, 0x400, timers_[1].get());
    bus_.add_device(0x40000400u, 0x400, timers_[2].get());
    bus_.add_device(0x40000800u, 0x400, timers_[3].get());
    for (int n = 1; n <= 2; ++n) {
        adcs_[static_cast<std::size_t>(n - 1)] = std::make_unique<Stm32F1Adc>(static_cast<AdcHost&>(*this), n);
    }
    dma_ = std::make_unique<Stm32F1Dma>(static_cast<DmaHost&>(*this));
    bus_.add_device(0x40020000u, 0x400, dma_.get());
    bus_.add_device(0x40012400u, 0x400, adcs_[0].get());
    bus_.add_device(0x40012800u, 0x400, adcs_[1].get());
    for (const auto& timer : timers_) {
        timed_.push_back(timer.get());
    }
    for (const auto& adc : adcs_) {
        timed_.push_back(adc.get());
    }
    for (const auto& usart : usart_) {
        timed_.push_back(usart.get());
    }
    for (int n = 1; n <= 2; ++n) {
        spi_[static_cast<std::size_t>(n - 1)] = std::make_unique<Stm32F1Spi>(static_cast<SpiHost&>(*this), n);
        timed_.push_back(spi_[static_cast<std::size_t>(n - 1)].get());
    }
    for (int n = 1; n <= 2; ++n) {
        i2c_[static_cast<std::size_t>(n - 1)] = std::make_unique<Stm32F1I2c>(static_cast<I2cHost&>(*this), n);
        timed_.push_back(i2c_[static_cast<std::size_t>(n - 1)].get());
    }
    bus_.add_device(0x40005400u, 0x400, i2c_[0].get());
    bus_.add_device(0x40005800u, 0x400, i2c_[1].get());
    bus_.add_device(0x40013000u, 0x400, spi_[0].get());
    bus_.add_device(0x40003800u, 0x400, spi_[1].get());
    crc_ = std::make_unique<Stm32F1Crc>();
    iwdg_ = std::make_unique<Stm32F1Iwdg>(static_cast<SystemHost&>(*this));
    wwdg_ = std::make_unique<Stm32F1Wwdg>(static_cast<SystemHost&>(*this));
    rtc_ = std::make_unique<Stm32F1Rtc>(static_cast<SystemHost&>(*this));
    bkp_ = std::make_unique<Stm32F1Bkp>(static_cast<SystemHost&>(*this));
    bus_.add_device(0x40023000u, 0x400, crc_.get());
    bus_.add_device(0x40003000u, 0x400, iwdg_.get());
    bus_.add_device(0x40002C00u, 0x400, wwdg_.get());
    bus_.add_device(0x40002800u, 0x400, rtc_.get());
    bus_.add_device(0x40006C00u, 0x400, bkp_.get());
    timed_.push_back(iwdg_.get());
    timed_.push_back(wwdg_.get());
    timed_.push_back(rtc_.get());

    struct StubBlock {
        std::uint32_t base;
        const char* name;
    };
    static const StubBlock blocks[] = {
        {0x40005C00u, "USB"}, {0x40006000u, "USB/CAN SRAM"}, {0x40006400u, "CAN"},
    };
    for (const auto& block : blocks) {
        stubs_.push_back(std::make_unique<Stub>(*this, block.name));
        bus_.add_device(block.base, 0x400, stubs_.back().get());
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

Stm32F103::~Stm32F103() = default;

bool Stm32F103::load(const ElfImage& image, std::string& error)
{
    // Power-on first (it clears SRAM), then the image, then the core reads its vectors.
    image_ = std::make_unique<ElfImage>(image);
    reset(ResetKind::PowerOn);
    take_diagnostics();
    for (const auto& segment : image.segments) {
        if (!segment.data.empty()
            && !bus_.load(segment.physical, segment.data.data(), static_cast<std::uint32_t>(segment.data.size()))) {
            char text[200];
            std::snprintf(text, sizeof text,
                "The firmware does not fit the STM32F103: %u bytes at 0x%08X are outside its flash (%u KiB) and SRAM (%u KiB).",
                static_cast<unsigned>(segment.data.size()), static_cast<unsigned>(segment.physical),
                static_cast<unsigned>(options_.flash_size / 1024), static_cast<unsigned>(options_.sram_size / 1024));
            error = text;
            return false;
        }
        if (segment.memory_size > segment.data.size()) {
            bus_.fill(segment.physical + static_cast<std::uint32_t>(segment.data.size()), 0,
                segment.memory_size - static_cast<std::uint32_t>(segment.data.size()));
        }
    }
    cpu_.reset();
    if (cpu_.state() == CoreState::Lockup) {
        diagnostic(cpu_.last_fault());
    }
    return true;
}

void Stm32F103::reset(ResetKind kind)
{
    switch (kind) {
    case ResetKind::PowerOn:
        reset_flags_ = 0x0C000000u; // PORRSTF | PINRSTF
        rcc_->reset_backup_domain();
        backup_domain_reset();
        if (std::uint8_t* sram = bus_.memory_at(0x20000000u, options_.sram_size)) {
            std::fill(sram, sram + options_.sram_size, std::uint8_t{0});
        }
        break;
    case ResetKind::Pin:
        reset_flags_ |= 0x04000000u; // PINRSTF
        break;
    case ResetKind::Software:
        reset_flags_ |= 0x14000000u; // SFTRSTF | PINRSTF (the reset also drives NRST low)
        break;
    case ResetKind::IndependentWatchdog:
        reset_flags_ |= 0x24000000u; // IWDGRSTF | PINRSTF
        break;
    case ResetKind::WindowWatchdog:
        reset_flags_ |= 0x44000000u; // WWDGRSTF | PINRSTF
        break;
    }
    reset_requested_ = false;
    for (auto& sources : af_high_) {
        sources.clear();
    }
    bus_.reset_devices();
    for (auto& gpio : gpio_) {
        gpio->update();
    }
    for (int line = 0; line < 16; ++line) {
        exti_->set_level(line, gpio_[static_cast<std::size_t>(exti_port(line) % num_ports)]->level(line));
    }
    cpu_.reset();
    if (cpu_.state() == CoreState::Lockup) {
        diagnostic(cpu_.last_fault());
    }
}

std::uint64_t Stm32F103::time_ns() const
{
    return base_ns_ + scale(cpu_.cycles() - base_cycles_, ns_per_s, hclk_, false);
}

std::uint64_t Stm32F103::cycles_at_ns(std::uint64_t ns) const
{
    if (ns <= base_ns_) {
        return base_cycles_;
    }
    return base_cycles_ + scale(ns - base_ns_, hclk_, ns_per_s, true);
}

std::uint64_t Stm32F103::ns_at_cycles(std::uint64_t cycles) const
{
    return base_ns_ + (cycles > base_cycles_ ? scale(cycles - base_cycles_, ns_per_s, hclk_, false) : 0);
}

void Stm32F103::set_hclk(std::uint32_t hz)
{
    if (hz == 0 || hz == hclk_) {
        return;
    }
    base_ns_ = time_ns();
    base_cycles_ = cpu_.cycles();
    hclk_ = hz;
    cpu_.request_stop(); // the run loop recomputes its cycle target
}

void Stm32F103::run_until_ns(std::uint64_t target_ns)
{
    bool reported_lockup = false;
    while (time_ns() < target_ns) {
        if (reset_requested_) {
            reset(requested_kind_);
        }
        // The core stops at the next timer event, which then runs at its cycle.
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

void Stm32F103::step_instruction()
{
    if (reset_requested_) {
        reset(requested_kind_);
    }
    cpu_.step_instruction();
    run_timers();
    if (reset_requested_) {
        reset(requested_kind_);
    }
}

void Stm32F103::set_pin_input(int port, int pin, bool level)
{
    if (port >= 0 && port < num_ports && pin >= 0 && pin < 16) {
        gpio_[static_cast<std::size_t>(port)]->set_input(pin, level);
    }
}

void Stm32F103::set_pin_input(int port, int pin, bool level, std::uint64_t changed_ns)
{
    input_changed_ns_ = changed_ns;
    set_pin_input(port, pin, level);
    input_changed_ns_.reset();
}

void Stm32F103::uart_receive(int usart, std::uint8_t value)
{
    if (usart >= 1 && usart <= 3) {
        usart_[static_cast<std::size_t>(usart - 1)]->receive(value);
    }
}

std::vector<PinEvent> Stm32F103::take_pin_events()
{
    return std::exchange(pin_events_, {});
}

std::vector<UartByte> Stm32F103::take_uart_bytes()
{
    return std::exchange(uart_bytes_, {});
}

std::vector<Diagnostic> Stm32F103::take_diagnostics()
{
    return std::exchange(diagnostics_, {});
}

PinConfig Stm32F103::pin_config(int port, int pin) const
{
    return gpio_[static_cast<std::size_t>(port)]->config(pin);
}

bool Stm32F103::pin_level(int port, int pin) const
{
    return gpio_[static_cast<std::size_t>(port)]->level(pin);
}

void Stm32F103::diagnostic(const std::string& text)
{
    std::string full = text;
    if (image_) {
        if (const ElfSymbol* function = image_->function_at(cpu_.pc())) {
            full += " (in " + function->name + ")";
        }
    }
    diagnostics_.push_back({time_ns(), cpu_.pc(), full});
}

void Stm32F103::pin_output_changed(int port, int pin, bool level)
{
    if (!gpio_[static_cast<std::size_t>(port)]) {
        return; // the port is being constructed
    }
    const PinConfig c = gpio_[static_cast<std::size_t>(port)]->config(pin);
    std::uint64_t at = stamping_ ? ns_at_cycles(stamp_cycles_) : time_ns();
    if (!pin_events_.empty()) {
        at = std::max(at, pin_events_.back().time_ns); // a timer event can be found an instruction late
    }
    pin_events_.push_back({at, port, pin, level, c.mode, c.cnf, c.drive_high, c.pull_up});
    if (pin_hook_) {
        pin_hook_(port, pin, at);
    }
}

void Stm32F103::pin_input_changed(int port, int pin, bool level)
{
    if (exti_ && exti_port(pin) == port) {
        exti_->input(pin, level);
    }
    for (const auto& timer : timers_) {
        if (timer) {
            timer->pin_changed(port, pin, level);
        }
    }
    for (const auto& i2c : i2c_) {
        if (i2c) {
            i2c->pin_changed(port, pin, level);
        }
    }
    const std::uint64_t now = cpu_.cycles();
    const std::uint64_t at = input_changed_ns_ ? std::min(cycles_at_ns(*input_changed_ns_), now) : now;
    for (const auto& usart : usart_) {
        if (usart) {
            usart->pin_changed(port, pin, level, at);
        }
    }
}

void Stm32F103::uart_transmit(int usart, std::uint8_t value, std::uint64_t cycle)
{
    uart_bytes_.push_back({ns_at_cycles(cycle), usart, value});
}

void Stm32F103::af_output(int port, int pin, int source, bool level)
{
    if (port < 0 || port >= num_ports || pin < 0 || pin >= 16) {
        return;
    }
    auto& sources = af_high_[static_cast<std::size_t>(port * 16 + pin)];
    const auto it = std::find(sources.begin(), sources.end(), source);
    if (level && it == sources.end()) {
        sources.push_back(source);
    } else if (!level && it != sources.end()) {
        sources.erase(it);
    }
    gpio_[static_cast<std::size_t>(port)]->set_af(pin, !sources.empty());
}

void Stm32F103::timer_output(int port, int pin, int source, bool level, std::uint64_t cycle)
{
    stamping_ = true;
    stamp_cycles_ = cycle;
    af_output(port, pin, source, level);
    stamping_ = false;
}

// UsartHost: where the USARTs' pins are (AFIO remaps), their bus clock and
// their DMA1 channels (RM0008 table 78: TX 4, 7, 2; RX 5, 6, 3).
std::uint32_t Stm32F103::usart_apb_divider(int number) const
{
    return rcc_->apb_divider(number == 1);
}

std::pair<int, int> Stm32F103::usart_rx_pin(int number) const
{
    const std::uint32_t mapr = afio_->mapr();
    switch (number) {
    case 1: return (mapr & 4u) ? std::pair{1, 7} : std::pair{0, 10};
    case 2: return (mapr & 8u) ? std::pair{3, 6} : std::pair{0, 3};
    default:
        switch ((mapr >> 4) & 3u) {
        case 1: return {2, 11};
        case 3: return {3, 9};
        default: return {1, 11};
        }
    }
}

std::pair<int, int> Stm32F103::usart_tx_pin(int number) const
{
    const std::uint32_t mapr = afio_->mapr();
    switch (number) {
    case 1: return (mapr & 4u) ? std::pair{1, 6} : std::pair{0, 9};
    case 2: return (mapr & 8u) ? std::pair{3, 5} : std::pair{0, 2};
    default:
        switch ((mapr >> 4) & 3u) {
        case 1: return {2, 10};
        case 3: return {3, 8};
        default: return {1, 10};
        }
    }
}

bool Stm32F103::usart_dma(int number, bool transmit)
{
    static const int tx[3] = {4, 7, 2};
    static const int rx[3] = {5, 6, 3};
    return dma_ && dma_->request((transmit ? tx : rx)[number - 1]);
}

std::uint32_t Stm32F103::hclk_per_kernel_tick(bool apb2) const
{
    return rcc_->hclk_per_timer_tick(apb2);
}

std::uint32_t Stm32F103::afio_mapr() const
{
    return afio_ ? afio_->mapr() : 0;
}

std::uint64_t Stm32F103::next_timer_event() const
{
    std::uint64_t next = no_event;
    for (const TimedDevice* device : timed_) {
        next = std::min(next, device->next_event());
    }
    return next;
}

void Stm32F103::run_timers()
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

std::uint32_t Stm32F103::hclk_per_adc_clock() const
{
    return rcc_->hclk_per_adc_clock();
}

double Stm32F103::adc_channel_voltage(int channel, std::uint64_t cycle)
{
    // IN0..IN7: PA0..PA7, IN8..IN9: PB0..PB1, IN10..IN15: PC0..PC5.
    int port = 0;
    int pin = channel;
    if (channel >= 8 && channel < 10) {
        port = 1;
        pin = channel - 8;
    } else if (channel >= 10) {
        port = 2;
        pin = channel - 10;
    }
    if (analog_source_) {
        return analog_source_(port, pin, ns_at_cycles(cycle));
    }
    return pin_level(port, pin) ? Stm32F1Adc::vdda : 0.0;
}

void Stm32F103::dma_channel_enabled(int channel)
{
    for (auto& usart : usart_) {
        if (usart) {
            usart->update(); // a transmitter waiting with DMAT set starts now
        }
    }
    for (auto& spi : spi_) {
        if (spi) {
            spi->dma_enabled();
        }
    }
    for (auto& i2c : i2c_) {
        if (i2c) {
            i2c->dma_enabled();
        }
    }
    (void)channel;
}

void Stm32F103::adc_irq_changed()
{
    const bool level = (adcs_[0] && adcs_[0]->irq()) || (adcs_[1] && adcs_[1]->irq());
    cpu_.set_irq_line(18, level); // ADC1_2
}

std::uint32_t Stm32F103::pclk1_hz() const
{
    return hclk_ / rcc_->apb_divider(false);
}

std::uint32_t Stm32F103::rtc_clock_hz() const
{
    return rcc_ ? rcc_->rtc_clock_hz() : 0;
}

bool Stm32F103::backup_writable() const
{
    return pwr_ && pwr_->dbp();
}

void Stm32F103::watchdog_reset(bool independent)
{
    bool& reported = independent ? reported_iwdg_ : reported_wwdg_;
    if (!reported) {
        reported = true;
        diagnostic(independent
                ? "The independent watchdog (IWDG) reset the chip: the firmware did not reload it in time. "
                  "Later watchdog resets are not reported."
                : "The window watchdog (WWDG) reset the chip. Later watchdog resets are not reported.");
    }
    reset_requested_ = true;
    requested_kind_ = independent ? ResetKind::IndependentWatchdog : ResetKind::WindowWatchdog;
    cpu_.request_stop();
}

void Stm32F103::backup_domain_reset()
{
    if (rtc_) {
        rtc_->backup_reset();
    }
    if (bkp_) {
        bkp_->backup_reset();
    }
}

void Stm32F103::rtc_clock_changed()
{
    if (rtc_) {
        rtc_->clock_changed();
    }
}

void Stm32F103::apb_changed()
{
    if (wwdg_) {
        wwdg_->clocks_changed();
    }
    for (const auto& timer : timers_) {
        if (timer) {
            timer->clocks_changed();
        }
    }
}

int Stm32F103::exti_port(int line) const
{
    return afio_ ? afio_->exti_port(line) : 0;
}

void Stm32F103::exti_input(int line, bool level)
{
    exti_->input(line, level);
}

std::uint32_t Stm32F103::hclk_per_pclk(bool apb2) const
{
    return rcc_->apb_divider(apb2);
}

void Stm32F103::spi_output(int port, int pin, int source, bool level, std::uint64_t cycle)
{
    stamping_ = true;
    stamp_cycles_ = cycle;
    af_output(port, pin, source, level);
    stamping_ = false;
}

void Stm32F103::remap_changed()
{
    for (const auto& spi : spi_) {
        if (spi) {
            spi->remap_changed();
        }
    }
    for (const auto& i2c : i2c_) {
        if (i2c) {
            i2c->remap_changed();
        }
    }
    for (auto& usart : usart_) {
        usart->update();
    }
    for (const auto& timer : timers_) {
        timer->remap_changed();
    }
}

void Stm32F103::exti_selection_changed()
{
    for (int line = 0; line < 16; ++line) {
        const int port = exti_port(line);
        if (port < num_ports) {
            exti_->set_level(line, gpio_[static_cast<std::size_t>(port)]->level(line));
        }
    }
}

std::uint32_t Stm32F103::sysclk_hz() const
{
    return rcc_->sysclk();
}

std::uint32_t Stm32F103::flash_latency() const
{
    return flash_ ? flash_->latency() : 0;
}

} // namespace simnodus::emu
