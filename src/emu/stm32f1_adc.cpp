// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f1_adc.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace simnodus::emu {

namespace {

// Register offsets (RM0008 11.12).
enum : std::uint32_t {
    SR = 0x00, CR1 = 0x04, CR2 = 0x08, SMPR1 = 0x0C, SMPR2 = 0x10, JOFR1 = 0x14, HTR = 0x24, LTR = 0x28,
    SQR1 = 0x2C, SQR2 = 0x30, SQR3 = 0x34, JSQR = 0x38, JDR1 = 0x3C, DR = 0x4C
};

constexpr std::uint32_t SR_AWD = 1u << 0, SR_EOC = 1u << 1, SR_JEOC = 1u << 2, SR_JSTRT = 1u << 3, SR_STRT = 1u << 4;
constexpr std::uint32_t CR1_EOCIE = 1u << 5, CR1_AWDIE = 1u << 6, CR1_JEOCIE = 1u << 7, CR1_SCAN = 1u << 8,
                        CR1_AWDSGL = 1u << 9, CR1_JAUTO = 1u << 10, CR1_DISCEN = 1u << 11, CR1_JDISCEN = 1u << 12,
                        CR1_JAWDEN = 1u << 22, CR1_AWDEN = 1u << 23;
constexpr std::uint32_t CR2_ADON = 1u << 0, CR2_CONT = 1u << 1, CR2_CAL = 1u << 2, CR2_RSTCAL = 1u << 3,
                        CR2_DMA = 1u << 8, CR2_ALIGN = 1u << 11, CR2_JEXTTRIG = 1u << 15, CR2_EXTTRIG = 1u << 20,
                        CR2_JSWSTART = 1u << 21, CR2_SWSTART = 1u << 22, CR2_TSVREFE = 1u << 23;

// Sample times in half ADC clock cycles: 1.5, 7.5, 13.5, 28.5, 41.5, 55.5, 71.5, 239.5.
constexpr std::uint32_t sample_x2[8] = {3, 15, 27, 57, 83, 111, 143, 479};
constexpr std::uint32_t conversion_x2 = 25; // 12.5 cycles
constexpr std::uint32_t calibration_cycles = 83;

constexpr double temperature_volts = 1.43; // V25, datasheet typical (25 °C)
constexpr double vrefint_volts = 1.20;

enum : std::uint32_t { F_DMA = 1u << 0, F_EXTERNAL = 1u << 1, F_DISCONTINUOUS = 1u << 2, F_DUAL = 1u << 3 };

} // namespace

Stm32F1Adc::Stm32F1Adc(AdcHost& host, int number) : host_(host), number_(number)
{
    std::snprintf(name_, sizeof name_, "ADC%d", number);
    reset();
}

void Stm32F1Adc::reset()
{
    sr_ = cr1_ = cr2_ = smpr1_ = smpr2_ = ltr_ = sqr1_ = sqr2_ = sqr3_ = jsqr_ = dr_ = 0;
    htr_ = 0xFFF;
    jofr_ = {};
    jdr_ = {};
    busy_ = Busy::None;
    regular_active_ = injected_active_ = false;
    regular_index_ = injected_index_ = 0;
}

std::uint32_t Stm32F1Adc::code_of(double volts)
{
    const double code = std::floor(volts / vdda * 4096.0 + 0.5);
    return static_cast<std::uint32_t>(std::clamp(code, 0.0, 4095.0));
}

bool Stm32F1Adc::irq() const
{
    return ((sr_ & SR_EOC) && (cr1_ & CR1_EOCIE)) || ((sr_ & SR_JEOC) && (cr1_ & CR1_JEOCIE))
        || ((sr_ & SR_AWD) && (cr1_ & CR1_AWDIE));
}

void Stm32F1Adc::update_irq()
{
    host_.adc_irq_changed();
}

std::uint32_t Stm32F1Adc::sample_cycles_x2(int channel) const
{
    const std::uint32_t smp = channel < 10 ? (smpr2_ >> (3 * channel)) & 7u : (smpr1_ >> (3 * (channel - 10))) & 7u;
    return sample_x2[smp];
}

int Stm32F1Adc::regular_length() const
{
    return (cr1_ & CR1_SCAN) ? static_cast<int>((sqr1_ >> 20) & 15u) + 1 : 1;
}

int Stm32F1Adc::injected_length() const
{
    return (cr1_ & CR1_SCAN) ? static_cast<int>((jsqr_ >> 20) & 3u) + 1 : 1;
}

int Stm32F1Adc::regular_channel(int index) const
{
    // SQ1..SQ6 in SQR3, SQ7..SQ12 in SQR2, SQ13..SQ16 in SQR1; 5 bits each.
    const std::uint32_t reg = index < 6 ? sqr3_ : (index < 12 ? sqr2_ : sqr1_);
    return static_cast<int>((reg >> (5 * (index % 6))) & 31u);
}

