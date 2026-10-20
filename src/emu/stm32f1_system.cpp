// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f1_system.hpp"

#include <algorithm>
#include <cmath>

namespace simnodus::emu {

namespace {

constexpr std::uint64_t ns_per_s = 1000000000ull;

// value * num / den without overflow for simulation times.
std::uint64_t scale(std::uint64_t value, std::uint64_t num, std::uint64_t den, bool round_up)
{
    const std::uint64_t q = value / den;
    const std::uint64_t r = value % den;
    const std::uint64_t part = r * num;
    return q * num + part / den + (round_up && part % den ? 1 : 0);
}

} // namespace

// ---------------------------------------------------------------------------

bool WordDevice::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    const std::uint32_t word = read32(offset & ~3u);
    const unsigned shift = 8 * (offset & 3u);
    value = (word >> shift) & (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u));
    return true;
}

bool WordDevice::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    const unsigned shift = 8 * (offset & 3u);
    const std::uint32_t mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u)) << shift;
    write32(offset & ~3u, (value << shift) & mask, mask);
    return true;
}

// ---------------------------------------------------------------------------
// CRC (RM0008 4)

void Stm32F1Crc::reset()
{
    dr_ = 0xFFFFFFFFu;
    idr_ = 0;
}

std::uint32_t Stm32F1Crc::step(std::uint32_t crc, std::uint32_t word)
{
    crc ^= word;
    for (int bit = 0; bit < 32; ++bit) {
        crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : crc << 1;
    }
    return crc;
}

std::uint32_t Stm32F1Crc::read32(std::uint32_t offset)
{
    switch (offset) {
    case 0x00: return dr_;
    case 0x04: return idr_;
    default: return 0;
    }
}

