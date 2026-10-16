// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f1_timer.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace simnodus::emu {

namespace {

constexpr std::uint64_t never = std::numeric_limits<std::uint64_t>::max();

// Register offsets (RM0008 15.4 / 14.4).
enum : std::uint32_t {
    CR1 = 0x00, CR2 = 0x04, SMCR = 0x08, DIER = 0x0C, SR = 0x10, EGR = 0x14, CCMR1 = 0x18, CCMR2 = 0x1C,
    CCER = 0x20, CNT = 0x24, PSC = 0x28, ARR = 0x2C, RCR = 0x30, CCR1 = 0x34, BDTR = 0x44, DCR = 0x48, DMAR = 0x4C
};

constexpr std::uint32_t CR1_CEN = 1u << 0, CR1_UDIS = 1u << 1, CR1_URS = 1u << 2, CR1_OPM = 1u << 3,
                        CR1_ARPE = 1u << 7;

// Features reported once when the firmware uses them.
enum : std::uint32_t {
    F_CENTER = 1u << 0, F_SLAVE = 1u << 1, F_FILTER = 1u << 2, F_DEADTIME = 1u << 3, F_BREAK = 1u << 4,
    F_DMA = 1u << 5, F_INPUT_SOURCE = 1u << 6
};

} // namespace

Stm32F1Timer::Stm32F1Timer(TimerHost& host, int number) : host_(host), number_(number), advanced_(number == 1)
{
    std::snprintf(name_, sizeof name_, "TIM%d", number);
    reset();
}

void Stm32F1Timer::reset()
{
    // The machine releases the alternate-function levels of every pin at reset.
    cr1_ = cr2_ = smcr_ = dier_ = sr_ = ccmr1_ = ccmr2_ = ccer_ = 0;
    psc_ = psc_active_ = 0;
    arr_ = arr_active_ = 0xFFFF;
    rcr_ = rcr_count_ = 0;
    bdtr_ = dcr_ = 0;
    ch_ = {};
    kernel_ = 1; // RCC reset: APB dividers 1
    output_cycle_ = 0;
    base_cycle_ = host_.now_cycles();
    base_count_ = 0;
    update_irqs();
}

void Stm32F1Timer::catch_up()
{
    on_time(host_.now_cycles());
}

std::uint64_t Stm32F1Timer::tick_cycles() const
{
    return std::uint64_t{psc_active_ + 1} * kernel_;
}

std::uint32_t Stm32F1Timer::top() const
{
    return base_count_ > arr_active_ ? 0xFFFFu : arr_active_;
}

std::uint32_t Stm32F1Timer::counter_at(std::uint64_t cycle) const
{
    if (!(cr1_ & CR1_CEN) || cycle <= base_cycle_) {
        return base_count_;
    }
    const std::uint64_t steps = (cycle - base_cycle_) / tick_cycles();
    if (counting_down()) {
        return steps >= base_count_ ? 0 : base_count_ - static_cast<std::uint32_t>(steps);
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(base_count_ + steps, top()));
}

void Stm32F1Timer::rebase(std::uint64_t cycle)
{
    if (!(cr1_ & CR1_CEN)) {
        base_cycle_ = cycle; // a stopped counter starts its first tick when enabled
        return;
    }
    if (cycle <= base_cycle_) {
        return;
    }
    // Moves the base to the last tick at or before `cycle`, keeping the phase of
    // the prescaler.
    std::uint64_t steps = (cycle - base_cycle_) / tick_cycles();
    if (counting_down()) {
        steps = std::min<std::uint64_t>(steps, base_count_);
        base_count_ -= static_cast<std::uint32_t>(steps);
    } else {
        steps = std::min<std::uint64_t>(steps, top() - base_count_);
        base_count_ += static_cast<std::uint32_t>(steps);
    }
    base_cycle_ += steps * tick_cycles();
}

std::uint32_t Stm32F1Timer::mode(int channel) const
{
    const std::uint32_t reg = channel < 2 ? ccmr1_ : ccmr2_;
    return (reg >> ((channel & 1) * 8 + 4)) & 7u;
}

std::uint32_t Stm32F1Timer::selection(int channel) const
{
    const std::uint32_t reg = channel < 2 ? ccmr1_ : ccmr2_;
    return (reg >> ((channel & 1) * 8)) & 3u;
}

