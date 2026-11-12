// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f4_analog.hpp"

#include <algorithm>
#include <cmath>

namespace simnodus::emu {

namespace {

// ADC register offsets inside one converter's block (RM0090 13.13).
enum : std::uint32_t {
    SR = 0x00, CR1 = 0x04, CR2 = 0x08, SMPR1 = 0x0C, SMPR2 = 0x10, JOFR1 = 0x14, HTR = 0x24, LTR = 0x28,
    SQR1 = 0x2C, SQR2 = 0x30, SQR3 = 0x34, JSQR = 0x38, JDR1 = 0x3C, DR = 0x4C,
    CSR = 0x300, CCR = 0x304, CDR = 0x308
};

constexpr std::uint32_t SR_AWD = 1u << 0, SR_EOC = 1u << 1, SR_JEOC = 1u << 2, SR_JSTRT = 1u << 3, SR_STRT = 1u << 4,
                        SR_OVR = 1u << 5;
constexpr std::uint32_t CR1_EOCIE = 1u << 5, CR1_AWDIE = 1u << 6, CR1_JEOCIE = 1u << 7, CR1_SCAN = 1u << 8,
                        CR1_AWDSGL = 1u << 9, CR1_JAUTO = 1u << 10, CR1_DISCEN = 1u << 11, CR1_JDISCEN = 1u << 12,
                        CR1_JAWDEN = 1u << 22, CR1_AWDEN = 1u << 23, CR1_OVRIE = 1u << 26;
constexpr std::uint32_t CR2_ADON = 1u << 0, CR2_CONT = 1u << 1, CR2_DMA = 1u << 8, CR2_EOCS = 1u << 10,
                        CR2_ALIGN = 1u << 11, CR2_JSWSTART = 1u << 22, CR2_SWSTART = 1u << 30;
constexpr std::uint32_t CCR_VBATE = 1u << 22, CCR_TSVREFE = 1u << 23;

// Sample times in ADC clock cycles.
constexpr std::uint32_t sample_cycles[8] = {3, 15, 28, 56, 84, 112, 144, 480};

constexpr double temperature_volts = 0.76; // V25, datasheet typical (25 °C)
constexpr double vrefint_volts = 1.21;

enum : std::uint32_t { F_DMA = 1u << 0, F_EXTI = 1u << 1, F_DISCONTINUOUS = 1u << 2, F_MULTI = 1u << 3 };

// The EXTSEL code of a timer event for the regular group (RM0090 table 69);
// signal 0 is TRGO, 1 to 4 a channel's reference rising. -1: none.
int regular_code(int timer, int signal)
{
    switch (timer * 10 + signal) {
    case 11: return 0;
    case 12: return 1;
    case 13: return 2;
    case 22: return 3;
    case 23: return 4;
    case 24: return 5;
    case 20: return 6;
    case 31: return 7;
    case 30: return 8;
    case 44: return 9;
    case 51: return 10;
    case 52: return 11;
    case 53: return 12;
    case 81: return 13;
    case 80: return 14;
    default: return -1;
    }
}

// The JEXTSEL code for the injected group (RM0090 table 70).
int injected_code(int timer, int signal)
{
    switch (timer * 10 + signal) {
    case 14: return 0;
    case 10: return 1;
    case 21: return 2;
    case 20: return 3;
    case 32: return 4;
    case 34: return 5;
    case 41: return 6;
    case 42: return 7;
    case 43: return 8;
    case 40: return 9;
    case 54: return 10;
    case 50: return 11;
    case 82: return 12;
    case 83: return 13;
    case 84: return 14;
    default: return -1;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// ADC

void Stm32F4Adc::reset()
{
    units_ = {};
    ccr_ = 0;
}

std::uint32_t Stm32F4Adc::code_of(double volts, int bits)
{
    const double full = static_cast<double>(1u << bits);
    return static_cast<std::uint32_t>(std::clamp(std::floor(volts / vref * full), 0.0, full - 1.0));
}

int Stm32F4Adc::bits(const Unit& u) const
{
    return 12 - 2 * static_cast<int>((u.cr1 >> 24) & 3u);
}

int Stm32F4Adc::regular_length(const Unit& u) const
{
    return (u.cr1 & CR1_SCAN) ? static_cast<int>((u.sqr1 >> 20) & 15u) + 1 : 1;
}

int Stm32F4Adc::injected_length(const Unit& u) const
{
    return (u.cr1 & CR1_SCAN) ? static_cast<int>((u.jsqr >> 20) & 3u) + 1 : 1;
}

int Stm32F4Adc::regular_channel(const Unit& u, int index) const
{
    // SQ1..SQ6 in SQR3, SQ7..SQ12 in SQR2, SQ13..SQ16 in SQR1; 5 bits each.
    const std::uint32_t reg = index < 6 ? u.sqr3 : (index < 12 ? u.sqr2 : u.sqr1);
    return static_cast<int>((reg >> (5 * (index % 6))) & 31u);
}

int Stm32F4Adc::injected_channel(const Unit& u, int index) const
{
    // With JL + 1 conversions the sequence is the last JL + 1 of JSQ1..JSQ4.
    const int jl = static_cast<int>((u.jsqr >> 20) & 3u);
    const int slot = (u.cr1 & CR1_SCAN) ? 3 - jl + index : 3;
    return static_cast<int>((u.jsqr >> (5 * slot)) & 31u);
}

void Stm32F4Adc::catch_up()
{
    if (!in_event_) {
        on_time(host_.analog_now_cycles());
    }
}

std::uint64_t Stm32F4Adc::next_event() const
{
    std::uint64_t next = no_event;
    for (const Unit& u : units_) {
        if (u.busy != Busy::None) {
            next = std::min(next, u.end_at);
        }
    }
    return next;
}

void Stm32F4Adc::on_time(std::uint64_t cycle)
{
    const bool nested = in_event_;
    in_event_ = true;
    for (;;) {
        int first = -1;
        std::uint64_t at = cycle;
        for (int i = 0; i < 3; ++i) {
            const Unit& u = unit(i);
            if (u.busy != Busy::None && u.end_at <= at) {
                at = u.end_at;
                first = i;
            }
        }
        if (first < 0) {
            break;
        }
        finish_step(first);
    }
    in_event_ = nested;
}

void Stm32F4Adc::schedule_next(int index, std::uint64_t at)
{
    Unit& u = unit(index);
    if (u.injected_active) {
        u.busy = Busy::Injected;
        u.channel = injected_channel(u, u.injected_index);
    } else if (u.regular_active) {
        u.busy = Busy::Regular;
        u.channel = regular_channel(u, u.regular_index);
    } else {
        u.busy = Busy::None;
        host_.analog_schedule_changed();
        return;
    }
    const std::uint64_t ratio = std::uint64_t{host_.hclk_per_pclk2()} * 2u * (((ccr_ >> 16) & 3u) + 1u);
    const std::uint32_t smp = u.channel < 10 ? (u.smpr2 >> (3 * u.channel)) & 7u
                                             : (u.smpr1 >> (3 * (u.channel - 10))) & 7u;
    u.sample_at = at + sample_cycles[smp] * ratio;
    u.end_at = at + (sample_cycles[smp] + static_cast<std::uint32_t>(bits(u))) * ratio;
    host_.analog_schedule_changed();
}

void Stm32F4Adc::finish_step(int index)
{
    Unit& u = unit(index);
    const std::uint64_t at = u.end_at;
    double volts = 0.0;
    if (u.channel < 16) {
        volts = host_.adc_voltage(index + 1, u.channel, u.sample_at);
    } else if (index == 0) { // the internal channels are on ADC1
        if (u.channel == 18 && (ccr_ & CCR_VBATE)) {
            volts = vref / 2.0; // VBAT / 2, with VBAT at the supply
        } else if (u.channel == 16 && (ccr_ & CCR_TSVREFE)) {
            volts = temperature_volts;
        } else if (u.channel == 17 && (ccr_ & CCR_TSVREFE)) {
            volts = vrefint_volts;
        }
    }
    const int n = bits(u);
    const std::uint32_t code = code_of(volts, n);
    const bool left = (u.cr2 & CR2_ALIGN) != 0;
    if (u.busy == Busy::Regular) {
        check_watchdog(u, u.channel, code, false);
        if ((u.cr2 & (CR2_DMA | CR2_EOCS)) && u.unread) {
            // Overrun: the new result is lost and the regular group stops until OVR is cleared.
            u.sr |= SR_OVR;
            u.regular_active = false;
            u.regular_index = 0;
        } else {
            u.dr = left ? code << (n == 6 ? 2 : 16 - n) : code;
            u.unread = true;
            const bool last = ++u.regular_index >= regular_length(u);
            if (last || (u.cr2 & CR2_EOCS)) {
                u.sr |= SR_EOC;
            }
            if (last) {
                u.regular_index = 0;
                if (!(u.cr2 & CR2_CONT)) {
                    u.regular_active = false;
                }
                if ((u.cr1 & CR1_JAUTO) && !u.injected_active) {
                    u.injected_active = true; // automatic injected conversion after the regular group
                    u.injected_index = 0;
                    u.sr |= SR_JSTRT;
                }
            }
        }
    } else {
        const auto rank = static_cast<std::size_t>(u.injected_index);
        const auto value = static_cast<std::int32_t>(code) - static_cast<std::int32_t>(u.jofr[rank]);
        u.jdr[rank] = static_cast<std::uint32_t>(left ? value * (1 << (n == 6 ? 1 : 15 - n)) : value) & 0xFFFFu;
        check_watchdog(u, u.channel, code, true);
        if (++u.injected_index >= injected_length(u)) {
            u.injected_index = 0;
            u.injected_active = false;
            u.sr |= SR_JEOC;
        }
    }
    update_irq();
    schedule_next(index, at);
}

void Stm32F4Adc::check_watchdog(Unit& u, int channel, std::uint32_t code, bool injected)
{
    if (!(u.cr1 & (injected ? CR1_JAWDEN : CR1_AWDEN))) {
        return;
    }
    if ((u.cr1 & CR1_AWDSGL) && channel != static_cast<int>(u.cr1 & 31u)) {
        return;
    }
    if (code > u.htr || code < u.ltr) {
        u.sr |= SR_AWD;
    }
}

void Stm32F4Adc::start_regular(int index, std::uint64_t at)
{
    Unit& u = unit(index);
    if (!(u.cr2 & CR2_ADON)) {
        return;
    }
    u.sr |= SR_STRT;
    u.regular_active = true;
    if (u.busy == Busy::None) {
        u.regular_index = 0;
        schedule_next(index, at);
    } else if (u.busy != Busy::Regular) {
        u.regular_index = 0;
    }
}

void Stm32F4Adc::start_injected(int index, std::uint64_t at)
{
    Unit& u = unit(index);
    if (!(u.cr2 & CR2_ADON)) {
        return;
    }
    u.sr |= SR_JSTRT;
    if (!u.injected_active) {
        u.injected_active = true;
        u.injected_index = 0;
    }
    if (u.busy == Busy::None) {
        schedule_next(index, at); // otherwise after the current conversion
    }
}

void Stm32F4Adc::timer_event(int timer, int signal, std::uint64_t cycle)
{
    const int regular = regular_code(timer, signal);
    const int injected = injected_code(timer, signal);
    if (regular < 0 && injected < 0) {
        return;
    }
    for (int i = 0; i < 3; ++i) {
        const Unit& u = unit(i);
        if (!(u.cr2 & CR2_ADON)) {
            continue;
        }
        // EXTEN and JEXTEN choose the edge of a trigger that here is an instant.
        if (regular >= 0 && ((u.cr2 >> 28) & 3u) != 0 && static_cast<int>((u.cr2 >> 24) & 15u) == regular) {
            start_regular(i, cycle);
        }
        if (injected >= 0 && ((u.cr2 >> 20) & 3u) != 0 && static_cast<int>((u.cr2 >> 16) & 15u) == injected) {
            start_injected(i, cycle);
        }
    }
}

void Stm32F4Adc::update_irq()
{
    bool level = false;
    for (const Unit& u : units_) {
        level = level || ((u.sr & SR_EOC) && (u.cr1 & CR1_EOCIE)) || ((u.sr & SR_JEOC) && (u.cr1 & CR1_JEOCIE))
            || ((u.sr & SR_AWD) && (u.cr1 & CR1_AWDIE)) || ((u.sr & SR_OVR) && (u.cr1 & CR1_OVRIE));
    }
    host_.adc_irq(level);
}

void Stm32F4Adc::report(std::uint32_t feature, const std::string& text)
{
    if (!(reported_ & feature)) {
        reported_ |= feature;
        host_.analog_diagnostic("ADC: " + text + " is not simulated yet.");
    }
}

bool Stm32F4Adc::read32(std::uint32_t offset, std::uint32_t& value)
{
    catch_up();
    value = 0;
    if (offset >= CSR) {
        if (offset == CSR) {
            for (int i = 0; i < 3; ++i) {
                value |= (unit(i).sr & 0x3Fu) << (8 * i);
            }
        } else if (offset == CCR) {
            value = ccr_;
        } else if (offset == CDR) {
            value = (unit(1).dr << 16) | (unit(0).dr & 0xFFFFu);
        }
        return true;
    }
    Unit& u = unit(static_cast<int>(offset >> 8));
    const std::uint32_t reg = offset & 0xFFu;
    switch (reg) {
    case SR: value = u.sr; break;
    case CR1: value = u.cr1; break;
    case CR2: value = u.cr2; break;
    case SMPR1: value = u.smpr1; break;
    case SMPR2: value = u.smpr2; break;
    case JOFR1: case JOFR1 + 4: case JOFR1 + 8: case JOFR1 + 12: value = u.jofr[(reg - JOFR1) / 4]; break;
    case HTR: value = u.htr; break;
    case LTR: value = u.ltr; break;
    case SQR1: value = u.sqr1; break;
    case SQR2: value = u.sqr2; break;
    case SQR3: value = u.sqr3; break;
    case JSQR: value = u.jsqr; break;
    case JDR1: case JDR1 + 4: case JDR1 + 8: case JDR1 + 12: value = u.jdr[(reg - JDR1) / 4]; break;
    case DR:
        if (!peeking()) {
            u.sr &= ~SR_EOC; // reading DR clears EOC
            u.unread = false;
            update_irq();
        }
        value = u.dr;
        break;
    default: break;
    }
    return true;
}

bool Stm32F4Adc::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    catch_up();
    if (offset >= CSR) {
        if (offset == CCR) {
            ccr_ = merge(ccr_, value, mask) & 0x00C3EF1Fu;
            if (ccr_ & 0x1Fu) {
                report(F_MULTI, "the multi-ADC modes");
            }
        }
        return true;
    }
    write_unit(static_cast<int>(offset >> 8), offset & 0xFFu, value, mask);
    return true;
}

void Stm32F4Adc::write_unit(int index, std::uint32_t reg, std::uint32_t value, std::uint32_t mask)
{
    Unit& u = unit(index);
    switch (reg) {
    case SR:
        u.sr &= (value | ~mask) | ~0x3Fu; // rc_w0
        update_irq();
        break;
    case CR1:
        u.cr1 = merge(u.cr1, value, mask) & 0x07C0FFFFu;
        if (u.cr1 & (CR1_DISCEN | CR1_JDISCEN)) {
            report(F_DISCONTINUOUS, "discontinuous mode");
        }
        update_irq();
        break;
    case CR2: {
        const std::uint32_t written = value & mask;
        u.cr2 = merge(u.cr2, value, mask) & 0x7F7F0F03u & ~(CR2_SWSTART | CR2_JSWSTART); // the starts act at once
        if (!(u.cr2 & CR2_ADON)) {
            u.busy = Busy::None; // powered down: conversions stop
            u.regular_active = u.injected_active = false;
            host_.analog_schedule_changed();
            break;
        }
        if (u.cr2 & CR2_DMA) {
            report(F_DMA, "DMA");
        }
        if ((((u.cr2 >> 28) & 3u) != 0 && ((u.cr2 >> 24) & 15u) == 15u)
            || (((u.cr2 >> 20) & 3u) != 0 && ((u.cr2 >> 16) & 15u) == 15u)) {
            report(F_EXTI, "starting conversions from an EXTI line");
        }
        if (written & CR2_SWSTART) {
            start_regular(index, host_.analog_now_cycles());
        }
        if (written & CR2_JSWSTART) {
            start_injected(index, host_.analog_now_cycles());
        }
        update_irq();
        break;
    }
    case SMPR1: u.smpr1 = merge(u.smpr1, value, mask) & 0x07FFFFFFu; break;
    case SMPR2: u.smpr2 = merge(u.smpr2, value, mask) & 0x3FFFFFFFu; break;
    case JOFR1: case JOFR1 + 4: case JOFR1 + 8: case JOFR1 + 12: {
        auto& r = u.jofr[(reg - JOFR1) / 4];
        r = merge(r, value, mask) & 0xFFFu;
        break;
    }
    case HTR: u.htr = merge(u.htr, value, mask) & 0xFFFu; break;
    case LTR: u.ltr = merge(u.ltr, value, mask) & 0xFFFu; break;
    case SQR1: u.sqr1 = merge(u.sqr1, value, mask) & 0x00FFFFFFu; break;
    case SQR2: u.sqr2 = merge(u.sqr2, value, mask) & 0x3FFFFFFFu; break;
    case SQR3: u.sqr3 = merge(u.sqr3, value, mask) & 0x3FFFFFFFu; break;
    case JSQR: u.jsqr = merge(u.jsqr, value, mask) & 0x003FFFFFu; break;
    default: break; // JDRx and DR are read-only
    }
}

// ---------------------------------------------------------------------------
// DAC

namespace {

enum : std::uint32_t {
    DAC_CR = 0x00, DAC_SWTRIGR = 0x04, DHR12R1 = 0x08, DHR12L1 = 0x0C, DHR8R1 = 0x10, DHR12R2 = 0x14,
    DHR12L2 = 0x18, DHR8R2 = 0x1C, DHR12RD = 0x20, DHR12LD = 0x24, DHR8RD = 0x28, DOR1 = 0x2C, DOR2 = 0x30
};

constexpr std::uint32_t DAC_EN = 1u << 0, DAC_BOFF = 1u << 1, DAC_TEN = 1u << 2, DAC_DMAEN = 1u << 12;

enum : std::uint32_t { F_NOISE = 1u << 0, F_DAC_DMA = 1u << 1 };

} // namespace

void Stm32F4Dac::reset()
{
    cr_ = 0;
    dhr_ = {};
    dor_ = {};
    wave_ = {};
    rising_ = {true, true};
    // The machine releases the pins at reset.
}

void Stm32F4Dac::report(std::uint32_t feature, const std::string& text)
{
    if (!(reported_ & feature)) {
        reported_ |= feature;
        host_.analog_diagnostic("DAC: " + text + " is not simulated yet.");
    }
}

void Stm32F4Dac::publish(int channel, std::uint64_t cycle)
{
    const std::uint32_t c = control(channel);
    host_.dac_output(channel, (c & DAC_EN) != 0, !(c & DAC_BOFF), dor_[static_cast<std::size_t>(channel)], cycle);
}

void Stm32F4Dac::load(int channel, std::uint64_t cycle)
{
    const auto i = static_cast<std::size_t>(channel);
    const std::uint32_t c = control(channel);
    std::uint32_t value = dhr_[i];
    const std::uint32_t wave = (c >> 6) & 3u;
    if ((c & DAC_TEN) && wave >= 2) { // triangle: the counter is added to the data
        value = (value + wave_[i]) & 0xFFFu;
        const std::uint32_t mamp = (c >> 8) & 15u;
        const std::uint32_t peak = mamp >= 11 ? 4095u : (2u << mamp) - 1u;
        if (rising_[i] && wave_[i] >= peak) {
            rising_[i] = false;
        } else if (!rising_[i] && wave_[i] == 0) {
            rising_[i] = true;
        }
        wave_[i] = rising_[i] ? std::min(wave_[i] + 1, peak) : wave_[i] - (wave_[i] ? 1u : 0u);
    } else if ((c & DAC_TEN) && wave == 1) {
        report(F_NOISE, "the noise generator");
    }
    if (value != dor_[i]) {
        dor_[i] = value;
        if (c & DAC_EN) {
            publish(channel, cycle);
        }
    }
}

void Stm32F4Dac::trigger(int channel, std::uint64_t cycle)
{
    const std::uint32_t c = control(channel);
    if ((c & DAC_EN) && (c & DAC_TEN)) {
        load(channel, cycle);
    }
}

void Stm32F4Dac::timer_event(int timer, std::uint64_t cycle)
{
    // TSEL: TIM6, TIM8, TIM7, TIM5, TIM2, TIM4 (then EXTI line 9 and the software trigger).
    static const int codes[9] = {-1, -1, 4, -1, 5, 3, 0, 2, 1};
    const int code = timer >= 0 && timer < 9 ? codes[timer] : -1;
    if (code < 0) {
        return;
    }
    for (int channel = 0; channel < 2; ++channel) {
        if (static_cast<int>((control(channel) >> 3) & 7u) == code) {
            trigger(channel, cycle);
        }
    }
}

bool Stm32F4Dac::read32(std::uint32_t offset, std::uint32_t& value)
{
    switch (offset) {
    case DAC_CR: value = cr_; break;
    case DHR12R1: value = dhr_[0]; break;
    case DHR12L1: value = dhr_[0] << 4; break;
    case DHR8R1: value = dhr_[0] >> 4; break;
    case DHR12R2: value = dhr_[1]; break;
    case DHR12L2: value = dhr_[1] << 4; break;
    case DHR8R2: value = dhr_[1] >> 4; break;
    case DHR12RD: value = dhr_[0] | dhr_[1] << 16; break;
    case DHR12LD: value = dhr_[0] << 4 | dhr_[1] << 20; break;
    case DHR8RD: value = dhr_[0] >> 4 | (dhr_[1] >> 4) << 8; break;
    case DOR1: value = dor_[0]; break;
    case DOR2: value = dor_[1]; break;
    default: value = 0; break; // SWTRIGR reads 0; SR: no underrun without DMA
    }
    return true;
}

bool Stm32F4Dac::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    const std::uint64_t now = host_.analog_now_cycles();
    std::uint32_t old = 0;
    read32(offset, old);
    const std::uint32_t v = merge(old, value, mask);
    bool data[2] = {false, false};
    switch (offset) {
    case DAC_CR: {
        const std::uint32_t before = cr_;
        cr_ = v & 0x3FFF3FFFu;
        for (int channel = 0; channel < 2; ++channel) {
            const std::uint32_t changed = ((before ^ cr_) >> (16 * channel)) & 0xFFFFu;
            if (control(channel) & DAC_DMAEN) {
                report(F_DAC_DMA, "DMA");
            }
            if (changed & (DAC_EN | DAC_BOFF)) {
                const auto i = static_cast<std::size_t>(channel);
                if ((control(channel) & DAC_EN) && !(control(channel) & DAC_TEN)) {
                    dor_[i] = dhr_[i];
                }
                if (!(control(channel) & DAC_EN)) {
                    dor_[i] = 0;
                    wave_[i] = 0;
                    rising_[i] = true;
                }
                publish(channel, now);
            }
        }
        return true;
    }
    case DAC_SWTRIGR:
        for (int channel = 0; channel < 2; ++channel) {
            if ((value & mask & (1u << channel)) && ((control(channel) >> 3) & 7u) == 7u) {
                trigger(channel, now);
            }
        }
        return true;
    case DHR12R1: dhr_[0] = v & 0xFFFu; data[0] = true; break;
    case DHR12L1: dhr_[0] = (v >> 4) & 0xFFFu; data[0] = true; break;
    case DHR8R1: dhr_[0] = (v & 0xFFu) << 4; data[0] = true; break;
    case DHR12R2: dhr_[1] = v & 0xFFFu; data[1] = true; break;
    case DHR12L2: dhr_[1] = (v >> 4) & 0xFFFu; data[1] = true; break;
    case DHR8R2: dhr_[1] = (v & 0xFFu) << 4; data[1] = true; break;
    case DHR12RD:
        dhr_[0] = v & 0xFFFu;
        dhr_[1] = (v >> 16) & 0xFFFu;
        data[0] = data[1] = true;
        break;
    case DHR12LD:
        dhr_[0] = (v >> 4) & 0xFFFu;
        dhr_[1] = (v >> 20) & 0xFFFu;
        data[0] = data[1] = true;
        break;
    case DHR8RD:
        dhr_[0] = (v & 0xFFu) << 4;
        dhr_[1] = ((v >> 8) & 0xFFu) << 4;
        data[0] = data[1] = true;
        break;
    default: return true;
    }
    for (int channel = 0; channel < 2; ++channel) {
        if (data[channel] && !(control(channel) & DAC_TEN)) {
            load(channel, now); // without a trigger the output follows the data
        }
    }
    return true;
}

} // namespace simnodus::emu
