// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F4 analog peripherals for the SimNodus emulator (E-12, SN-297).
//
// ADC1 to ADC3 with their common registers (RM0090 chapter 13), one device at
// 0x40012000. Simulated: ADON, SWSTART and JSWSTART, starts from a timer
// (EXTSEL, JEXTSEL with EXTEN, JEXTEN), single, continuous and scan
// conversion of the regular group, the injected group with offsets and JAUTO,
// the resolution (12, 10, 8, 6 bits), per-channel sample times and the
// conversion time of sample time + resolution in ADC clock cycles from PCLK2
// and ADCPRE, alignment, EOC at the end of a sequence or of each conversion
// (EOCS), the overrun flag, the analog watchdog, the interrupt (IRQ 18,
// shared), and on ADC1 the temperature sensor (0.76 V at 25 °C), VREFINT
// (1.21 V) and VBAT / 2. The code of a voltage is floor(V * 2^n / VREF+) with
// VREF+ = 3.3 V. DMA (SN-298): with CR2.DMA each regular result asks the
// DMA for a transfer, which reads DR; a result the DMA does not take before
// the next one is an overrun. Not simulated yet (reported once):
// discontinuous mode, the multi-ADC modes, triggers from EXTI lines.
//
// The DAC (RM0090 chapter 14). Simulated: both channels with their enable and
// output buffer bits, the three data alignments and the dual registers, the
// software trigger and the timer triggers (TIM2, TIM4 to TIM8), the triangle
// wave, DMA (SN-298: each trigger moves DHR to the output and asks the DMA
// for the next value; a request still waiting at the next trigger is an
// underrun, DMAUDR, with its interrupt on IRQ 54). Not simulated yet
// (reported once): the noise generator.
#pragma once

#include "reg_device.hpp"
#include "timed_device.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace simnodus::emu {

class F4AnalogHost {
public:
    virtual ~F4AnalogHost() = default;
    virtual std::uint64_t analog_now_cycles() const = 0;
    virtual std::uint32_t hclk_per_pclk2() const = 0;
    // Voltage of external channel 0..15 of ADC `adc` (1 to 3) at CPU cycle `cycle`.
    virtual double adc_voltage(int adc, int channel, std::uint64_t cycle) = 0;
    virtual void adc_irq(bool level) = 0;
    virtual void analog_schedule_changed() = 0;
    virtual void analog_diagnostic(const std::string& text) = 0;
    // A DAC channel (0 or 1) changed at `cycle`: whether it drives its pin,
    // through the buffer or not, and its 12-bit code.
    virtual void dac_output(int channel, bool enabled, bool buffered, std::uint32_t code, std::uint64_t cycle) = 0;
    // A DMA request of ADC `adc` (1 to 3) or of DAC channel `channel` (0, 1); true if a transfer was made.
    virtual bool adc_dma(int adc)
    {
        (void)adc;
        return false;
    }
    virtual bool dac_dma(int channel)
    {
        (void)channel;
        return false;
    }
    virtual void dac_irq(bool level) { (void)level; } // DMA underrun, shared with TIM6
};

class Stm32F4Adc : public RegDevice, public TimedDevice {
public:
    static constexpr double vref = 3.3;

    explicit Stm32F4Adc(F4AnalogHost& host) : host_(host) { reset(); }

    const char* name() const override { return "ADC"; }
    void reset() override;
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;

    // An event of a timer (TimerHost::timer_trigger): starts the groups that wait for it.
    void timer_event(int timer, int signal, std::uint64_t cycle);
    // A DMA stream was enabled: a result waiting in DR is offered to it.
    void dma_enabled();

    // The code of a voltage at a resolution of `bits`.
    static std::uint32_t code_of(double volts, int bits);

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override;
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    enum class Busy { None, Regular, Injected };
    struct Unit {
        std::uint32_t sr = 0, cr1 = 0, cr2 = 0, smpr1 = 0, smpr2 = 0, htr = 0xFFF, ltr = 0;
        std::uint32_t sqr1 = 0, sqr2 = 0, sqr3 = 0, jsqr = 0, dr = 0;
        std::array<std::uint32_t, 4> jofr{}, jdr{};
        Busy busy = Busy::None;
        int channel = 0;
        std::uint64_t sample_at = 0;
        std::uint64_t end_at = 0;
        bool regular_active = false;
        int regular_index = 0;
        bool injected_active = false;
        int injected_index = 0;
        bool unread = false; // DR holds a result the firmware has not read
    };

    Unit& unit(int index) { return units_[static_cast<std::size_t>(index)]; }
    void catch_up();
    int bits(const Unit& u) const;
    int regular_length(const Unit& u) const;
    int injected_length(const Unit& u) const;
    int regular_channel(const Unit& u, int index) const;
    int injected_channel(const Unit& u, int index) const;
    void schedule_next(int index, std::uint64_t at);
    void finish_step(int index);
    void start_regular(int index, std::uint64_t at);
    void start_injected(int index, std::uint64_t at);
    void check_watchdog(Unit& u, int channel, std::uint32_t code, bool injected);
    void update_irq();
    void write_unit(int index, std::uint32_t offset, std::uint32_t value, std::uint32_t mask);
    void report(std::uint32_t feature, const std::string& text);
    void offer_dma(int index);

    F4AnalogHost& host_;
    std::array<Unit, 3> units_{};
    std::uint32_t ccr_ = 0;
    std::uint32_t reported_ = 0;
    bool in_event_ = false;
};

class Stm32F4Dac : public RegDevice {
public:
    explicit Stm32F4Dac(F4AnalogHost& host) : host_(host) { reset(); }

    const char* name() const override { return "DAC"; }
    void reset() override;

    // A timer's TRGO at `cycle`: the channels that wait for it load their output.
    void timer_event(int timer, std::uint64_t cycle);
    // A DMA stream was enabled: waiting requests are served.
    void dma_enabled();

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override;
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override;

private:
    std::uint32_t control(int channel) const { return (cr_ >> (16 * channel)) & 0xFFFFu; }
    void trigger(int channel, std::uint64_t cycle);
    void load(int channel, std::uint64_t cycle); // DOR follows DHR (and the wave)
    void publish(int channel, std::uint64_t cycle);
    void report(std::uint32_t feature, const std::string& text);
    void request_dma(int channel);
    void update_irq();

    F4AnalogHost& host_;
    std::uint32_t cr_ = 0;
    std::uint32_t sr_ = 0; // DMAUDR1, DMAUDR2
    std::array<bool, 2> dma_waiting_{};
    std::array<std::uint32_t, 2> dhr_{}; // 12 bits, right aligned
    std::array<std::uint32_t, 2> dor_{};
    std::array<std::uint32_t, 2> wave_{}; // the triangle's counter
    std::array<bool, 2> rising_{{true, true}};
    std::uint32_t reported_ = 0;
};

} // namespace simnodus::emu