int Stm32F1Adc::injected_channel(int index) const
{
    // With JL + 1 conversions the sequence is the last JL + 1 of JSQ1..JSQ4
    // (RM0008 11.12.12: JL = 1 converts JSQ3 then JSQ4).
    const int jl = static_cast<int>((jsqr_ >> 20) & 3u);
    const int slot = (cr1_ & CR1_SCAN) ? 3 - jl + index : 3;
    return static_cast<int>((jsqr_ >> (5 * slot)) & 31u);
}

void Stm32F1Adc::catch_up()
{
    on_time(host_.now_cycles());
}

std::uint64_t Stm32F1Adc::next_event() const
{
    return busy_ == Busy::None ? no_event : end_at_;
}

void Stm32F1Adc::on_time(std::uint64_t cycle)
{
    while (busy_ != Busy::None && end_at_ <= cycle) {
        finish_step();
    }
}

void Stm32F1Adc::schedule_next(std::uint64_t at)
{
    const std::uint64_t ratio = host_.hclk_per_adc_clock();
    if (injected_active_) {
        busy_ = Busy::Injected;
        channel_ = injected_channel(injected_index_);
    } else if (regular_active_) {
        busy_ = Busy::Regular;
        channel_ = regular_channel(regular_index_);
    } else {
        busy_ = Busy::None;
        host_.adc_schedule_changed();
        return;
    }
    const std::uint32_t sample = sample_cycles_x2(channel_);
    sample_at_ = at + sample * ratio / 2;
    end_at_ = at + (sample + conversion_x2) * ratio / 2;
    host_.adc_schedule_changed();
}

void Stm32F1Adc::finish_step()
{
    const std::uint64_t at = end_at_;
    if (busy_ == Busy::Calibration) {
        cr2_ &= ~CR2_CAL;
        schedule_next(at);
        return;
    }
    double volts = 0.0;
    if (channel_ < 16) {
        volts = host_.adc_channel_voltage(channel_, sample_at_);
    } else if (number_ == 1 && (cr2_ & CR2_TSVREFE)) {
        volts = channel_ == 16 ? temperature_volts : vrefint_volts; // ADC2's IN16/IN17 are on VSS
    }
    const std::uint32_t code = code_of(volts);
    const bool left = (cr2_ & CR2_ALIGN) != 0;
    if (busy_ == Busy::Regular) {
        dr_ = left ? code << 4 : code;
        check_watchdog(channel_, code, false);
        if (++regular_index_ >= regular_length()) {
            regular_index_ = 0;
            sr_ |= SR_EOC;
            if (!(cr2_ & CR2_CONT)) {
                regular_active_ = false;
            }
            if ((cr1_ & CR1_JAUTO) && !injected_active_) {
                injected_active_ = true; // automatic injected conversion after the regular group
                injected_index_ = 0;
                sr_ |= SR_JSTRT;
            }
        }
    } else {
        const int rank = injected_index_;
        const auto value = static_cast<std::int32_t>(code) - static_cast<std::int32_t>(jofr_[static_cast<std::size_t>(rank)]);
        jdr_[static_cast<std::size_t>(rank)] = static_cast<std::uint32_t>(left ? value * 8 : value) & 0xFFFFu;
        check_watchdog(channel_, code, true);
        if (++injected_index_ >= injected_length()) {
            injected_index_ = 0;
            injected_active_ = false;
            sr_ |= SR_JEOC;
        }
    }
    update_irq();
    schedule_next(at);
}

void Stm32F1Adc::check_watchdog(int channel, std::uint32_t code, bool injected)
{
    if (!(cr1_ & (injected ? CR1_JAWDEN : CR1_AWDEN))) {
        return;
    }
    if ((cr1_ & CR1_AWDSGL) && channel != static_cast<int>(cr1_ & 31u)) {
        return;
    }
    if (code > htr_ || code < ltr_) {
        sr_ |= SR_AWD;
    }
}

void Stm32F1Adc::start_regular()
{
    if (!(cr2_ & CR2_ADON)) {
        return;
    }
    sr_ |= SR_STRT;
    regular_active_ = true;
    if (busy_ == Busy::None) {
        regular_index_ = 0;
        schedule_next(host_.now_cycles());
    } else if (busy_ != Busy::Regular) {
        regular_index_ = 0;
    }
}

void Stm32F1Adc::start_injected()
{
    if (!(cr2_ & CR2_ADON)) {
        return;
    }
    sr_ |= SR_JSTRT;
    if (!injected_active_) {
        injected_active_ = true;
        injected_index_ = 0;
    }
    if (busy_ == Busy::None) {
        schedule_next(host_.now_cycles()); // otherwise after the current conversion
    }
}

void Stm32F1Adc::report(std::uint32_t feature, const char* text)
{
    if (!(reported_ & feature)) {
        reported_ |= feature;
        host_.adc_diagnostic(std::string(name_) + ": " + text + " is not simulated yet.");
    }
}