std::uint64_t Stm32F1Timer::next_event() const
{
    if (!(cr1_ & CR1_CEN)) {
        return never;
    }
    const std::uint64_t tick = tick_cycles();
    std::uint64_t next;
    if (counting_down()) {
        next = base_cycle_ + (std::uint64_t{base_count_} + 1) * tick;
        for (int c = 0; c < 4; ++c) {
            const std::uint32_t ccr = ch_[static_cast<std::size_t>(c)].ccr;
            if (selection(c) == 0 && ccr < base_count_) {
                next = std::min(next, base_cycle_ + std::uint64_t{base_count_ - ccr} * tick);
            }
        }
    } else {
        const std::uint32_t limit = top();
        next = base_cycle_ + (std::uint64_t{limit} - base_count_ + 1) * tick;
        for (int c = 0; c < 4; ++c) {
            const std::uint32_t ccr = ch_[static_cast<std::size_t>(c)].ccr;
            if (selection(c) == 0 && ccr > base_count_ && ccr <= limit) {
                next = std::min(next, base_cycle_ + std::uint64_t{ccr - base_count_} * tick);
            }
        }
    }
    return next;
}

void Stm32F1Timer::on_time(std::uint64_t cycle)
{
    for (std::uint64_t event = next_event(); event <= cycle; event = next_event()) {
        output_cycle_ = event;
        const std::uint64_t tick = tick_cycles();
        const bool overflow = counting_down() ? event == base_cycle_ + (std::uint64_t{base_count_} + 1) * tick
                                              : event == base_cycle_ + (std::uint64_t{top()} - base_count_ + 1) * tick;
        if (overflow) {
            base_cycle_ = event;
            base_count_ = counting_down() ? arr_active_ : 0;
            update_event(true);
        } else {
            base_count_ = counter_at(event);
            base_cycle_ = event;
            for (int c = 0; c < 4; ++c) {
                if (selection(c) == 0 && ch_[static_cast<std::size_t>(c)].ccr == base_count_) {
                    match(c);
                }
            }
            evaluate_refs();
            update_outputs();
            update_irqs();
        }
    }
    output_cycle_ = 0;
}

void Stm32F1Timer::update_event(bool from_counter)
{
    if (cr1_ & CR1_UDIS) {
        // No update event; UG still reinitializes the counter (done by the caller).
    } else if (from_counter && advanced_ && rcr_count_ > 0) {
        --rcr_count_; // repetition counter: no update event yet
    } else {
        rcr_count_ = rcr_;
        psc_active_ = psc_;
        arr_active_ = arr_;
        for (int c = 0; c < 4; ++c) {
            auto& channel = ch_[static_cast<std::size_t>(c)];
            const std::uint32_t reg = c < 2 ? ccmr1_ : ccmr2_;
            if ((reg >> ((c & 1) * 8 + 3)) & 1u) { // OCxPE
                channel.ccr = channel.ccr_preload;
            }
        }
        if (from_counter || !(cr1_ & CR1_URS)) {
            sr_ |= 1u; // UIF
        }
        if (from_counter && (cr1_ & CR1_OPM)) {
            cr1_ &= ~CR1_CEN; // one-pulse mode stops at the update event
        }
    }
    for (int c = 0; c < 4; ++c) {
        if (selection(c) == 0 && ch_[static_cast<std::size_t>(c)].ccr == base_count_ && from_counter) {
            match(c);
        }
    }
    evaluate_refs();
    update_outputs();
    update_irqs();
    host_.timer_schedule_changed();
}

void Stm32F1Timer::match(int channel)
{
    auto& c = ch_[static_cast<std::size_t>(channel)];
    sr_ |= 1u << (channel + 1); // CCxIF
    switch (mode(channel)) {
    case 1: c.ref = true; break;  // active on match
    case 2: c.ref = false; break; // inactive on match
    case 3: c.ref = !c.ref; break; // toggle
    default: break;
    }
}

void Stm32F1Timer::evaluate_refs()
{
    const std::uint32_t count = base_count_;
    for (int c = 0; c < 4; ++c) {
        if (selection(c) != 0) {
            continue;
        }
        auto& channel = ch_[static_cast<std::size_t>(c)];
        switch (mode(c)) {
        case 4: channel.ref = false; break;
        case 5: channel.ref = true; break;
        case 6: // PWM 1
            channel.ref = counting_down() ? count <= channel.ccr : count < channel.ccr;
            break;
        case 7: // PWM 2
            channel.ref = counting_down() ? count > channel.ccr : count >= channel.ccr;
            break;
        default: break;
        }
    }
}

