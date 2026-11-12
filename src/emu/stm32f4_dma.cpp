// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f4_dma.hpp"

namespace simnodus::emu {

namespace {

constexpr std::uint32_t CR_EN = 1u << 0, CR_DMEIE = 1u << 1, CR_TEIE = 1u << 2, CR_HTIE = 1u << 3, CR_TCIE = 1u << 4,
                        CR_PFCTRL = 1u << 5, CR_CIRC = 1u << 8, CR_PINC = 1u << 9, CR_MINC = 1u << 10,
                        CR_PINCOS = 1u << 15, CR_DBM = 1u << 18, CR_CT = 1u << 19;
constexpr std::uint32_t CR_MASK = 0x0FEFFFFFu;
constexpr std::uint32_t FEIF = 1u << 0, DMEIF = 1u << 2, TEIF = 1u << 3, HTIF = 1u << 4, TCIF = 1u << 5;
constexpr std::uint32_t FCR_FEIE = 1u << 7;

enum : std::uint32_t { F_FLOW = 1u << 0, F_DMA1_M2M = 1u << 1, F_CIRC_M2M = 1u << 2 };

using R = F4DmaRequest;
constexpr R N = R::None;

// RM0090 tables 42 (DMA1) and 43 (DMA2): [channel][stream].
constexpr R dma1_map[8][8] = {
    {R::Spi3Rx, N, R::Spi3Rx, R::Spi2Rx, R::Spi2Tx, R::Spi3Tx, N, R::Spi3Tx},
    {R::I2c1Rx, N, N, N, N, R::I2c1Rx, R::I2c1Tx, R::I2c1Tx},
    {N, N, N, N, N, N, N, N},
    {N, N, R::I2c3Rx, N, R::I2c3Tx, N, N, N},
    {R::Uart5Rx, R::Usart3Rx, R::Uart4Rx, R::Usart3Tx, R::Uart4Tx, R::Usart2Rx, R::Usart2Tx, R::Uart5Tx},
    {N, N, N, N, N, N, N, N},
    {N, N, N, N, N, N, N, N},
    {N, N, R::I2c2Rx, R::I2c2Rx, R::Usart3Tx, R::Dac1, R::Dac2, R::I2c2Tx},
};
constexpr R dma2_map[8][8] = {
    {R::Adc1, N, N, N, R::Adc1, N, N, N},
    {N, N, R::Adc2, R::Adc2, N, N, N, N},
    {R::Adc3, R::Adc3, N, N, N, N, N, N},
    {R::Spi1Rx, N, R::Spi1Rx, R::Spi1Tx, N, R::Spi1Tx, N, N},
    {N, N, R::Usart1Rx, N, N, R::Usart1Rx, N, R::Usart1Tx},
    {N, R::Usart6Rx, R::Usart6Rx, N, N, N, R::Usart6Tx, R::Usart6Tx},
    {N, N, N, N, N, N, N, N},
    {N, N, N, N, N, N, N, N},
};

// Interrupt lines of the streams.
constexpr int dma1_irq[8] = {11, 12, 13, 14, 15, 16, 17, 47};
constexpr int dma2_irq[8] = {56, 57, 58, 59, 60, 68, 69, 70};

// Position of a stream's six flags in LISR or HISR.
constexpr unsigned flag_shift[4] = {0, 6, 16, 22};

unsigned item_size(std::uint32_t code)
{
    return code == 0 ? 1u : code == 1 ? 2u : 4u;
}

std::uint32_t size_mask(unsigned size)
{
    return size >= 4 ? 0xFFFFFFFFu : (1u << (8 * size)) - 1u;
}

} // namespace

F4DmaRequest f4_dma_request(int controller, int stream, int channel)
{
    if (stream < 0 || stream > 7 || channel < 0 || channel > 7) {
        return R::None;
    }
    return controller == 1 ? dma1_map[channel][stream] : dma2_map[channel][stream];
}

Stm32F4Dma::Stm32F4Dma(DmaHost& host, int controller) : host_(host), controller_(controller) {}

void Stm32F4Dma::reset()
{
    st_ = {};
    lisr_ = hisr_ = 0;
}

void Stm32F4Dma::report(std::uint32_t feature, const std::string& text)
{
    if (!(reported_ & feature)) {
        reported_ |= feature;
        host_.dma_diagnostic(std::string(name()) + ": " + text);
    }
}

std::uint32_t Stm32F4Dma::flags(int index) const
{
    const std::uint32_t reg = index < 4 ? lisr_ : hisr_;
    return (reg >> flag_shift[index % 4]) & 0x3Fu;
}

void Stm32F4Dma::set_flags(int index, std::uint32_t bits)
{
    std::uint32_t& reg = index < 4 ? lisr_ : hisr_;
    reg |= (bits & 0x3Du) << flag_shift[index % 4];
    update_irq(index);
}

void Stm32F4Dma::update_irq(int index)
{
    const Stream& s = st_[static_cast<std::size_t>(index)];
    const std::uint32_t f = flags(index);
    const bool level = ((s.cr & CR_TCIE) && (f & TCIF)) || ((s.cr & CR_HTIE) && (f & HTIF))
        || ((s.cr & CR_TEIE) && (f & TEIF)) || ((s.cr & CR_DMEIE) && (f & DMEIF)) || ((s.fcr & FCR_FEIE) && (f & FEIF));
    host_.dma_irq(controller_ == 1 ? dma1_irq[index] : dma2_irq[index], level);
}

bool Stm32F4Dma::request(F4DmaRequest request)
{
    // The enabled stream with the highest priority (PL, then the lower number) serves the request.
    int best = -1;
    std::uint32_t best_pl = 0;
    for (int i = 0; i < streams; ++i) {
        const Stream& s = st_[static_cast<std::size_t>(i)];
        if (!(s.cr & CR_EN) || s.ndtr == 0 || ((s.cr >> 6) & 3u) == 2u) {
            continue;
        }
        if (f4_dma_request(controller_, i, static_cast<int>((s.cr >> 25) & 7u)) != request) {
            continue;
        }
        const std::uint32_t pl = (s.cr >> 16) & 3u;
        if (best < 0 || pl > best_pl) {
            best = i;
            best_pl = pl;
        }
    }
    if (best < 0) {
        return false;
    }
    transfer(best);
    return true;
}

void Stm32F4Dma::transfer(int index)
{
    Stream& s = st_[static_cast<std::size_t>(index)];
    // Direct mode uses PSIZE on both sides. In FIFO mode the FIFO packs or
    // unpacks: moving peripheral-sized items through consecutive memory
    // addresses leaves the same bytes in memory.
    const unsigned size = item_size((s.cr >> 11) & 3u);
    const std::uint32_t dir = (s.cr >> 6) & 3u;
    const bool to_peripheral = dir == 1;
    const std::uint32_t from = to_peripheral ? s.memory : s.peripheral;
    const std::uint32_t to = to_peripheral ? s.peripheral : s.memory;
    Bus& bus = host_.dma_bus();
    std::uint32_t value = 0;
    if (!bus.read(from, size, value) || !bus.write(to, size, value & size_mask(size))) {
        s.cr &= ~CR_EN; // a transfer error disables the stream
        host_.dma_diagnostic(std::string(name()) + " stream " + std::to_string(index)
            + ": transfer error (no memory or peripheral at the address).");
        set_flags(index, TEIF);
        return;
    }
    if (s.cr & CR_PINC) {
        s.peripheral += (s.cr & CR_PINCOS) ? 4u : size;
    }
    if (s.cr & CR_MINC) {
        s.memory += size;
    }
    --s.ndtr;
    std::uint32_t raised = 0;
    if (s.reload / 2 > 0 && s.reload - s.ndtr == s.reload / 2) {
        raised |= HTIF;
    }
    if (s.ndtr == 0) {
        raised |= TCIF;
        if (s.cr & CR_DBM) {
            s.cr ^= CR_CT; // the other buffer, at once
            s.ndtr = s.reload;
            s.peripheral = s.par;
            s.memory = (s.cr & CR_CT) ? s.m1ar : s.m0ar;
        } else if (s.cr & CR_CIRC) {
            s.ndtr = s.reload;
            s.peripheral = s.par;
            s.memory = s.m0ar;
        } else {
            s.cr &= ~CR_EN;
        }
    }
    if (raised) {
        set_flags(index, raised);
    }
}

void Stm32F4Dma::memory_to_memory(int index)
{
    Stream& s = st_[static_cast<std::size_t>(index)];
    if (controller_ == 1) {
        report(F_DMA1_M2M, "memory-to-memory transfers need DMA2 (RM0090 10.3.6); DMA1 does not move the data.");
        return;
    }
    if (s.cr & (CR_CIRC | CR_DBM)) {
        report(F_CIRC_M2M, "circular and double-buffer modes are not allowed from memory to memory; the stream moves "
                           "its items once.");
    }
    // PAR is the source, M0AR the destination.
    const std::uint32_t modes = s.cr & (CR_CIRC | CR_DBM);
    s.cr &= ~modes;
    while ((s.cr & CR_EN) && s.ndtr > 0) {
        transfer(index);
    }
    s.cr = (s.cr & ~CR_EN) | modes; // the register keeps what was written
}

void Stm32F4Dma::start(int index)
{
    Stream& s = st_[static_cast<std::size_t>(index)];
    s.peripheral = s.par;
    s.memory = (s.cr & CR_DBM) && (s.cr & CR_CT) ? s.m1ar : s.m0ar;
    if (s.cr & CR_PFCTRL) {
        report(F_FLOW, "the peripheral as the flow controller (SDIO) is not simulated yet.");
    }
    if (((s.cr >> 6) & 3u) == 2u) {
        memory_to_memory(index);
    } else {
        host_.dma_channel_enabled(index);
    }
}

bool Stm32F4Dma::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    const std::uint32_t word = read32(offset & ~3u);
    value = (word >> (8 * (offset & 3u))) & size_mask(size);
    return true;
}