void Stm32F1Crc::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    switch (offset) {
    case 0x00: dr_ = step(dr_, value & mask); break;
    case 0x04: idr_ = merge(idr_, value, mask) & 0xFFu; break;
    case 0x08:
        if (value & mask & 1u) {
            dr_ = 0xFFFFFFFFu;
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// IWDG (RM0008 19)

void Stm32F1Iwdg::reset()
{
    running_ = false;
    unlocked_ = false;
    pr_ = 0;
    rlr_ = 0xFFF;
}

void Stm32F1Iwdg::reload()
{
    // Timeout = (RLR + 1) * prescaler / LSI; the prescaler is 4 << PR, at most 256.
    const std::uint64_t prescaler = 4u << std::min<std::uint32_t>(pr_, 6);
    deadline_ns_ = host_.system_now_ns() + scale((rlr_ + 1) * prescaler, ns_per_s, lsi_hz, true);
    host_.system_schedule_changed();
}

std::uint64_t Stm32F1Iwdg::next_event() const
{
    return running_ ? host_.system_cycles_at(deadline_ns_) : no_event;
}

void Stm32F1Iwdg::on_time(std::uint64_t)
{
    if (running_) {
        running_ = false;
        host_.watchdog_reset(true);
    }
}

std::uint32_t Stm32F1Iwdg::read32(std::uint32_t offset)
{
    switch (offset) {
    case 0x04: return pr_;
    case 0x08: return rlr_;
    default: return 0; // KR is write-only; SR: PVU and RVU, updates apply at once
    }
}

void Stm32F1Iwdg::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    switch (offset) {
    case 0x00: {
        if ((mask & 0xFFFFu) != 0xFFFFu) {
            return;
        }
        const std::uint32_t key = value & 0xFFFFu;
        unlocked_ = key == 0x5555u; // any other key locks PR and RLR again
        if (key == 0xCCCCu) {
            running_ = true;
            reload();
        } else if (key == 0xAAAAu && running_) {
            reload();
        }
        break;
    }
    case 0x04:
        if (unlocked_) {
            pr_ = merge(pr_, value, mask) & 7u;
        }
        break;
    case 0x08:
        if (unlocked_) {
            rlr_ = merge(rlr_, value, mask) & 0xFFFu;
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// WWDG (RM0008 20)

void Stm32F1Wwdg::reset()
{
    active_ = false;
    loaded_ = 0x7F;
    loaded_ns_ = host_.system_now_ns();
    cfr_ = 0x7F;
    ewif_ = false;
    ewi_done_ = false;
    update_irq();
}

std::uint32_t Stm32F1Wwdg::ticks_since_load() const
{
    const std::uint64_t pclk = std::max<std::uint32_t>(host_.pclk1_hz(), 1);
    const std::uint64_t divider = 4096ull << ((cfr_ >> 7) & 3u);
    const std::uint64_t elapsed = host_.system_now_ns() - loaded_ns_;
    // elapsed * pclk / (divider * 1e9), exact in whole counter steps.
    const std::uint64_t pclk_cycles = scale(elapsed, pclk, ns_per_s, false);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(pclk_cycles / divider, 0xFFFFFFFFu));
}

std::uint32_t Stm32F1Wwdg::counter() const
{
    return (loaded_ - ticks_since_load()) & 0x7Fu;
}

void Stm32F1Wwdg::load(std::uint32_t value)
{
    loaded_ = value & 0x7Fu;
    loaded_ns_ = host_.system_now_ns();
    host_.system_schedule_changed();
}

void Stm32F1Wwdg::update_irq()
{
    host_.system_irq(0, ewif_ && (cfr_ & 0x200u));
}

std::uint64_t Stm32F1Wwdg::next_event() const
{
    if (!active_ || loaded_ < 0x40) {
        return no_event;
    }
    // The early wakeup at 0x40, then the reset when T6 clears (0x40 -> 0x3F).
    const bool ewi = (cfr_ & 0x200u) && !ewi_done_;
    const std::uint32_t ticks = loaded_ - (ewi ? 0x40u : 0x3Fu);
    const std::uint64_t pclk = std::max<std::uint32_t>(host_.pclk1_hz(), 1);
    const std::uint64_t divider = 4096ull << ((cfr_ >> 7) & 3u);
    return host_.system_cycles_at(loaded_ns_ + scale(ticks * divider, ns_per_s, pclk, true));
}

void Stm32F1Wwdg::on_time(std::uint64_t)
{
    if (!active_) {
        return;
    }
    const std::uint32_t ticks = ticks_since_load();
    if ((cfr_ & 0x200u) && !ewi_done_ && ticks >= loaded_ - 0x40u) {
        ewi_done_ = true;
        ewif_ = true;
        update_irq();
    }
    if (ticks >= loaded_ - 0x3Fu) {
        active_ = false;
        host_.watchdog_reset(false);
    }
}

void Stm32F1Wwdg::clocks_changed()
{
    // The counter keeps its value and counts on at the new PCLK1.
    const std::uint32_t now = counter();
    if (active_ && now < 0x40) {
        return; // the reset is due already
    }
    loaded_ = now;
    loaded_ns_ = host_.system_now_ns();
    host_.system_schedule_changed();
}

std::uint32_t Stm32F1Wwdg::read32(std::uint32_t offset)
{
    switch (offset) {
    case 0x00: return counter() | (active_ ? 0x80u : 0u);
    case 0x04: return cfr_;
    case 0x08: return ewif_ ? 1u : 0u;
    default: return 0;
    }
}

void Stm32F1Wwdg::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    switch (offset) {
    case 0x00: {
        const bool was_active = active_;
        active_ = active_ || (value & mask & 0x80u); // WDGA is cleared only by a reset
        if (mask & 0x7Fu) {
            const std::uint32_t now = counter();
            if (was_active && now > (cfr_ & 0x7Fu)) {
                if (!warned_early_) {
                    warned_early_ = true;
                    host_.system_diagnostic("The window watchdog (WWDG) was refreshed too early (counter 0x"
                    + std::string(1, "0123456789ABCDEF"[now >> 4]) + std::string(1, "0123456789ABCDEF"[now & 15])
                    + " above the window); the chip resets, as a real STM32F103 does.");
                }
                active_ = false;
                host_.watchdog_reset(false);
                return;
            }
            load(merge(loaded_, value, mask & 0x7Fu));
            ewi_done_ = false;
        }
        if (active_ && !(loaded_ & 0x40u)) {
            active_ = false; // T6 written as 0: an immediate reset
            host_.watchdog_reset(false);
        }
        host_.system_schedule_changed();
        break;
    }
    case 0x04: {
        const std::uint32_t now = counter();
        const std::uint32_t ewi = cfr_ & 0x200u; // EWI is cleared only by a reset
        cfr_ = (merge(cfr_, value, mask) & 0x3FFu) | ewi;
        if (now != loaded_ || ((cfr_ ^ value) & mask & 0x180u)) {
            loaded_ = now; // the time base may have changed: count on from now
            loaded_ns_ = host_.system_now_ns();
        }
        update_irq();
        host_.system_schedule_changed();
        break;
    }
    case 0x08:
        if (mask & 1u) {
            ewif_ = ewif_ && (value & 1u); // rc_w0
            update_irq();
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// RTC (RM0008 18)

void Stm32F1Rtc::reset()
{
    advance();
    crh_ = 0;
    flags_ = 0;
    config_ = false;
    update_irq();
}

void Stm32F1Rtc::backup_reset()
{
    prl_ = 0x8000;
    cnt_ = 0;
    alr_ = 0xFFFFFFFFu;
    phase_ = 0;
    flags_ = 0;
    anchor_ns_ = host_.system_now_ns();
    counted_ = 0;
    hz_ = host_.rtc_clock_hz();
    update_irq();
}

void Stm32F1Rtc::advance()
{
    if (hz_ == 0) {
        return;
    }
    const std::uint64_t now = host_.system_now_ns();
    const std::uint64_t cycles = now > anchor_ns_ ? scale(now - anchor_ns_, hz_, ns_per_s, false) : 0;
    if (cycles <= counted_) {
        return;
    }
    const std::uint64_t total = phase_ + (cycles - counted_);
    counted_ = cycles;
    const std::uint64_t period = static_cast<std::uint64_t>(prl_) + 1;
    const std::uint64_t ticks = total / period;
    phase_ = static_cast<std::uint32_t>(total % period);
    if (ticks == 0) {
        return;
    }
    const std::uint32_t before = cnt_;
    const std::uint32_t to_alarm = alr_ - before; // ticks until CNT equals ALR (0: it does already)
    flags_ |= 1u;                                 // SECF
    if (static_cast<std::uint64_t>(before) + ticks > 0xFFFFFFFFull) {
        flags_ |= 4u; // OWF
    }
    cnt_ = static_cast<std::uint32_t>(before + ticks);
    if (to_alarm != 0 && to_alarm <= ticks) {
        flags_ |= 2u; // ALRF; the alarm also raises EXTI line 17
        host_.system_exti(17, true);
        host_.system_exti(17, false);
    }
    update_irq();
}

void Stm32F1Rtc::anchor()
{
    advance();
    anchor_ns_ = host_.system_now_ns();
    counted_ = 0;
    hz_ = host_.rtc_clock_hz();
    phase_ = std::min(phase_, prl_);
    host_.system_schedule_changed();
}

void Stm32F1Rtc::clock_changed()
{
    anchor();
}

std::uint64_t Stm32F1Rtc::ns_after(std::uint64_t rtc_cycles) const
{
    return anchor_ns_ + scale(rtc_cycles, ns_per_s, hz_, true);
}

void Stm32F1Rtc::update_irq()
{
    host_.system_irq(3, (flags_ & crh_ & 7u) != 0);
}

std::uint64_t Stm32F1Rtc::next_event() const
{
    if (hz_ == 0) {
        return no_event;
    }
    // Ticks to the next second that matters: every second with SECIE, else the
    // alarm (also for EXTI line 17) and the overflow.
    std::uint64_t ticks = static_cast<std::uint32_t>(alr_ - cnt_);
    ticks = ticks == 0 ? 0x100000000ull : ticks;
    if (crh_ & 1u) {
        ticks = 1;
    }
    if (crh_ & 4u) {
        ticks = std::min<std::uint64_t>(ticks, 0x100000000ull - cnt_);
    }
    const std::uint64_t period = static_cast<std::uint64_t>(prl_) + 1;
    const std::uint64_t cycles = counted_ + ticks * period - phase_;
    return host_.system_cycles_at(ns_after(cycles));
}

void Stm32F1Rtc::on_time(std::uint64_t)
{
    advance();
}

std::uint32_t Stm32F1Rtc::read32(std::uint32_t offset)
{
    switch (offset) {
    case 0x00: return crh_;
    case 0x04:
        advance();
        return flags_ | 0x08u /* RSF */ | (config_ ? 0x10u : 0u) | 0x20u /* RTOFF */;
    case 0x10: advance(); return ((prl_ - phase_) >> 16) & 0xFu; // DIVH
    case 0x14: advance(); return (prl_ - phase_) & 0xFFFFu;        // DIVL
    case 0x18: advance(); return cnt_ >> 16;
    case 0x1C: advance(); return cnt_ & 0xFFFFu;
    default: return 0; // PRL and ALR are write-only
    }
}

void Stm32F1Rtc::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    if (!host_.backup_writable()) {
        if (!warned_locked_) {
            warned_locked_ = true;
            host_.system_diagnostic("The firmware wrote an RTC register without PWR_CR.DBP set; the write is "
                "ignored, as on a real STM32F103 (call HAL_PWR_EnableBkUpAccess first).");
        }
        return;
    }
    advance();
    switch (offset) {
    case 0x00:
        crh_ = merge(crh_, value, mask) & 7u;
        update_irq();
        host_.system_schedule_changed();
        return;
    case 0x04:
        // SECF, ALRF, OWF and RSF are cleared by writing 0; RSF is set again at once.
        flags_ &= (value | ~mask) | ~7u;
        if (mask & 0x10u) {
            config_ = (value & 0x10u) != 0;
        }
        update_irq();
        return;
    default: break;
    }
    if (offset >= 0x08 && offset <= 0x24 && offset != 0x10 && offset != 0x14) {
        if (!config_) {
            if (warned_config_) {
                return;
            }
            warned_config_ = true;
            host_.system_diagnostic("The firmware wrote an RTC prescaler, counter or alarm register outside "
                "configuration mode (RTC_CRL.CNF); the write is ignored, as on a real STM32F103.");
            return;
        }
        const std::uint32_t low = value & mask & 0xFFFFu;
        switch (offset) {
        case 0x08: prl_ = (prl_ & 0xFFFFu) | ((value & mask & 0xFu) << 16); break;
        case 0x0C: prl_ = (prl_ & 0xF0000u) | low; break;
        case 0x18: cnt_ = (cnt_ & 0xFFFFu) | (low << 16); break;
        case 0x1C: cnt_ = (cnt_ & 0xFFFF0000u) | low; break;
        case 0x20: alr_ = (alr_ & 0xFFFFu) | (low << 16); break;
        case 0x24: alr_ = (alr_ & 0xFFFF0000u) | low; break;
        default: break;
        }
        anchor();
    }
}

// ---------------------------------------------------------------------------
// BKP (RM0008 5)

void Stm32F1Bkp::backup_reset()
{
    data_.fill(0);
    rtccr_ = cr_ = csr_ = 0;
}

std::uint32_t Stm32F1Bkp::read32(std::uint32_t offset)
{
    if (offset >= 0x04 && offset <= 0x28) {
        return data_[(offset - 0x04) / 4];
    }
    switch (offset) {
    case 0x2C: return rtccr_;
    case 0x30: return cr_;
    case 0x34: return csr_;
    default: return 0;
    }
}

void Stm32F1Bkp::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    if (!host_.backup_writable()) {
        if (!warned_locked_) {
            warned_locked_ = true;
            host_.system_diagnostic("The firmware wrote a backup register without PWR_CR.DBP set; the write is "
                "ignored, as on a real STM32F103 (call HAL_PWR_EnableBkUpAccess first).");
        }
        return;
    }
    if (offset >= 0x04 && offset <= 0x28) {
        auto& word = data_[(offset - 0x04) / 4];
        word = static_cast<std::uint16_t>(merge(word, value, mask) & 0xFFFFu);
        return;
    }
    switch (offset) {
    case 0x2C: rtccr_ = merge(rtccr_, value, mask) & 0x3FFu; break;
    case 0x30:
        cr_ = merge(cr_, value, mask) & 3u;
        if ((cr_ & 1u) && !warned_tamper_) {
            warned_tamper_ = true;
            host_.system_diagnostic("The firmware enabled the tamper pin (BKP_CR.TPE), which SimNodus does not "
                "simulate yet.");
        }
        break;
    case 0x34: csr_ = merge(csr_, value, mask) & 0x4u; break; // TPIE; CTE and CTI clear nothing to clear
    default: break;
    }
}

} // namespace simnodus::emu
