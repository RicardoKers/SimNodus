// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f4_system.hpp"

#include <algorithm>

namespace simnodus::emu {

namespace {

constexpr std::uint64_t ns_per_s = 1000000000ull;

// value * mul / div without overflow for value * mul up to about 2^96, rounded down or up.
std::uint64_t scale(std::uint64_t value, std::uint64_t mul, std::uint64_t div, bool round_up)
{
    const std::uint64_t q = value / div;
    const std::uint64_t r = value % div;
    const std::uint64_t part = r * mul; // r < div; the callers keep div * mul below 2^64
    return q * mul + part / div + (round_up && part % div != 0 ? 1u : 0u);
}

std::uint32_t bcd(int value)
{
    return static_cast<std::uint32_t>((value / 10) << 4 | (value % 10));
}

int binary(std::uint32_t value)
{
    return static_cast<int>((value >> 4) * 10 + (value & 15u));
}

int days_in_month(int month, int year)
{
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && year % 4 == 0) {
        return 29; // 2000 to 2099: every fourth year
    }
    return month >= 1 && month <= 12 ? days[month - 1] : 31;
}

// Registers (RM0090 26.6).
enum : std::uint32_t {
    TR = 0x00, DR = 0x04, CR = 0x08, ISR = 0x0C, PRER = 0x10, WUTR = 0x14, CALIBR = 0x18, ALRMAR = 0x1C,
    ALRMBR = 0x20, WPR = 0x24, SSR = 0x28, SHIFTR = 0x2C, TSTR = 0x30, TSDR = 0x34, TSSSR = 0x38, CALR = 0x3C,
    TAFCR = 0x40, ALRMASSR = 0x44, ALRMBSSR = 0x48, BKP0R = 0x50
};

constexpr std::uint32_t CR_FMT = 1u << 6, CR_DCE = 1u << 7, CR_ALRAE = 1u << 8, CR_ALRBE = 1u << 9,
                        CR_WUTE = 1u << 10, CR_TSE = 1u << 11, CR_ALRAIE = 1u << 12, CR_ALRBIE = 1u << 13,
                        CR_WUTIE = 1u << 14, CR_ADD1H = 1u << 16, CR_SUB1H = 1u << 17, CR_COE = 1u << 23;
constexpr std::uint32_t ISR_ALRAWF = 1u << 0, ISR_ALRBWF = 1u << 1, ISR_WUTWF = 1u << 2, ISR_INITS = 1u << 4,
                        ISR_RSF = 1u << 5, ISR_INITF = 1u << 6, ISR_INIT = 1u << 7, ISR_ALRAF = 1u << 8,
                        ISR_ALRBF = 1u << 9, ISR_WUTF = 1u << 10;
constexpr std::uint32_t ISR_FLAGS = 0x7F00u; // ALRAF .. TAMP2F: rc_w0

enum : std::uint32_t {
    F_LOCKED = 1u << 0, F_NO_CLOCK = 1u << 1, F_DBP = 1u << 2, F_OUTPUT = 1u << 3, F_CALIBRATION = 1u << 4,
    F_TAMPER = 1u << 5, F_SUBSECOND = 1u << 6, F_SHIFT = 1u << 7, F_NOT_INIT = 1u << 8
};

} // namespace

// ---------------------------------------------------------------------------
// RTC

void Stm32F4Rtc::report(std::uint32_t feature, const std::string& text)
{
    if (!(reported_ & feature)) {
        reported_ |= feature;
        host_.system_diagnostic("RTC: " + text);
    }
}

void Stm32F4Rtc::reset()
{
    advance();
    locked_ = true;
    key_step_ = 0;
}

void Stm32F4Rtc::backup_reset()
{
    cal_ = {};
    cr_ = 0;
    prer_ = 0x007F00FFu;
    wutr_ = 0xFFFFu;
    calibr_ = alrmar_ = alrmbr_ = alrmassr_ = alrmbssr_ = calr_ = tafcr_ = 0;
    flags_ = 0;
    init_ = false;
    locked_ = true;
    key_step_ = 0;
    bkp_ = {};
    hz_ = host_.rtc_clock_hz();
    anchor_ns_ = host_.system_now_ns();
    counted_ = 0;
    sub_ = 0;
    wakeups_ = 0;
    host_.system_schedule_changed();
}

std::uint64_t Stm32F4Rtc::rtc_ns(std::uint64_t rtc_cycles) const
{
    return scale(rtc_cycles, ns_per_s, hz_, true);
}

