// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f1_spi.hpp"

namespace simnodus::emu {

namespace {

constexpr std::uint32_t CR1_CPHA = 1u << 0, CR1_CPOL = 1u << 1, CR1_MSTR = 1u << 2, CR1_SPE = 1u << 6;
constexpr std::uint32_t CR1_LSBFIRST = 1u << 7, CR1_SSM = 1u << 9, CR1_RXONLY = 1u << 10;
constexpr std::uint32_t CR1_CRCEN = 1u << 13, CR1_BIDIMODE = 1u << 15;
constexpr std::uint32_t CR2_RXDMAEN = 1u << 0, CR2_TXDMAEN = 1u << 1, CR2_SSOE = 1u << 2;
constexpr std::uint32_t CR2_ERRIE = 1u << 5, CR2_RXNEIE = 1u << 6, CR2_TXEIE = 1u << 7;
enum Signal { NSS, SCK, MOSI };

} // namespace

Stm32F1Spi::Stm32F1Spi(SpiHost& host, int number)
    : host_(host), number_(number), irq_(number == 1 ? 35 : 36), dma_rx_(number == 1 ? 2 : 4),
      dma_tx_(number == 1 ? 3 : 5)
{
}

void Stm32F1Spi::reset()
{
    cr1_ = cr2_ = 0;
    txe_ = true;
    rxne_ = ovr_ = modf_ = ovr_dr_read_ = false;
    tx_ = rx_ = 0;
    busy_ = false;
    next_edge_ = no_event;
    sck_ = false;
    driven_ = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}}; // the machine reset released the pins
}

Stm32F1Spi::Pins Stm32F1Spi::pins() const
{
    if (number_ == 2) {
        return {{1, 12}, {1, 13}, {1, 14}, {1, 15}};
    }
    if (host_.afio_mapr() & 1u) { // SPI1_REMAP
        return {{0, 15}, {1, 3}, {1, 4}, {1, 5}};
    }
    return {{0, 4}, {0, 5}, {0, 6}, {0, 7}};
}

bool Stm32F1Spi::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    const std::uint32_t reg = read16(offset & ~3u);
    value = (reg >> (8 * (offset & 3u))) & (size == 1 ? 0xFFu : 0xFFFFu);
    return true;
}

bool Stm32F1Spi::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    if ((offset & 3u) != 0) {
        return true; // the upper halves are reserved
    }
    write16(offset, value & (size == 1 ? 0xFFu : 0xFFFFu));
    return true;
}

std::uint32_t Stm32F1Spi::read16(std::uint32_t offset)
{
    switch (offset) {
    case 0x00: return cr1_;
    case 0x04: return cr2_;
    case 0x08: {
        const std::uint32_t sr = (rxne_ ? 1u : 0u) | (txe_ ? 2u : 0u) | (modf_ ? 0x20u : 0u) | (ovr_ ? 0x40u : 0u)
            | (busy_ ? 0x80u : 0u);
        if (ovr_dr_read_) {
            ovr_ = false; // DR, then SR: OVR clears
            ovr_dr_read_ = false;
            service();
        }
        return sr;
    }
    case 0x0C: {
        const std::uint32_t value = rx_;
        rxne_ = false;
        ovr_dr_read_ = ovr_;
        service();
        return value;
    }
    case 0x10: return 7; // CRCPR reset value
    default: return 0;
    }
}

void Stm32F1Spi::write16(std::uint32_t offset, std::uint32_t value)
{
    switch (offset) {
    case 0x00:
        cr1_ = value;
        if (modf_ && (value & CR1_MSTR)) {
            modf_ = false; // written again after a mode fault
        }
        configure();
        break;
    case 0x04:
        cr2_ = value & 0xE7u;
        configure();
        break;
    case 0x08:
        // CRCERR is the only rc_w0 flag; not simulated.
        break;
    case 0x0C:
        if (!master()) {
            tx_ = value;
            txe_ = false;
            break;
        }
        tx_ = value & (bits() == 16 ? 0xFFFFu : 0xFFu);
        txe_ = false;
        if (!busy_) {
            start(host_.spi_now_cycles());
        }
        service();
        break;
    default: break;
    }
}

void Stm32F1Spi::output(std::pair<int, int> pin, int signal, bool level, std::uint64_t cycle)
{
    std::pair<int, int>* driven = signal == NSS ? &driven_.nss : (signal == SCK ? &driven_.sck : &driven_.mosi);
    const int source = 300 + 8 * number_ + signal;
    if (*driven != pin && driven->first >= 0) {
        host_.spi_output(driven->first, driven->second, source, false, cycle);
    }
    *driven = pin;
    if (pin.first >= 0) {
        host_.spi_output(pin.first, pin.second, source, level, cycle);
    }
}