bool Stm32F4Dma::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    const unsigned shift = 8 * (offset & 3u);
    const std::uint32_t mask = size_mask(size) << shift;
    write32(offset & ~3u, (value << shift) & mask, mask);
    return true;
}

std::uint32_t Stm32F4Dma::read32(std::uint32_t offset)
{
    switch (offset) {
    case 0x00: return lisr_;
    case 0x04: return hisr_;
    case 0x08: case 0x0C: return 0; // the clear registers are write-only
    default: break;
    }
    if (offset < 0x10 || offset >= 0x10 + 0x18u * streams) {
        return 0;
    }
    const Stream& s = st_[(offset - 0x10) / 0x18];
    switch ((offset - 0x10) % 0x18) {
    case 0x00: return s.cr;
    case 0x04: return s.ndtr;
    case 0x08: return s.par;
    case 0x0C: return s.m0ar;
    case 0x10: return s.m1ar;
    case 0x14: return (s.fcr & 0xA7u) | (4u << 3); // FS: the FIFO is empty
    default: return 0;
    }
}

void Stm32F4Dma::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    auto merge = [mask, value](std::uint32_t old) { return (old & ~mask) | (value & mask); };
    if (offset == 0x08 || offset == 0x0C) {
        const std::uint32_t clear = value & mask;
        std::uint32_t& reg = offset == 0x08 ? lisr_ : hisr_;
        reg &= ~(clear & 0x0F7D0F7Du);
        for (int k = 0; k < 4; ++k) {
            update_irq((offset == 0x08 ? 0 : 4) + k);
        }
        return;
    }
    if (offset < 0x10 || offset >= 0x10 + 0x18u * streams) {
        return;
    }
    const int index = static_cast<int>((offset - 0x10) / 0x18);
    Stream& s = st_[static_cast<std::size_t>(index)];
    const bool enabled = (s.cr & CR_EN) != 0;
    switch ((offset - 0x10) % 0x18) {
    case 0x00: {
        const std::uint32_t written = merge(s.cr) & CR_MASK;
        if (enabled) {
            // While enabled only EN (and, in double-buffer mode, CT through hardware) changes.
            if (!(written & CR_EN)) {
                s.cr &= ~CR_EN;
                set_flags(index, TCIF); // disabled by software: TCIF is set (RM0090 10.3.17)
            }
            break;
        }
        s.cr = written;
        if (s.cr & CR_EN) {
            start(index);
        }
        update_irq(index);
        break;
    }
    case 0x04:
        if (!enabled) {
            s.ndtr = s.reload = merge(s.ndtr) & 0xFFFFu;
        }
        break;
    case 0x08:
        if (!enabled) {
            s.par = merge(s.par);
        }
        break;
    case 0x0C:
        if (!enabled || ((s.cr & CR_DBM) && (s.cr & CR_CT))) {
            s.m0ar = merge(s.m0ar); // in double-buffer mode, the buffer not in use
        }
        break;
    case 0x10:
        if (!enabled || ((s.cr & CR_DBM) && !(s.cr & CR_CT))) {
            s.m1ar = merge(s.m1ar);
        }
        break;
    case 0x14:
        if (!enabled) {
            s.fcr = merge(s.fcr) & 0x87u;
        }
        update_irq(index);
        break;
    default: break;
    }
}

} // namespace simnodus::emu