void Stm32F4Rtc::advance()
{
    if (!counting()) {
        return;
    }
    const std::uint64_t now = host_.system_now_ns();
    if (now <= anchor_ns_) {
        return;
    }
    const std::uint64_t ticks = scale(now - anchor_ns_, hz_, ns_per_s, false) / prediv_a();
    if (ticks <= counted_) {
        return;
    }
    std::uint64_t total = sub_ + (ticks - counted_);
    counted_ = ticks;
    const std::uint32_t per_second = prediv_s();
    while (total >= per_second) {
        total -= per_second;
        second_tick();
    }
    sub_ = static_cast<std::uint32_t>(total);
}

void Stm32F4Rtc::anchor()
{
    advance();
    if (hz_ != 0) {
        anchor_ns_ += rtc_ns(counted_ * prediv_a()); // from the last tick, so no fraction of one is lost
    }
    const std::uint32_t hz = host_.rtc_clock_hz();
    if (hz != hz_) {
        anchor_ns_ = host_.system_now_ns();
        hz_ = hz;
    }
    counted_ = 0;
    host_.system_schedule_changed();
}

void Stm32F4Rtc::clock_changed()
{
    anchor();
    if (cr_ & CR_WUTE) {
        start_wakeup();
    }
}

void Stm32F4Rtc::second_tick()
{
    Calendar& c = cal_;
    if (++c.second >= 60) {
        c.second = 0;
        if (++c.minute >= 60) {
            c.minute = 0;
            if (++c.hour >= 24) {
                c.hour = 0;
                c.weekday = c.weekday % 7 + 1;
                if (++c.day > days_in_month(c.month, c.year)) {
                    c.day = 1;
                    if (++c.month > 12) {
                        c.month = 1;
                        c.year = (c.year + 1) % 100;
                    }
                }
            }
        }
    }
    if (cr_ & CR_ALRAE) {
        check_alarm(0);
    }
    if (cr_ & CR_ALRBE) {
        check_alarm(1);
    }
    if ((cr_ & CR_WUTE) && wakeup_on_seconds() && --wakeup_left_ == 0) {
        wakeup_event(); // the counter counts the 1 Hz clock's edges, in step with the calendar
    }
}

void Stm32F4Rtc::wakeup_event()
{
    flags_ |= ISR_WUTF;
    if (cr_ & CR_WUTIE) {
        host_.system_exti(22, true); // RTC wakeup: EXTI line 22
        host_.system_exti(22, false);
    }
    if (wakeup_on_seconds()) {
        wakeup_left_ = (wutr_ & 0xFFFFu) + 1u + ((cr_ & 7u) >= 6 ? 0x10000u : 0u);
    }
}

void Stm32F4Rtc::check_alarm(int which)
{
    const std::uint32_t alarm = which == 0 ? alrmar_ : alrmbr_;
    const std::uint32_t tr = time_register();
    if (!(alarm & (1u << 7)) && (alarm & 0x7Fu) != (tr & 0x7Fu)) {
        return; // MSK1: seconds
    }
    if (!(alarm & (1u << 15)) && ((alarm >> 8) & 0x7Fu) != ((tr >> 8) & 0x7Fu)) {
        return; // MSK2: minutes
    }
    if (!(alarm & (1u << 23)) && ((alarm >> 16) & 0x7Fu) != ((tr >> 16) & 0x7Fu)) {
        return; // MSK3: hours and PM
    }
    if (!(alarm & (1u << 31))) {
        if (alarm & (1u << 30)) { // WDSEL: the weekday in DU
            if (static_cast<int>((alarm >> 24) & 15u) != cal_.weekday) {
                return;
            }
        } else if (((alarm >> 24) & 0x3Fu) != bcd(cal_.day)) {
            return;
        }
    }
    if (((which == 0 ? alrmassr_ : alrmbssr_) >> 24) & 15u) {
        report(F_SUBSECOND, "the subsecond comparison of the alarms is not simulated yet; the alarm fires on the second.");
    }
    flags_ |= which == 0 ? ISR_ALRAF : ISR_ALRBF;
    if (cr_ & (which == 0 ? CR_ALRAIE : CR_ALRBIE)) {
        host_.system_exti(17, true); // RTC alarm: EXTI line 17
        host_.system_exti(17, false);
    }
}

