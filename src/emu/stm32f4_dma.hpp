// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F4 DMA1 and DMA2 for the SimNodus emulator (E-12, SN-298; RM0090
// chapter 10). Each controller has eight streams; a stream serves the
// peripheral request its CHSEL selects (RM0090 tables 42 and 43).
// Simulated: peripheral to memory, memory to peripheral and memory to memory
// (DMA2 only), the transfer counter in peripheral items, increments (PINCOS
// too), 8/16/32-bit items, circular and double-buffer modes, the stream
// priority (PL, then the stream number), the half-transfer, transfer-complete
// and transfer-error flags (LISR, HISR, LIFCR, HIFCR) and interrupts.
// A request moves one item at once, without bus timing. In FIFO mode the
// memory side is packed or unpacked as the FIFO does, so memory ends with the
// same bytes; FIFO thresholds, bursts and the FIFO and direct-mode error
// flags are not simulated, nor is the peripheral flow controller (reported
// once). Requests come from the serial ports, SPI1 to SPI3, I2C1 to I2C3, the
// ADCs, the DAC and the timers (update and channel events; a stream slot
// that RM0090 shares between two of a timer's requests serves both).
#pragma once

#include "bus.hpp"
#include "stm32f1_dma.hpp" // DmaHost

#include <array>
#include <cstdint>
#include <string>

namespace simnodus::emu {

// The DMA requests of the peripherals SimNodus simulates.
enum class F4DmaRequest : std::uint8_t {
    None,
    Spi1Rx, Spi1Tx, Spi2Rx, Spi2Tx, Spi3Rx, Spi3Tx,
    I2c1Rx, I2c1Tx, I2c2Rx, I2c2Tx, I2c3Rx, I2c3Tx,
    Usart1Rx, Usart1Tx, Usart2Rx, Usart2Tx, Usart3Rx, Usart3Tx,
    Uart4Rx, Uart4Tx, Uart5Rx, Uart5Tx, Usart6Rx, Usart6Tx,
    Adc1, Adc2, Adc3, Dac1, Dac2,
};

// The request that stream `stream` (0 to 7) of DMA `controller` (1 or 2) serves on channel `channel` (0 to 7).
F4DmaRequest f4_dma_request(int controller, int stream, int channel);

// A timer's request: `kind` 0 the update event, 1 to 4 channel 1 to 4's event.
constexpr F4DmaRequest f4_timer_request(int timer, int kind)
{
    return static_cast<F4DmaRequest>(64 + 8 * timer + kind);
}

// Whether that stream and channel serve `request` (a peripheral's or a timer's).
bool f4_dma_serves(int controller, int stream, int channel, F4DmaRequest request);

class Stm32F4Dma : public Device {
public:
    static constexpr int streams = 8;

    // controller: 1 or 2. DmaHost::dma_channel_enabled gets the stream (0 to 7).
    Stm32F4Dma(DmaHost& host, int controller);

    const char* name() const override { return controller_ == 1 ? "DMA1" : "DMA2"; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    // A peripheral asks for one transfer. True if an item moved (an enabled
    // stream serves the request and has items left).
    bool request(F4DmaRequest request);

private:
    struct Stream {
        std::uint32_t cr = 0;
        std::uint32_t ndtr = 0;   // items left
        std::uint32_t reload = 0; // NDTR as programmed
        std::uint32_t par = 0;
        std::uint32_t m0ar = 0, m1ar = 0;
        std::uint32_t fcr = 0x21;
        std::uint32_t peripheral = 0; // current addresses
        std::uint32_t memory = 0;
    };

    std::uint32_t read32(std::uint32_t offset);
    void write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask);
    void start(int index);
    void transfer(int index);
    void memory_to_memory(int index);
    void set_flags(int index, std::uint32_t flags);
    std::uint32_t flags(int index) const;
    void update_irq(int index);
    void report(std::uint32_t feature, const std::string& text);

    DmaHost& host_;
    int controller_;
    std::array<Stream, streams> st_{};
    std::uint32_t lisr_ = 0, hisr_ = 0;
    std::uint32_t reported_ = 0;
};

} // namespace simnodus::emu
