// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F4 system peripherals for the SimNodus emulator (E-12, SN-298).
//
// The RTC (RM0090 chapter 26): the calendar in BCD (time, date, weekday,
// 24-hour or AM/PM format) counting at RTCCLK / (PREDIV_A + 1) /
// (PREDIV_S + 1), the subsecond register, initialization mode (INITF sets
// only while RTCCLK runs, as on the chip), the write protection key, the
// summer-time bits, alarms A and B with their masks (on EXTI line 17), the
// wakeup timer on RTCCLK / 16 to / 2 or on the 1 Hz clock (on EXTI line 22),
// and the twenty backup registers. The RTC is in the backup domain: it keeps
// its contents across system resets; a power-on or a backup domain reset
// (RCC_BDCR.BDRST) clears it, and writes need PWR_CR.DBP.
// Not simulated (reported once): the time stamp and the tamper inputs, the
// calibration output and the alarm output on PC13, smooth and coarse
// calibration, the shift register, subsecond alarm comparison.
//
// The RNG (RM0090 chapter 24): a new 32-bit number about 40 cycles of a
// 48 MHz clock after the last read, DRDY and the interrupt (IRQ 80). The
// numbers come from a fixed pseudo-random sequence, the same on every run,
// so that runs repeat; the log says so once. The clock and seed error flags
// never set.
#pragma once

#include "stm32f1_system.hpp" // SystemHost, WordDevice

#include <array>
#include <cstdint>
#include <string>

namespace simnodus::emu {

class Stm32F4Rtc : public WordDevice, public TimedDevice {
public:
    explicit Stm32F4Rtc(SystemHost& host) : host_(host) { backup_reset(); }
    const char* name() const override { return "RTC"; }
    // System reset: the write protection locks again; the calendar keeps counting.
    void reset() override;
    // Power-on or backup domain reset: everything, the backup registers too.
    void backup_reset();
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    // RTCCLK changed (source, enable, ready): the calendar continues at the new rate.
    void clock_changed();

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    struct Calendar {
        int year = 0, month = 1, day = 1, weekday = 1; // weekday: 1 Monday .. 7 Sunday
        int hour = 0, minute = 0, second = 0;          // 24-hour
    };

    std::uint32_t prediv_a() const { return ((prer_ >> 16) & 0x7Fu) + 1; }
    std::uint32_t prediv_s() const { return (prer_ & 0x7FFFu) + 1; }
    bool counting() const { return hz_ != 0 && !init_; }
    std::uint64_t rtc_ns(std::uint64_t rtc_cycles) const; // time of `rtc_cycles` of RTCCLK at hz_
    void advance();   // counts the time up to now
    void anchor();    // after advance(): new settings count from the last tick
    void second_tick();
    void check_alarm(int which);
    void start_wakeup();
    bool wakeup_on_seconds() const { return (cr_ & 7u) >= 4; } // WUCKSEL 10x, 11x: ck_spre
    std::uint64_t wakeup_ns() const; // time of the next wakeup event on RTCCLK / 16 to / 2
    void wakeup_event();
    std::uint32_t time_register() const;
    std::uint32_t date_register() const;
    void set_time(std::uint32_t tr);
    void set_date(std::uint32_t dr);
    void add_hour(int delta);
    bool writable(bool protected_register);
    void report(std::uint32_t feature, const std::string& text);

    SystemHost& host_;
    Calendar cal_;
    std::uint32_t cr_ = 0, prer_ = 0x007F00FFu, wutr_ = 0xFFFFu, calibr_ = 0;
    std::uint32_t alrmar_ = 0, alrmbr_ = 0, alrmassr_ = 0, alrmbssr_ = 0, calr_ = 0, tafcr_ = 0;
    std::uint32_t flags_ = 0; // ISR: ALRAF, ALRBF, WUTF and the others SimNodus does not raise
    bool init_ = false;       // INIT (INITF follows while RTCCLK runs)
    bool locked_ = true;
    int key_step_ = 0;        // 0xCA seen
    std::array<std::uint32_t, 20> bkp_{};
    // Time keeping: ck_apre ticks since the anchor.
    std::uint32_t hz_ = 0;
    std::uint64_t anchor_ns_ = 0;
    std::uint64_t counted_ = 0;  // ck_apre ticks since the anchor already counted
    std::uint32_t sub_ = 0;      // ck_apre ticks into the current second
    std::uint64_t wakeup_start_ns_ = 0;
    std::uint64_t wakeups_ = 0;  // wakeup events since the start
    std::uint32_t wakeup_left_ = 0; // on ck_spre: seconds to the next event
    std::uint32_t reported_ = 0;
};

class Stm32F4Rng : public WordDevice, public TimedDevice {
public:
    explicit Stm32F4Rng(SystemHost& host) : host_(host) { reset(); }
    const char* name() const override { return "RNG"; }
    void reset() override;
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    void schedule(); // the next number is ready about 40 cycles of 48 MHz from now
    void update_irq();

    SystemHost& host_;
    std::uint32_t cr_ = 0;
    bool drdy_ = false;
    std::uint32_t dr_ = 0;
    std::uint32_t state_ = 0;
    std::uint64_t ready_ns_ = 0;
    bool pending_ = false;
    bool reported_ = false;
};

} // namespace simnodus::emu
