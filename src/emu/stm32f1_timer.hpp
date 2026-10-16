// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 timers TIM1 (advanced) and TIM2-TIM4 (general purpose) for the
// SimNodus emulator (SN-177; RM0008 chapters 14 and 15). Simulated: time base
// (up and down counting, prescaler, auto-reload with preload, one-pulse mode,
// update disable/request source, repetition counter of TIM1), output compare
// (frozen, active/inactive/toggle on match, forced levels, PWM 1 and 2, CCR
// preload, polarity, output enable, TIM1 main output enable and complementary
// outputs without dead time), input capture (rising/falling edge, capture
// prescaler, overcapture), status flags, interrupts and the channel pins with
// the AFIO remaps. Not simulated yet (reported once): center-aligned counting,
// slave modes and encoder interface, input filters, dead time, break input,
// DMA requests (SN-179).
#pragma once

#include "bus.hpp"
#include "timed_device.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace simnodus::emu {

// What a timer needs from the machine.
class TimerHost {
public:
    virtual ~TimerHost() = default;
    virtual std::uint64_t now_cycles() const = 0;
    // HCLK cycles per timer kernel clock tick: APB divider 1 -> 1; 2, 4, 8, 16 -> divider / 2.
    virtual std::uint32_t hclk_per_kernel_tick(bool apb2) const = 0;
    virtual std::uint32_t afio_mapr() const = 0;
    virtual void timer_irq(int irq, bool level) = 0;
    // An output of the timer changed at `cycle` (at or before now).
    virtual void timer_output(int port, int pin, int source, bool level, std::uint64_t cycle) = 0;
    virtual bool pin_level(int port, int pin) const = 0;
    virtual void timer_schedule_changed() = 0;
    virtual void timer_diagnostic(const std::string& text) = 0;
};

class Stm32F1Timer : public Device, public TimedDevice {
public:
    // number: 1 (advanced, APB2) or 2..4 (general purpose, APB1).
    Stm32F1Timer(TimerHost& host, int number);

    const char* name() const override { return name_; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    // Scheduler: cycle of the next counter event (update or compare match), or
    // the maximum when the counter is stopped.
    std::uint64_t next_event() const override;
    // Processes the events due at or before `cycle`.
    void on_time(std::uint64_t cycle) override;
    // The kernel clock changed (APB prescaler): the counter continues at the new rate.
    void clocks_changed();
    // AFIO_MAPR changed: the channels move to their new pins.
    void remap_changed();
    // A pin changed level (input capture).
    void pin_changed(int port, int pin, bool level);

private:
    struct Channel {
        std::uint32_t ccr = 0;        // active value
        std::uint32_t ccr_preload = 0;
        bool ref = false;             // OCxREF
        bool driven = false;          // last level written to the pin
        bool driven_n = false;        // complementary output (TIM1)
        int capture_count = 0;        // input capture prescaler progress
        bool last_input = false;
        std::pair<int, int> pin{-1, -1};   // pin that `driven` was written to
        std::pair<int, int> pin_n{-1, -1};
    };

    std::uint32_t read32(std::uint32_t offset);
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask);

    std::uint32_t counter_at(std::uint64_t cycle) const;
    void rebase(std::uint64_t cycle);
    std::uint64_t tick_cycles() const;
    bool counting_down() const { return (cr1_ >> 4) & 1u; }
    std::uint32_t top() const;     // the value at which the up-counter overflows
    void update_event(bool from_counter);
    void match(int channel);
    void evaluate_refs();
    void update_outputs(); // drives the pins at `output_cycle_`
    void catch_up();       // processes the events that are due before a register access
    void update_irqs();
    std::uint32_t mode(int channel) const;     // OCxM
    std::uint32_t selection(int channel) const; // CCxS: 0 output, else input
    std::pair<int, int> channel_pin(int channel, bool complementary) const;
    int source_id(int channel, bool complementary) const; // for the machine's AF levels
    void report(std::uint32_t feature, const char* text);

    TimerHost& host_;
    int number_;
    char name_[8] = {};
    bool advanced_;

    std::uint32_t cr1_ = 0, cr2_ = 0, smcr_ = 0, dier_ = 0, sr_ = 0, ccmr1_ = 0, ccmr2_ = 0, ccer_ = 0;
    std::uint32_t psc_ = 0, psc_active_ = 0, arr_ = 0xFFFF, arr_active_ = 0xFFFF, rcr_ = 0, rcr_count_ = 0;
    std::uint32_t bdtr_ = 0, dcr_ = 0;
    std::array<Channel, 4> ch_{};

    // Counter: value `base_count_` at cycle `base_cycle_`, one step every tick.
    std::uint64_t base_cycle_ = 0;
    std::uint32_t base_count_ = 0;
    std::uint64_t kernel_ = 1;        // HCLK cycles per kernel clock tick
    std::uint64_t output_cycle_ = 0;  // time stamp of output changes
    std::uint32_t reported_ = 0; // features already reported as not simulated
};

} // namespace simnodus::emu
