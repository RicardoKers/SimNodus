// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32 timers for the SimNodus emulator: TIM1 (advanced) and TIM2-TIM4
// (general purpose) of the STM32F1 (SN-177; RM0008 chapters 14 and 15), and
// the fourteen timers of the STM32F4 (SN-297; RM0090 chapters 17 to 20),
// which have the same registers: the chip says what each timer has
// (TimerConfig) and where its channels are (TimerHost::timer_pin).
// Simulated: time base (up and down counting, prescaler, auto-reload with
// preload, one-pulse mode, update disable/request source, repetition counter
// of the advanced timers, 16 or 32 bits), output compare (frozen,
// active/inactive/toggle on match, forced levels, PWM 1 and 2, CCR preload,
// polarity, output enable, main output enable, complementary outputs with
// dead time), input capture (rising, falling or both edges, capture
// prescaler, overcapture), the encoder interface (modes 1 to 3), the trigger
// output (TRGO on update, on the first channel's match or on a reference
// rising), status flags, interrupts and the channel pins, and DMA
// requests on the update event and on each channel's event (UDE, CCxDE,
// CCDS) where the chip wires them (TimerHost::timer_dma; SN-298 for the F4).
// Not simulated yet (reported once): center-aligned counting, the other slave
// modes (reset, gated, trigger, external clock), input filters, the break
// input, the trigger and commutation DMA requests, and DMA on the STM32F103.
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
    // The pin (port, pin) of a channel (0 to 3) or of its complementary
    // output, as the chip routes it now; port -1: none.
    virtual std::pair<int, int> timer_pin(int number, int channel, bool complementary) const = 0;
    // An interrupt request of timer `number` on line `irq` (chips share lines between timers).
    virtual void timer_irq(int number, int irq, bool level) = 0;
    // An output of the timer changed at `cycle` (at or before now).
    virtual void timer_output(int port, int pin, int source, bool level, std::uint64_t cycle) = 0;
    virtual bool pin_level(int port, int pin) const = 0;
    virtual void timer_schedule_changed() = 0;
    virtual void timer_diagnostic(const std::string& text) = 0;
    // A trigger for other peripherals at `cycle`: signal 0 is the timer's
    // TRGO, 1 to 4 the rising of the output reference of channel 1 to 4.
    virtual void timer_trigger(int number, int signal, std::uint64_t cycle)
    {
        (void)number;
        (void)signal;
        (void)cycle;
    }
    // A DMA request of timer `number`: 0 the update event, 1 to 4 a
    // channel's event. True if a transfer was made. Chips without timer DMA
    // keep the default, and the timer reports UDE and CCxDE as not simulated.
    virtual bool timer_dma(int number, int request)
    {
        (void)number;
        (void)request;
        return false;
    }
    virtual bool timer_dma_wired() const { return false; }
};

// What the chip gives a timer.
struct TimerConfig {
    int number = 1;
    bool advanced = false;   // repetition counter, complementary outputs, BDTR
    int channels = 4;        // 4, 2, 1, or 0 (a basic timer)
    bool wide = false;       // a 32-bit counter
    bool apb2 = false;       // the bus its kernel clock comes from
    bool both_edges = false; // CCxNP with CCxP captures on both edges (STM32F2/F4)
    // Interrupt lines: `irq_update` carries everything unless the timer is
    // advanced, which has four; -1: none.
    int irq_break = -1;
    int irq_update = -1;
    int irq_trigger = -1;
    int irq_compare = -1;
};

class Stm32F1Timer : public Device, public TimedDevice {
public:
    // A timer of the STM32F103: 1 (advanced, APB2) or 2..4 (general purpose, APB1).
    Stm32F1Timer(TimerHost& host, int number);
    Stm32F1Timer(TimerHost& host, const TimerConfig& config);

    const char* name() const override { return name_; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    // Scheduler: cycle of the next event (update, compare match or the end of
    // a dead time), or the maximum when there is none.
    std::uint64_t next_event() const override;
    // Processes the events due at or before `cycle`.
    void on_time(std::uint64_t cycle) override;
    // The kernel clock changed (APB prescaler): the counter continues at the new rate.
    void clocks_changed();
    // The chip's routing changed: the channels move to their new pins.
    void remap_changed();
    // A pin changed level (input capture, encoder).
    void pin_changed(int port, int pin, bool level);

    // The STM32F103's channel pins with AFIO_MAPR.
    static std::pair<int, int> f1_pin(std::uint32_t mapr, int number, int channel, bool complementary);

private:
    struct Channel {
        std::uint32_t ccr = 0;        // active value
        std::uint32_t ccr_preload = 0;
        bool ref = false;             // OCxREF
        bool out_ref = false;         // the reference the outputs last followed
        bool dead = false;            // in the dead time after a change of the reference
        std::uint64_t dead_until = 0;
        bool driven = false;          // last level written to the pin
        bool driven_n = false;        // complementary output
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
    bool encoder() const { return (smcr_ & 7u) >= 1 && (smcr_ & 7u) <= 3; }
    bool running() const { return (cr1_ & 1u) && !encoder(); } // the counter follows the clock
    std::uint32_t top() const;     // the value at which the up-counter overflows
    std::uint64_t counter_event() const;
    std::uint64_t dead_event() const;
    std::uint64_t dead_cycles() const;
    void update_event(bool from_counter);
    void match(int channel);
    void evaluate_refs();
    void update_outputs(); // drives the pins at `output_cycle_`
    void catch_up();       // processes the events that are due before a register access
    void update_irqs();
    void encoder_step(bool up);
    void read_encoder_inputs();
    std::uint32_t mode(int channel) const;     // OCxM
    std::uint32_t selection(int channel) const; // CCxS: 0 output, else input
    std::pair<int, int> channel_pin(int channel, bool complementary) const;
    int source_id(int channel, bool complementary) const; // for the machine's AF levels
    void report(std::uint32_t feature, const char* text);

    TimerHost& host_;
    TimerConfig config_;
    int number_;
    char name_[8] = {};
    bool advanced_;
    int channels_;
    std::uint32_t mask_; // the counter's bits

    std::uint32_t cr1_ = 0, cr2_ = 0, smcr_ = 0, dier_ = 0, sr_ = 0, ccmr1_ = 0, ccmr2_ = 0, ccer_ = 0;
    std::uint32_t psc_ = 0, psc_active_ = 0, arr_ = 0xFFFF, arr_active_ = 0xFFFF, rcr_ = 0, rcr_count_ = 0;
    std::uint32_t bdtr_ = 0, dcr_ = 0;
    std::array<Channel, 4> ch_{};
    std::array<bool, 2> encoder_inputs_{}; // TI1 and TI2 after their polarity

    // Counter: value `base_count_` at cycle `base_cycle_`, one step every tick.
    std::uint64_t base_cycle_ = 0;
    std::uint32_t base_count_ = 0;
    std::uint64_t kernel_ = 1;        // HCLK cycles per kernel clock tick
    std::uint64_t output_cycle_ = 0;  // time stamp of output changes
    std::uint32_t reported_ = 0; // features already reported as not simulated
    std::uint32_t dma_pending_ = 0; // requests raised during an event: bit 0 update, 1 to 4 the channels
    bool in_dma_ = false;
    void request_dma(int request);
    void flush_dma(); // after an event: the DMA may write the timer's own registers
};

} // namespace simnodus::emu