void Stm32F4Rtc::start_wakeup()
{
    wakeup_start_ns_ = host_.system_now_ns();
    wakeups_ = 0;
    wakeup_left_ = (wutr_ & 0xFFFFu) + 1u + ((cr_ & 7u) >= 6 ? 0x10000u : 0u);
    host_.system_schedule_changed();
}

std::uint64_t Stm32F4Rtc::wakeup_ns() const
{
    const std::uint64_t units = std::uint64_t{wutr_ & 0xFFFFu} + 1u;
    const std::uint64_t divider = 16u >> (cr_ & 3u); // RTCCLK / 16, 8, 4, 2
    return wakeup_start_ns_ + rtc_ns((wakeups_ + 1) * units * divider);
}

std::uint64_t Stm32F4Rtc::next_event() const
{
    if (hz_ == 0) {
        return no_event;
    }
    std::uint64_t next = no_event;
    if (counting() && ((cr_ & (CR_ALRAE | CR_ALRBE)) || ((cr_ & CR_WUTE) && wakeup_on_seconds()))) {
        const std::uint64_t ticks = counted_ + (prediv_s() - sub_);
        next = host_.system_cycles_at(anchor_ns_ + rtc_ns(ticks * prediv_a()));
    }
    if ((cr_ & CR_WUTE) && !wakeup_on_seconds()) {
        next = std::min(next, host_.system_cycles_at(wakeup_ns()));
    }
    return next;
}

void Stm32F4Rtc::on_time(std::uint64_t cycle)
{
    (void)cycle;
    advance();
    const std::uint64_t now = host_.system_now_ns();
    while ((cr_ & CR_WUTE) && !wakeup_on_seconds() && hz_ != 0 && wakeup_ns() <= now) {
        ++wakeups_;
        wakeup_event();
    }
}

std::uint32_t Stm32F4Rtc::time_register() const
{
    int hour = cal_.hour;
    std::uint32_t pm = 0;
    if (cr_ & CR_FMT) {
        pm = hour >= 12 ? 1u : 0u;
        hour %= 12;
        if (hour == 0) {
            hour = 12;
        }
    }
    return bcd(cal_.second) | bcd(cal_.minute) << 8 | bcd(hour) << 16 | pm << 22;
}

std::uint32_t Stm32F4Rtc::date_register() const
{
    return bcd(cal_.day) | bcd(cal_.month) << 8 | static_cast<std::uint32_t>(cal_.weekday) << 13
        | bcd(cal_.year) << 16;
}

void Stm32F4Rtc::set_time(std::uint32_t tr)
{
    cal_.second = std::min(binary(tr & 0x7Fu), 59);
    cal_.minute = std::min(binary((tr >> 8) & 0x7Fu), 59);
    int hour = binary((tr >> 16) & 0x3Fu);
    if (cr_ & CR_FMT) {
        hour = hour % 12 + ((tr >> 22) & 1u ? 12 : 0);
    }
    cal_.hour = std::min(hour, 23);
}

void Stm32F4Rtc::set_date(std::uint32_t dr)
{
    cal_.year = binary((dr >> 16) & 0xFFu) % 100;
    cal_.weekday = std::max(1, static_cast<int>((dr >> 13) & 7u));
    cal_.month = std::clamp(binary((dr >> 8) & 0x1Fu), 1, 12);
    cal_.day = std::clamp(binary(dr & 0x3Fu), 1, days_in_month(cal_.month, cal_.year));
}

void Stm32F4Rtc::add_hour(int delta)
{
    if (delta > 0) {
        for (int i = 0; i < 3600; ++i) {
            const std::uint32_t enabled = cr_ & (CR_ALRAE | CR_ALRBE);
            cr_ &= ~enabled; // the jump does not raise the alarms on the way
            second_tick();
            cr_ |= enabled;
        }
    } else if (cal_.hour > 0) {
        --cal_.hour; // SUB1H acts only when the hour is not 0
    }
}

bool Stm32F4Rtc::writable(bool protected_register)
{
    if (!host_.backup_writable()) {
        report(F_DBP, "a write was ignored: the backup domain is write-protected (set PWR_CR.DBP first, as "
                      "HAL_PWR_EnableBkUpAccess does).");
        return false;
    }
    if (protected_register && locked_) {
        report(F_LOCKED, "a write was ignored: the RTC registers are write-protected (write 0xCA, then 0x53 to "
                         "RTC_WPR first).");
        return false;
    }
    return true;
}

