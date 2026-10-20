// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 system peripherals for the SimNodus emulator (SN-187; RM0008):
//   CRC (chapter 4): CRC-32 with the polynomial 0x04C11DB7 over 32-bit words,
//     IDR, the RESET bit.
//   IWDG (19): key register, prescaler and reload, a down-counter at LSI
//     (40 kHz, the typical value) that resets the chip at 0. PVU and RVU read 0:
//     new values apply at once.
//   WWDG (20): 7-bit down-counter at PCLK1 / 4096 / 2^WDGTB, reset when T6
//     clears or when the counter is refreshed above the window, early wakeup
//     interrupt at 0x40.
//   RTC (18): prescaler and 32-bit counter at RTCCLK (LSE 32.768 kHz, LSI
//     40 kHz or HSE / 128), alarm, second, alarm and overflow flags and
//     interrupts, the alarm on EXTI line 17. Writes in configuration mode apply
//     at once (RTOFF always reads 1, RSF stays set).
//   BKP (5): ten 16-bit data registers, RTCCR, CR, CSR.
// The RTC core and BKP are in the backup domain: they keep their contents
// across system resets and are cleared by a power-on or a backup domain reset;
// their registers can be written only with PWR_CR.DBP set. Not simulated
// (reported once): the tamper pin, the RTC calibration output, watchdog stop
// in debug (time stops anyway while the debugger halts the core).
#pragma once

#include "bus.hpp"
#include "timed_device.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace simnodus::emu {

// What the system peripherals need from the machine.
class SystemHost {
public:
    virtual ~SystemHost() = default;
    virtual std::uint64_t system_now_ns() const = 0;
    virtual std::uint64_t system_cycles_at(std::uint64_t ns) const = 0;
    virtual std::uint32_t pclk1_hz() const = 0;
    // RTCCLK in Hz, 0 while the RTC does not count (RTCEN off, no clock selected or not ready).
    virtual std::uint32_t rtc_clock_hz() const = 0;
    virtual bool backup_writable() const = 0; // PWR_CR.DBP
    virtual void system_irq(int irq, bool level) = 0;
    virtual void system_exti(int line, bool level) = 0;
    virtual void watchdog_reset(bool independent) = 0;
    virtual void system_schedule_changed() = 0;
    virtual void system_diagnostic(const std::string& text) = 0;
};

// Registers as 32-bit words; byte and halfword accesses become partial writes.
class WordDevice : public Device {
public:
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;

protected:
    virtual std::uint32_t read32(std::uint32_t offset) = 0;
    virtual void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) = 0;
    static std::uint32_t merge(std::uint32_t old, std::uint32_t value, std::uint32_t mask)
    {
        return (old & ~mask) | (value & mask);
    }
};

class Stm32F1Crc : public WordDevice {
public:
    const char* name() const override { return "CRC"; }
    void reset() override;
    // CRC-32 of one word as the peripheral computes it (MSB first, no reflection).
    static std::uint32_t step(std::uint32_t crc, std::uint32_t word);

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    std::uint32_t dr_ = 0xFFFFFFFFu;
    std::uint32_t idr_ = 0;
};

class Stm32F1Iwdg : public WordDevice, public TimedDevice {
public:
    static constexpr std::uint32_t lsi_hz = 40000;
    explicit Stm32F1Iwdg(SystemHost& host) : host_(host) {}
    const char* name() const override { return "IWDG"; }
    void reset() override;
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    bool running() const { return running_; }

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    void reload();

    SystemHost& host_;
    bool running_ = false;
    bool unlocked_ = false; // 0x5555 written: PR and RLR accept writes
    std::uint32_t pr_ = 0;
    std::uint32_t rlr_ = 0xFFF;
    std::uint64_t deadline_ns_ = 0;
};

class Stm32F1Wwdg : public WordDevice, public TimedDevice {
public:
    explicit Stm32F1Wwdg(SystemHost& host) : host_(host) {}
    const char* name() const override { return "WWDG"; }
    void reset() override;
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    // PCLK1 changed: the counter continues at the new rate.
    void clocks_changed();

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    std::uint32_t ticks_since_load() const;
    std::uint32_t counter() const;   // T[6:0] now
    void load(std::uint32_t value);  // the counter starts from `value` now
    void update_irq();

    SystemHost& host_;
    bool active_ = false; // WDGA
    std::uint32_t loaded_ = 0x7F;
    std::uint64_t loaded_ns_ = 0;
    std::uint32_t cfr_ = 0x7F;
    bool ewif_ = false;
    bool ewi_done_ = false; // the early wakeup of the current count was raised
    bool warned_early_ = false;
};

class Stm32F1Rtc : public WordDevice, public TimedDevice {
public:
    explicit Stm32F1Rtc(SystemHost& host) : host_(host) {}
    const char* name() const override { return "RTC"; }
    // System reset: the interface registers (CRH, CRL) only.
    void reset() override;
    // Power-on or backup domain reset: the counter, prescaler and alarm too.
    void backup_reset();
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    // RTCCLK changed (source, enable): the counter continues at the new rate.
    void clock_changed();

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    void advance();          // counts the RTCCLK cycles up to now and raises the flags
    void anchor();           // after advance(): new settings count from now
    void update_irq();
    std::uint64_t ns_after(std::uint64_t rtc_cycles) const; // time at `rtc_cycles` after the anchor

    SystemHost& host_;
    std::uint32_t crh_ = 0;
    std::uint32_t flags_ = 0; // CRL: SECF, ALRF, OWF
    bool config_ = false;     // CNF
    std::uint32_t prl_ = 0x8000;
    std::uint32_t cnt_ = 0;
    std::uint32_t alr_ = 0xFFFFFFFFu;
    std::uint32_t phase_ = 0; // RTCCLK cycles counted in the current second
    std::uint32_t hz_ = 0;    // RTCCLK at the anchor
    std::uint64_t anchor_ns_ = 0;
    std::uint64_t counted_ = 0; // RTCCLK cycles since the anchor already counted
    bool warned_locked_ = false;
    bool warned_config_ = false;
};

class Stm32F1Bkp : public WordDevice {
public:
    explicit Stm32F1Bkp(SystemHost& host) : host_(host) {}
    const char* name() const override { return "BKP"; }
    void reset() override {} // the backup domain keeps its contents
    void backup_reset();

protected:
    std::uint32_t read32(std::uint32_t offset) override;
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    SystemHost& host_;
    std::array<std::uint16_t, 10> data_{};
    std::uint32_t rtccr_ = 0, cr_ = 0, csr_ = 0;
    bool warned_tamper_ = false;
    bool warned_locked_ = false;
};

} // namespace simnodus::emu