std::pair<int, int> Stm32F1Timer::channel_pin(int channel, bool complementary) const
{
    const std::uint32_t mapr = host_.afio_mapr();
    // Ports: 0 = A, 1 = B, 2 = C, 3 = D, 4 = E.
    switch (number_) {
    case 1: {
        const std::uint32_t remap = (mapr >> 6) & 3u;
        if (!complementary) {
            if (remap == 3) {
                static const int pins[4] = {9, 11, 13, 14};
                return {4, pins[channel]};
            }
            return {0, 8 + channel};
        }
        if (channel == 3) {
            return {-1, -1};
        }
        if (remap == 1) {
            static const std::pair<int, int> pins[3] = {{0, 7}, {1, 0}, {1, 1}};
            return pins[channel];
        }
        if (remap == 3) {
            static const int pins[3] = {8, 10, 12};
            return {4, pins[channel]};
        }
        return {1, 13 + channel};
    }
    case 2: {
        const std::uint32_t remap = (mapr >> 8) & 3u;
        static const std::pair<int, int> table[4][4] = {
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
            {{0, 15}, {1, 3}, {0, 2}, {0, 3}},
            {{0, 0}, {0, 1}, {1, 10}, {1, 11}},
            {{0, 15}, {1, 3}, {1, 10}, {1, 11}},
        };
        return complementary ? std::pair{-1, -1} : table[remap][channel];
    }
    case 3: {
        const std::uint32_t remap = (mapr >> 10) & 3u;
        static const std::pair<int, int> table[4][4] = {
            {{0, 6}, {0, 7}, {1, 0}, {1, 1}},
            {{0, 6}, {0, 7}, {1, 0}, {1, 1}}, // 01 is not used on the F103
            {{1, 4}, {1, 5}, {1, 0}, {1, 1}},
            {{2, 6}, {2, 7}, {2, 8}, {2, 9}},
        };
        return complementary ? std::pair{-1, -1} : table[remap][channel];
    }
    default: {
        if (complementary) {
            return {-1, -1};
        }
        return (mapr >> 12) & 1u ? std::pair{3, 12 + channel} : std::pair{1, 6 + channel};
    }
    }
}

void Stm32F1Timer::update_outputs()
{
    const std::uint64_t cycle = output_cycle_ != 0 ? output_cycle_ : host_.now_cycles();
    const bool main_enable = !advanced_ || (bdtr_ & (1u << 15)); // MOE
    // Writes `level` for `source` on `pin`, moving it when the remap changed.
    auto drive = [&](bool& driven, std::pair<int, int>& at, std::pair<int, int> pin, int source, bool level) {
        if (pin != at && driven && at.first >= 0) {
            host_.timer_output(at.first, at.second, source, false, cycle);
            driven = false;
        }
        at = pin;
        if (level != driven) {
            driven = level;
            if (pin.first >= 0) {
                host_.timer_output(pin.first, pin.second, source, level, cycle);
            }
        }
    };
    for (int c = 0; c < 4; ++c) {
        auto& channel = ch_[static_cast<std::size_t>(c)];
        const bool output = selection(c) == 0;
        const bool enabled = output && main_enable && ((ccer_ >> (4 * c)) & 1u);
        const bool level = enabled && (channel.ref != (((ccer_ >> (4 * c + 1)) & 1u) != 0));
        drive(channel.driven, channel.pin, channel_pin(c, false), source_id(c, false), level);
        if (advanced_) {
            const bool enabled_n = output && main_enable && ((ccer_ >> (4 * c + 2)) & 1u);
            const bool level_n = enabled_n && (!channel.ref != (((ccer_ >> (4 * c + 3)) & 1u) != 0));
            drive(channel.driven_n, channel.pin_n, channel_pin(c, true), source_id(c, true), level_n);
        }
    }
}

int Stm32F1Timer::source_id(int channel, bool complementary) const
{
    return 100 + number_ * 10 + channel + (complementary ? 5 : 0);
}

void Stm32F1Timer::remap_changed()
{
    update_outputs();
}