std::uint32_t Stm32F4Rtc::read32(std::uint32_t offset)
{
    advance(); // catching up changes nothing a read could not see later, so peeks do it too
    if (offset >= BKP0R && offset < BKP0R + 4 * bkp_.size()) {
        return bkp_[(offset - BKP0R) / 4];
    }
    switch (offset) {
    case TR: return time_register();
    case DR: return date_register();
    case CR: return cr_;
    case ISR: {
        std::uint32_t isr = flags_;
        isr |= (cr_ & CR_ALRAE) ? 0u : ISR_ALRAWF;
        isr |= (cr_ & CR_ALRBE) ? 0u : ISR_ALRBWF;
        isr |= (cr_ & CR_WUTE) ? 0u : ISR_WUTWF;
        isr |= cal_.year != 0 ? ISR_INITS : 0u;
        isr |= init_ ? ISR_INIT : 0u;
        isr |= init_ && hz_ != 0 ? ISR_INITF : 0u;
        isr |= init_ ? 0u : ISR_RSF; // the shadow registers follow the calendar at once
        return isr;
    }
    case PRER: return prer_;
    case WUTR: return wutr_;
    case CALIBR: return calibr_;
    case ALRMAR: return alrmar_;
    case ALRMBR: return alrmbr_;
    case SSR: return prediv_s() - 1 - std::min(sub_, prediv_s() - 1);
    case CALR: return calr_;
    case TAFCR: return tafcr_;
    case ALRMASSR: return alrmassr_;
    case ALRMBSSR: return alrmbssr_;
    default: return 0; // WPR reads 0; SHIFTR, TSTR, TSDR, TSSSR: nothing is stamped
    }
}

void Stm32F4Rtc::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    advance();
    if (offset >= BKP0R && offset < BKP0R + 4 * bkp_.size()) {
        if (writable(false)) {
            auto& r = bkp_[(offset - BKP0R) / 4];
            r = merge(r, value, mask);
        }
        return;
    }
    if (offset == WPR) {
        const std::uint32_t key = value & mask & 0xFFu;
        if (key == 0xCA) {
            key_step_ = 1;
        } else if (key == 0x53 && key_step_ == 1) {
            key_step_ = 0;
            locked_ = false;
        } else {
            key_step_ = 0;
            locked_ = true; // any other value locks again
        }
        return;
    }
    if (offset == ISR) {
        if (!writable(false)) {
            return;
        }
        // The flags are rc_w0 and not write-protected; INIT is.
        flags_ &= (value | ~mask) | ~ISR_FLAGS;
        if (mask & ISR_INIT) {
            const bool init = (value & ISR_INIT) != 0;
            if (init != init_ && writable(true)) {
                if (init) {
                    advance();
                    init_ = true;
                    if (hz_ == 0) {
                        report(F_NO_CLOCK, "initialization mode was requested, but RTCCLK does not run (RCC_BDCR: "
                                           "RTCSEL, RTCEN, and the LSE, LSI or HSE ready): INITF never sets.");
                    }
                } else {
                    init_ = false;
                    hz_ = host_.rtc_clock_hz();
                    anchor_ns_ = host_.system_now_ns(); // the calendar starts again from what was written
                    counted_ = 0;
                    sub_ = 0;
                    host_.system_schedule_changed();
                }
            }
        }
        return;
    }
    if (!writable(true)) {
        return;
    }
    const bool initf = init_ && hz_ != 0;
    switch (offset) {
    case TR:
    case DR:
    case PRER:
        if (!initf) {
            report(F_NOT_INIT, "TR, DR and PRER can be written only in initialization mode (INITF); a write was "
                               "ignored.");
            return;
        }
        if (offset == TR) {
            set_time(merge(time_register(), value, mask) & 0x007F7F7Fu);
        } else if (offset == DR) {
            set_date(merge(date_register(), value, mask) & 0x00FFFF3Fu);
        } else {
            prer_ = merge(prer_, value, mask) & 0x007F7FFFu;
            sub_ = 0;
        }
        return;
    case CR: {
        const std::uint32_t old = cr_;
        const std::uint32_t v = merge(cr_, value, mask);
        cr_ = v & 0x00FFFFFFu & ~(CR_ADD1H | CR_SUB1H); // ADD1H and SUB1H act and read 0
        if (v & CR_ADD1H) {
            add_hour(1);
        }
        if (v & CR_SUB1H) {
            add_hour(-1);
        }
        if (cr_ & (CR_COE | (3u << 21))) {
            report(F_OUTPUT, "the calibration and alarm outputs on PC13 (RTC_AF1) are not simulated yet.");
        }
        if (cr_ & (CR_TSE | CR_DCE)) {
            report(F_CALIBRATION, "the time stamp and coarse calibration are not simulated yet.");
        }
        if ((cr_ & CR_WUTE) && !(old & CR_WUTE)) {
            start_wakeup();
        }
        host_.system_schedule_changed();
        return;
    }
    case WUTR:
        if (!(cr_ & CR_WUTE)) {
            wutr_ = merge(wutr_, value, mask) & 0xFFFFu;
        }
        return;
    case CALIBR:
        calibr_ = merge(calibr_, value, mask) & 0xBFu;
        return;
    case ALRMAR:
        if (!(cr_ & CR_ALRAE)) {
            alrmar_ = merge(alrmar_, value, mask);
        }
        return;
    case ALRMBR:
        if (!(cr_ & CR_ALRBE)) {
            alrmbr_ = merge(alrmbr_, value, mask);
        }
        return;
    case SHIFTR:
        report(F_SHIFT, "the shift register (synchronization to a reference) is not simulated yet.");
        return;
    case CALR:
        calr_ = merge(calr_, value, mask) & 0xE1FFu;
        if (calr_) {
            report(F_CALIBRATION, "smooth calibration is not simulated yet.");
        }
        return;
    case TAFCR:
        tafcr_ = merge(tafcr_, value, mask) & 0x00FCFFFFu;
        if (tafcr_ & 0x0Du) {
            report(F_TAMPER, "the tamper inputs are not simulated yet.");
        }
        return;
    case ALRMASSR:
        if (!(cr_ & CR_ALRAE)) {
            alrmassr_ = merge(alrmassr_, value, mask) & 0x0F007FFFu;
        }
        return;
    case ALRMBSSR:
        if (!(cr_ & CR_ALRBE)) {
            alrmbssr_ = merge(alrmbssr_, value, mask) & 0x0F007FFFu;
        }
        return;
    default: return;
    }
}

