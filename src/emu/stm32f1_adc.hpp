// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 ADC1 and ADC2 for the SimNodus emulator (SN-178; RM0008 chapter 11).
// Simulated: power-on with ADON, the "ADON again" and SWSTART/JSWSTART starts,
// calibration (CAL, RSTCAL), single, continuous and scan conversion of the
// regular group (SQR1-3), the injected group (JSQR, JDR1-4 with offsets, JAUTO),
// per-channel sample times (SMPR1-2) and the conversion time of sample time +
// 12.5 ADC clock cycles from PCLK2 and ADCPRE, right and left alignment, the
// analog watchdog, EOC/JEOC/AWD flags and interrupt (IRQ 18, shared by both
// ADCs), the temperature sensor (1.43 V at 25 °C) and VREFINT (1.20 V).
// The channel's voltage is taken at the end of its sampling time from the
// host, which reads the circuit. Codes are 12 bits over 0..3.3 V (VDDA).
// With CR2.DMA, ADC1 asks DMA1 channel 1 to read DR after each regular
// conversion (SN-179); ADC2 has no DMA request on the F103 (reported).
// Not simulated yet (reported once): external triggers from timers and EXTI,
// discontinuous mode, dual-ADC modes.
#pragma once

#include "bus.hpp"
#include "timed_device.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace simnodus::emu {

// What an ADC needs from the machine.
class AdcHost {
public:
    virtual ~AdcHost() = default;
    virtual std::uint64_t now_cycles() const = 0;
    // HCLK cycles per ADC clock cycle (APB2 divider x ADCPRE divider).
    virtual std::uint32_t hclk_per_adc_clock() const = 0;
    // Voltage of channel 0..17 at CPU cycle `cycle` (at or before now).
    virtual double adc_channel_voltage(int channel, std::uint64_t cycle) = 0;
    virtual void adc_irq_changed() = 0; // the machine ORs both ADCs onto IRQ 18
    virtual void adc_schedule_changed() = 0;
    virtual void adc_diagnostic(const std::string& text) = 0;
    virtual void adc_dma_request() = 0; // ADC1: DMA1 channel 1 reads DR
};

class Stm32F1Adc : public Device, public TimedDevice {
public:
    static constexpr double vdda = 3.3;

    Stm32F1Adc(AdcHost& host, int number);

    const char* name() const override { return name_; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;

    bool irq() const; // EOC, JEOC or AWD with its interrupt enabled

    // 12-bit code of a voltage (rounded, clamped to 0..4095).
    static std::uint32_t code_of(double volts);

private:
    enum class Busy { None, Calibration, Regular, Injected };

    std::uint32_t read32(std::uint32_t offset);
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask);
    void catch_up();
    void start_regular();
    void start_injected();
    void schedule_next(std::uint64_t at); // the next conversion, injected first
    void finish_step();
    std::uint32_t sample_cycles_x2(int channel) const; // sample time in half ADC cycles
    int regular_channel(int index) const;
    int injected_channel(int index) const;
    int regular_length() const;
    int injected_length() const;
    void check_watchdog(int channel, std::uint32_t code, bool injected);
    void update_irq();
    void report(std::uint32_t feature, const char* text);

    AdcHost& host_;
    int number_;
    char name_[8] = {};

    std::uint32_t sr_ = 0, cr1_ = 0, cr2_ = 0, smpr1_ = 0, smpr2_ = 0, htr_ = 0xFFF, ltr_ = 0;
    std::uint32_t sqr1_ = 0, sqr2_ = 0, sqr3_ = 0, jsqr_ = 0, dr_ = 0;
    std::array<std::uint32_t, 4> jofr_{}, jdr_{};

    Busy busy_ = Busy::None;      // what the converter does now
    int channel_ = 0;             // channel being converted
    std::uint64_t sample_at_ = 0; // cycle at which it is sampled (end of the sampling time)
    std::uint64_t end_at_ = 0;    // cycle at which the current step ends
    bool regular_active_ = false; // a regular group is in progress (or continuous)
    int regular_index_ = 0;
    bool injected_active_ = false;
    int injected_index_ = 0;
    std::uint32_t reported_ = 0;
    bool in_event_ = false; // processing events: a DMA read of DR must not run the next ones
};

} // namespace simnodus::emu
