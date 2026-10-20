// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 SPI1 and SPI2 in master mode for the SimNodus emulator (SN-204;
// RM0008 chapter 25). Simulated: full-duplex transfers of 8 or 16 bits, MSB or
// LSB first, clock polarity and phase, baud rate PCLK / 2..256 with every SCK
// edge at its exact cycle, MOSI driven and MISO sampled on the pins at the
// edges the mode prescribes (so chips on the board's nets answer within the
// transfer, ADR 0102), back-to-back transfers from the transmit buffer, TXE,
// RXNE, BSY and OVR, interrupts, DMA requests (SPI1 RX/TX on DMA1 channels 2/3,
// SPI2 on 4/5), the NSS output (SSOE) and the remap of SPI1. Not simulated
// (reported once): slave mode, receive-only and bidirectional modes, CRC, I2S;
// mode fault is reported instead of raised.
#pragma once

#include "bus.hpp"
#include "timed_device.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace simnodus::emu {

// What an SPI needs from the machine.
class SpiHost {
public:
    virtual ~SpiHost() = default;
    virtual std::uint64_t spi_now_cycles() const = 0;
    // HCLK cycles per PCLK cycle on APB2 (SPI1) or APB1 (SPI2).
    virtual std::uint32_t hclk_per_pclk(bool apb2) const = 0;
    virtual std::uint32_t afio_mapr() const = 0;
    virtual void spi_irq(int irq, bool level) = 0;
    // An output of the SPI changed at `cycle` (at or before now).
    virtual void spi_output(int port, int pin, int source, bool level, std::uint64_t cycle) = 0;
    virtual bool pin_level(int port, int pin) const = 0;
    // One DMA transfer on `channel`; true if an item moved.
    virtual bool spi_dma(int channel) = 0;
    virtual void spi_schedule_changed() = 0;
    virtual void spi_diagnostic(const std::string& text) = 0;
};

class Stm32F1Spi : public Device, public TimedDevice {
public:
    // number: 1 (APB2) or 2 (APB1).
    Stm32F1Spi(SpiHost& host, int number);

    const char* name() const override { return number_ == 1 ? "SPI1" : "SPI2"; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    // AFIO_MAPR changed: SPI1 moves to its other pins.
    void remap_changed();
    // A DMA channel was enabled: a waiting request is served now.
    void dma_enabled() { service(); }

private:
    struct Pins {
        std::pair<int, int> nss, sck, miso, mosi;
    };
    Pins pins() const;
    std::uint32_t read16(std::uint32_t offset);
    void write16(std::uint32_t offset, std::uint32_t value);
    void configure();          // after CR1/CR2 writes: idle levels, reports
    void start(std::uint64_t cycle); // moves the transmit buffer into the shift register
    void edge(std::uint64_t cycle);  // one SCK edge
    void output(std::pair<int, int> pin, int signal, bool level, std::uint64_t cycle);
    void service();            // interrupts and DMA after a flag changed
    bool master() const { return (cr1_ & 0x44u) == 0x44u; } // SPE and MSTR
    int bits() const { return (cr1_ & (1u << 11)) ? 16 : 8; }

    SpiHost& host_;
    int number_;
    int irq_;
    int dma_rx_;
    int dma_tx_;
    std::uint32_t cr1_ = 0, cr2_ = 0;
    bool txe_ = true, rxne_ = false, ovr_ = false, modf_ = false;
    bool ovr_dr_read_ = false; // OVR clears with a DR read followed by an SR read
    std::uint32_t tx_ = 0;     // transmit buffer
    std::uint32_t rx_ = 0;     // receive buffer (DR when read)
    bool busy_ = false;
    std::uint32_t shift_out_ = 0, shift_in_ = 0;
    int bit_ = 0;              // bits already sampled in the current frame
    int half_ = 0;             // edges done in the current frame
    std::uint64_t next_edge_ = no_event;
    bool sck_ = false;
    Pins driven_{{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}}; // pins the outputs were written to
    bool in_service_ = false;
    bool warned_slave_ = false, warned_mode_ = false, warned_crc_ = false, warned_modf_ = false;
};

} // namespace simnodus::emu