void Stm32F1Timer::update_irqs()
{
    const std::uint32_t active = sr_ & dier_;
    if (advanced_) {
        host_.timer_irq(24, (active & (1u << 7)) != 0);       // TIM1_BRK
        host_.timer_irq(25, (active & 1u) != 0);              // TIM1_UP
        host_.timer_irq(26, (active & 0x60u) != 0);           // TIM1_TRG_COM
        host_.timer_irq(27, (active & 0x1Eu) != 0);           // TIM1_CC
    } else {
        host_.timer_irq(26 + number_, (active & 0x5Fu) != 0); // TIM2 28, TIM3 29, TIM4 30
    }
}

void Stm32F1Timer::clocks_changed()
{
    catch_up();
    rebase(host_.now_cycles());
    kernel_ = host_.hclk_per_kernel_tick(advanced_);
    host_.timer_schedule_changed();
}

void Stm32F1Timer::pin_changed(int port, int pin, bool level)
{
    for (int c = 0; c < 4; ++c) {
        auto& channel = ch_[static_cast<std::size_t>(c)];
        const std::uint32_t sel = selection(c);
        if (sel == 0) {
            continue;
        }
        const auto [p, n] = channel_pin(c, false);
        if (p != port || n != pin) {
            continue;
        }
        const bool previous = channel.last_input;
        channel.last_input = level;
        if (sel != 1) {
            report(F_INPUT_SOURCE, "input capture from a neighbouring channel or TRC");
            continue;
        }
        if (!((ccer_ >> (4 * c)) & 1u)) {
            continue; // capture disabled
        }
        const bool falling = (ccer_ >> (4 * c + 1)) & 1u;
        if (level == previous || level == falling) {
            continue; // not the selected edge
        }
        const std::uint32_t reg = c < 2 ? ccmr1_ : ccmr2_;
        const std::uint32_t prescaler = 1u << ((reg >> ((c & 1) * 8 + 2)) & 3u);
        if (++channel.capture_count < static_cast<int>(prescaler)) {
            continue;
        }
        channel.capture_count = 0;
        if (sr_ & (1u << (c + 1))) {
            sr_ |= 1u << (c + 9); // CCxOF
        }
        channel.ccr = channel.ccr_preload = counter_at(host_.now_cycles());
        sr_ |= 1u << (c + 1);
        update_irqs();
    }
}

void Stm32F1Timer::report(std::uint32_t feature, const char* text)
{
    if (!(reported_ & feature)) {
        reported_ |= feature;
        host_.timer_diagnostic(std::string(name_) + ": " + text + " is not simulated yet.");
    }
}

bool Stm32F1Timer::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    catch_up();
    const std::uint32_t word = read32(offset & ~3u);
    const unsigned shift = 8 * (offset & 3u);
    value = (word >> shift) & (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u));
    return true;
}

bool Stm32F1Timer::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    catch_up();
    const unsigned shift = 8 * (offset & 3u);
    const std::uint32_t mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u)) << shift;
    write32(offset & ~3u, (value << shift) & mask, mask);
    return true;
}

std::uint32_t Stm32F1Timer::read32(std::uint32_t offset)
{
    switch (offset) {
    case CR1: return cr1_;
    case CR2: return cr2_;
    case SMCR: return smcr_;
    case DIER: return dier_;
    case SR: return sr_;
    case CCMR1: return ccmr1_;
    case CCMR2: return ccmr2_;
    case CCER: return ccer_;
    case CNT: return counter_at(host_.now_cycles());
    case PSC: return psc_;
    case ARR: return arr_;
    case RCR: return advanced_ ? rcr_ : 0;
    case CCR1: case CCR1 + 4: case CCR1 + 8: case CCR1 + 12: {
        const int c = static_cast<int>((offset - CCR1) / 4);
        const auto& channel = ch_[static_cast<std::size_t>(c)];
        if (selection(c) == 0) {
            return channel.ccr_preload;
        }
        sr_ &= ~(1u << (c + 1)); // reading a captured value clears CCxIF
        update_irqs();
        return channel.ccr;
    }
    case BDTR: return advanced_ ? bdtr_ : 0;
    case DCR: return dcr_;
    default: return 0;
    }
}