void Stm32F1Spi::configure()
{
    const std::uint64_t now = host_.spi_now_cycles();
    if ((cr1_ & CR1_SPE) && !(cr1_ & CR1_MSTR) && !warned_slave_) {
        warned_slave_ = true;
        host_.spi_diagnostic(std::string(name()) + " in slave mode, which SimNodus does not simulate yet.");
    }
    if ((cr1_ & (CR1_BIDIMODE | CR1_RXONLY)) && !warned_mode_) {
        warned_mode_ = true;
        host_.spi_diagnostic(std::string(name())
            + " in receive-only or bidirectional mode, which SimNodus does not simulate yet.");
    }
    if ((cr1_ & CR1_CRCEN) && !warned_crc_) {
        warned_crc_ = true;
        host_.spi_diagnostic(std::string(name()) + " with CRC, which SimNodus does not simulate yet.");
    }
    const Pins p = pins();
    // A master with NSS from its pin (SSM 0, SSOE 0) faults when the pin is low.
    if (master() && !(cr1_ & CR1_SSM) && !(cr2_ & CR2_SSOE) && !host_.pin_level(p.nss.first, p.nss.second)) {
        if (!warned_modf_) {
            warned_modf_ = true;
            host_.spi_diagnostic(std::string(name()) + ": mode fault, the NSS pin is low while the SPI is a master "
                "with hardware NSS; set SSM and SSI (software NSS) or SSOE, as STM32CubeMX does.");
        }
        modf_ = true;
        cr1_ &= ~(CR1_SPE | CR1_MSTR);
    }
    if (master()) {
        if (!busy_) {
            sck_ = (cr1_ & CR1_CPOL) != 0;
            output(p.sck, SCK, sck_, now);
            output(p.mosi, MOSI, false, now);
        }
        // SSOE: NSS is driven low while the master is enabled.
        output((cr2_ & CR2_SSOE) ? p.nss : std::pair<int, int>{-1, -1}, NSS, false, now);
    } else {
        busy_ = false;
        next_edge_ = no_event;
        output({-1, -1}, SCK, false, now);
        output({-1, -1}, MOSI, false, now);
        output({-1, -1}, NSS, false, now);
    }
    service();
    host_.spi_schedule_changed();
}

void Stm32F1Spi::remap_changed()
{
    if (master()) {
        configure();
    }
}

void Stm32F1Spi::start(std::uint64_t cycle)
{
    shift_out_ = tx_;
    txe_ = true;
    busy_ = true;
    bit_ = 0;
    half_ = 0;
    shift_in_ = 0;
    const int n = bits();
    if (!(cr1_ & CR1_CPHA)) {
        // CPHA 0: the first bit is on MOSI half a clock before the first edge.
        const bool first = (cr1_ & CR1_LSBFIRST) ? (shift_out_ & 1u) : ((shift_out_ >> (n - 1)) & 1u);
        output(pins().mosi, MOSI, first, cycle);
    }
    const std::uint64_t half = static_cast<std::uint64_t>(host_.hclk_per_pclk(number_ == 1)) << ((cr1_ >> 3) & 7u);
    next_edge_ = cycle + half;
    host_.spi_schedule_changed();
}

void Stm32F1Spi::edge(std::uint64_t cycle)
{
    const int n = bits();
    const Pins p = pins();
    ++half_;
    const bool leading = (half_ & 1) != 0;
    sck_ = !sck_;
    output(p.sck, SCK, sck_, cycle);
    const bool cpha = (cr1_ & CR1_CPHA) != 0;
    const bool lsb = (cr1_ & CR1_LSBFIRST) != 0;
    auto bit_of = [&](int k) { return lsb ? ((shift_out_ >> k) & 1u) != 0 : ((shift_out_ >> (n - 1 - k)) & 1u) != 0; };
    if (leading != cpha) {
        // Sample MISO: on the leading edge with CPHA 0, the trailing one with CPHA 1.
        const bool level = host_.pin_level(p.miso.first, p.miso.second);
        if (lsb) {
            shift_in_ |= (level ? 1u : 0u) << bit_;
        } else {
            shift_in_ = (shift_in_ << 1) | (level ? 1u : 0u);
        }
        ++bit_;
    } else {
        // Present the next bit: on the trailing edge with CPHA 0 (bit k + 1), the leading one with CPHA 1 (bit k).
        const int k = cpha ? (half_ - 1) / 2 : half_ / 2;
        if (k < n) {
            output(p.mosi, MOSI, bit_of(k), cycle);
        }
    }
    if (half_ < 2 * n) {
        next_edge_ = cycle + (static_cast<std::uint64_t>(host_.hclk_per_pclk(number_ == 1)) << ((cr1_ >> 3) & 7u));
        return;
    }
    // The frame is complete.
    if (rxne_) {
        ovr_ = true; // the previous byte was not read: this one is lost
    } else {
        rx_ = shift_in_ & (n == 16 ? 0xFFFFu : 0xFFu);
        rxne_ = true;
    }
    if (!txe_) {
        start(cycle); // the next frame follows at once
    } else {
        busy_ = false;
        next_edge_ = no_event;
    }
    service();
}

std::uint64_t Stm32F1Spi::next_event() const
{
    return busy_ ? next_edge_ : no_event;
}

void Stm32F1Spi::on_time(std::uint64_t cycle)
{
    while (busy_ && next_edge_ <= cycle) {
        edge(next_edge_);
    }
}

void Stm32F1Spi::service()
{
    if (!in_service_) {
        in_service_ = true;
        // DMA: received frames out of DR, frames to send into it (each re-enters here).
        while ((cr2_ & CR2_RXDMAEN) && rxne_ && host_.spi_dma(dma_rx_)) {
        }
        while ((cr2_ & CR2_TXDMAEN) && txe_ && master() && host_.spi_dma(dma_tx_)) {
        }
        in_service_ = false;
    }
    const bool irq = ((cr2_ & CR2_TXEIE) && txe_) || ((cr2_ & CR2_RXNEIE) && rxne_)
        || ((cr2_ & CR2_ERRIE) && (ovr_ || modf_));
    host_.spi_irq(irq_, irq);
}

} // namespace simnodus::emu