bool Stm32F1Adc::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    catch_up();
    const std::uint32_t word = read32(offset & ~3u);
    const unsigned shift = 8 * (offset & 3u);
    value = (word >> shift) & (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u));
    return true;
}

bool Stm32F1Adc::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    catch_up();
    const unsigned shift = 8 * (offset & 3u);
    const std::uint32_t mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u)) << shift;
    write32(offset & ~3u, (value << shift) & mask, mask);
    return true;
}

std::uint32_t Stm32F1Adc::read32(std::uint32_t offset)
{
    switch (offset) {
    case SR: return sr_;
    case CR1: return cr1_;
    case CR2: return cr2_;
    case SMPR1: return smpr1_;
    case SMPR2: return smpr2_;
    case JOFR1: case JOFR1 + 4: case JOFR1 + 8: case JOFR1 + 12: return jofr_[(offset - JOFR1) / 4];
    case HTR: return htr_;
    case LTR: return ltr_;
    case SQR1: return sqr1_;
    case SQR2: return sqr2_;
    case SQR3: return sqr3_;
    case JSQR: return jsqr_;
    case JDR1: case JDR1 + 4: case JDR1 + 8: case JDR1 + 12: return jdr_[(offset - JDR1) / 4];
    case DR:
        sr_ &= ~SR_EOC; // reading DR clears EOC
        update_irq();
        return dr_;
    default: return 0;
    }
}

void Stm32F1Adc::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    auto merge = [mask, value](std::uint32_t old) { return (old & ~mask) | (value & mask); };
    switch (offset) {
    case SR:
        sr_ &= (value | ~mask) | ~0x1Fu; // rc_w0
        update_irq();
        break;
    case CR1:
        cr1_ = merge(cr1_) & 0x00CFFFFFu;
        if (cr1_ & (CR1_DISCEN | CR1_JDISCEN)) {
            report(F_DISCONTINUOUS, "discontinuous mode");
        }
        if ((cr1_ >> 16) & 15u) {
            report(F_DUAL, "dual ADC mode");
        }
        update_irq();
        break;
    case CR2: {
        const std::uint32_t old = cr2_;
        const std::uint32_t written = value & mask;
        std::uint32_t now = merge(cr2_) & 0x00FEF90Fu;
        const bool adon_again = (old & CR2_ADON) && (written & CR2_ADON)
            && ((now ^ old) & ~(CR2_SWSTART | CR2_JSWSTART | CR2_RSTCAL)) == 0;
        now &= ~(CR2_SWSTART | CR2_JSWSTART | CR2_RSTCAL); // cleared as soon as they act
        cr2_ = now;
        if (!(now & CR2_ADON)) {
            busy_ = Busy::None; // powered down: conversions stop
            regular_active_ = injected_active_ = false;
            host_.adc_schedule_changed();
            break;
        }
        if (now & CR2_DMA) {
            report(F_DMA, "DMA transfer of conversions (SN-179)");
        }
        const bool regular_software = ((now >> 17) & 7u) == 7u;
        const bool injected_software = ((now >> 12) & 7u) == 7u;
        if (((now & CR2_EXTTRIG) && !regular_software) || ((now & CR2_JEXTTRIG) && !injected_software)) {
            report(F_EXTERNAL, "starting conversions from a timer or EXTI trigger");
        }
        if ((now & CR2_CAL) && !(old & CR2_CAL) && busy_ == Busy::None) {
            busy_ = Busy::Calibration;
            end_at_ = host_.now_cycles() + std::uint64_t{calibration_cycles} * host_.hclk_per_adc_clock();
            host_.adc_schedule_changed();
        }
        if (adon_again || ((written & CR2_SWSTART) && (now & CR2_EXTTRIG) && regular_software)) {
            start_regular();
        }
        if ((written & CR2_JSWSTART) && (now & CR2_JEXTTRIG) && injected_software) {
            start_injected();
        }
        update_irq();
        break;
    }
    case SMPR1: smpr1_ = merge(smpr1_) & 0x00FFFFFFu; break;
    case SMPR2: smpr2_ = merge(smpr2_) & 0x3FFFFFFFu; break;
    case JOFR1: case JOFR1 + 4: case JOFR1 + 8: case JOFR1 + 12: {
        auto& reg = jofr_[(offset - JOFR1) / 4];
        reg = merge(reg) & 0xFFFu;
        break;
    }
    case HTR: htr_ = merge(htr_) & 0xFFFu; break;
    case LTR: ltr_ = merge(ltr_) & 0xFFFu; break;
    case SQR1: sqr1_ = merge(sqr1_) & 0x00FFFFFFu; break;
    case SQR2: sqr2_ = merge(sqr2_) & 0x3FFFFFFFu; break;
    case SQR3: sqr3_ = merge(sqr3_) & 0x3FFFFFFFu; break;
    case JSQR: jsqr_ = merge(jsqr_) & 0x003FFFFFu; break;
    default: break; // JDRx and DR are read-only
    }
}

} // namespace simnodus::emu