// ---------------------------------------------------------------------------
// RNG

void Stm32F4Rng::reset()
{
    cr_ = 0;
    drdy_ = false;
    dr_ = 0;
    state_ = 0x2545F491u; // the fixed seed: every run gives the same numbers
    pending_ = false;
    update_irq();
}

void Stm32F4Rng::schedule()
{
    ready_ns_ = host_.system_now_ns() + 834; // 40 cycles of the 48 MHz clock
    pending_ = true;
    host_.system_schedule_changed();
}

std::uint64_t Stm32F4Rng::next_event() const
{
    return pending_ && (cr_ & 4u) ? host_.system_cycles_at(ready_ns_) : no_event;
}

void Stm32F4Rng::on_time(std::uint64_t cycle)
{
    (void)cycle;
    if (!pending_ || !(cr_ & 4u) || host_.system_now_ns() < ready_ns_) {
        return;
    }
    pending_ = false;
    // xorshift32 (Marsaglia): a fixed sequence, not random numbers.
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    dr_ = state_;
    drdy_ = true;
    update_irq();
}

void Stm32F4Rng::update_irq()
{
    host_.system_irq(80, (cr_ & 8u) && drdy_); // HASH_RNG
}

std::uint32_t Stm32F4Rng::read32(std::uint32_t offset)
{
    switch (offset) {
    case 0x00: return cr_;
    case 0x04: return drdy_ ? 1u : 0u;
    case 0x08:
        if (!peeking() && drdy_) {
            drdy_ = false;
            update_irq();
            schedule();
        }
        return dr_;
    default: return 0;
    }
}

void Stm32F4Rng::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    if (offset == 0x00) {
        const std::uint32_t old = cr_;
        cr_ = merge(cr_, value, mask) & 0x0Cu;
        if ((cr_ & 4u) && !(old & 4u)) {
            if (!reported_) {
                reported_ = true;
                host_.system_diagnostic("RNG: SimNodus gives a fixed pseudo-random sequence, the same on every run "
                                        "(so that runs repeat), not random numbers.");
            }
            if (!drdy_) {
                schedule();
            }
        }
        update_irq();
    }
    // SR: CEIS and SEIS are rc_w0 and never set.
}

} // namespace simnodus::emu
