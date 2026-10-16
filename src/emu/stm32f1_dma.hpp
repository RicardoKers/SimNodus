// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 DMA1 for the SimNodus emulator (SN-179; RM0008 chapter 13), as far as
// HAL ADC+DMA and UART+DMA need it: seven channels with peripheral and memory
// addresses, the transfer counter, increments, 8/16/32-bit sizes on each side,
// circular mode, memory-to-memory transfers, half-transfer, transfer-complete
// and transfer-error flags (ISR, IFCR) and interrupts (DMA1_Channel1..7, IRQ
// 11..17). A request moves one item at once, without bus timing. Requests come
// from ADC1 (channel 1) and USART1-3 (TX on 4, 7, 2; RX on 5, 6, 3); timer and
// SPI/I2C requests are not wired yet.
#pragma once

#include "bus.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace simnodus::emu {

class DmaHost {
public:
    virtual ~DmaHost() = default;
    virtual Bus& dma_bus() = 0;
    virtual void dma_irq(int irq, bool level) = 0;
    virtual void dma_diagnostic(const std::string& text) = 0;
    // A channel was enabled: a peripheral whose request is already active
    // (a transmitter with an empty data register) asks for transfers now.
    virtual void dma_channel_enabled(int channel) = 0;
};

class Stm32F1Dma : public Device {
public:
    static constexpr int channels = 7;

    explicit Stm32F1Dma(DmaHost& host);

    const char* name() const override { return "DMA1"; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    // A peripheral asks channel 1..7 for one transfer. Returns true if an item
    // moved (the channel is enabled, not memory-to-memory, and has items left).
    bool request(int channel);
    // True while the channel is enabled with items left (for peripherals that
    // ask repeatedly, such as a transmitter).
    bool ready(int channel) const;

private:
    struct Channel {
        std::uint32_t ccr = 0;
        std::uint32_t cndtr = 0;   // items left
        std::uint32_t reload = 0;  // CNDTR as programmed (circular mode, half transfer)
        std::uint32_t cpar = 0;
        std::uint32_t cmar = 0;
        std::uint32_t peripheral = 0; // current addresses
        std::uint32_t memory = 0;
    };

    void transfer(int index);
    void memory_to_memory(int index);
    void update_irq(int index);
    std::uint32_t read32(std::uint32_t offset);
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask);

    DmaHost& host_;
    std::array<Channel, channels> ch_{};
    std::uint32_t isr_ = 0;
};

} // namespace simnodus::emu