void Stm32F1Timer::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    const std::uint64_t now = host_.now_cycles();
    auto merge = [mask, value](std::uint32_t old) { return (old & ~mask) | (value & mask); };
    switch (offset) {
    case CR1: {
        rebase(now);
        cr1_ = merge(cr1_) & 0x3FFu;
        if ((cr1_ >> 5) & 3u) {
            report(F_CENTER, "center-aligned counting");
        }
        break;
    }
    case CR2: cr2_ = merge(cr2_) & 0x7FFFu; break;
    case SMCR:
        smcr_ = merge(smcr_) & 0xFFF7u;
        if (smcr_ & 7u) {
            report(F_SLAVE, "the slave mode controller (encoder, reset, gated and trigger modes)");
        }
        break;
    case DIER:
        dier_ = merge(dier_) & 0x7FFFu;
        if (dier_ & 0x5F00u) {
            report(F_DMA, "timer DMA requests");
        }
        break;
    case SR: sr_ &= (value | ~mask) | ~0x1EFFu; break; // rc_w0
    case EGR: {
        const std::uint32_t v = value & mask;
        if (v & 1u) { // UG: reinitialize the counter and update the shadow registers
            base_cycle_ = now;
            base_count_ = counting_down() ? arr_ : 0;
            update_event(false);
        }
        for (int c = 0; c < 4; ++c) {
            if (v & (1u << (c + 1))) { // CCxG
                if (selection(c) != 0) {
                    ch_[static_cast<std::size_t>(c)].ccr = counter_at(now);
                }
                sr_ |= 1u << (c + 1);
            }
        }
        if (v & (1u << 6)) {
            sr_ |= 1u << 6; // TIF
        }
        break;
    }
    case CCMR1:
    case CCMR2: {
        std::uint32_t& reg = offset == CCMR1 ? ccmr1_ : ccmr2_;
        reg = merge(reg) & 0xFFFFu;
        for (int half = 0; half < 2; ++half) {
            const std::uint32_t bits = (reg >> (8 * half)) & 0xFFu;
            if ((bits & 3u) != 0 && (bits >> 4) != 0) {
                report(F_FILTER, "the input capture filter");
            }
        }
        for (int c = offset == CCMR1 ? 0 : 2, end = c + 2; c < end; ++c) {
            if (selection(c) != 0) { // input: edges are seen from the current level on
                const auto [port, pin] = channel_pin(c, false);
                ch_[static_cast<std::size_t>(c)].last_input = port >= 0 && host_.pin_level(port, pin);
            }
        }
        rebase(now);
        evaluate_refs();
        break;
    }
    case CCER: ccer_ = merge(ccer_) & (advanced_ ? 0x3FFFu : 0x3333u); break;
    case CNT:
        base_cycle_ = now;
        base_count_ = merge(counter_at(now)) & 0xFFFFu;
        break;
    case PSC: psc_ = merge(psc_) & 0xFFFFu; break;
    case ARR:
        rebase(now);
        arr_ = merge(arr_) & 0xFFFFu;
        if (!(cr1_ & CR1_ARPE)) {
            arr_active_ = arr_;
        }
        break;
    case RCR:
        if (advanced_) {
            rcr_ = merge(rcr_) & 0xFFu;
        }
        break;
    case CCR1: case CCR1 + 4: case CCR1 + 8: case CCR1 + 12: {
        const int c = static_cast<int>((offset - CCR1) / 4);
        auto& channel = ch_[static_cast<std::size_t>(c)];
        if (selection(c) != 0) {
            break; // read-only in input capture mode
        }
        rebase(now);
        channel.ccr_preload = merge(channel.ccr_preload) & 0xFFFFu;
        const std::uint32_t reg = c < 2 ? ccmr1_ : ccmr2_;
        if (!((reg >> ((c & 1) * 8 + 3)) & 1u)) {
            channel.ccr = channel.ccr_preload;
        }
        evaluate_refs();
        break;
    }
    case BDTR:
        if (advanced_) {
            bdtr_ = merge(bdtr_) & 0xFFFFu;
            if (bdtr_ & 0xFFu) {
                report(F_DEADTIME, "dead time");
            }
            if (bdtr_ & (1u << 12)) {
                report(F_BREAK, "the break input");
            }
        }
        break;
    case DCR: dcr_ = merge(dcr_) & 0x1F1Fu; break;
    default: break;
    }
    update_outputs();
    update_irqs();
    host_.timer_schedule_changed();
}

} // namespace simnodus::emu
