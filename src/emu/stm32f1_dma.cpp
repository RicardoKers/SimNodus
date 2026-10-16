// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f1_dma.hpp"

namespace simnodus::emu {

namespace {

constexpr std::uint32_t CCR_EN = 1u << 0, CCR_TCIE = 1u << 1, CCR_HTIE = 1u << 2, CCR_TEIE = 1u << 3,
                        CCR_DIR = 1u << 4, CCR_CIRC = 1u << 5, CCR_PINC = 1u << 6, CCR_MINC = 1u << 7,
                        CCR_MEM2MEM = 1u << 14;

// ISR flags of channel index i (0-based).
constexpr std::uint32_t gif(int i) { return 1u << (4 * i); }
constexpr std::uint32_t tcif(int i) { return 2u << (4 * i); }
constexpr std::uint32_t htif(int i) { return 4u << (4 * i); }
constexpr std::uint32_t teif(int i) { return 8u << (4 * i); }

unsigned item_size(std::uint32_t bits)
{
    return bits == 0 ? 1u : (bits == 1 ? 2u : 4u); // 11 is reserved: treated as 32 bits
}

std::uint32_t size_mask(unsigned size)
{
    return size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u);
}

} // namespace

Stm32F1Dma::Stm32F1Dma(DmaHost& host) : host_(host)
{
    reset();
}

void Stm32F1Dma::reset()
{
    ch_ = {};
    isr_ = 0;
    for (int i = 0; i < channels; ++i) {
        update_irq(i);
    }
}

bool Stm32F1Dma::ready(int channel) const
{
    if (channel < 1 || channel > channels) {
        return false;
    }
    const auto& c = ch_[static_cast<std::size_t>(channel - 1)];
    return (c.ccr & CCR_EN) && !(c.ccr & CCR_MEM2MEM) && c.cndtr > 0;
}

bool Stm32F1Dma::request(int channel)
{
    if (!ready(channel)) {
        return false;
    }
    transfer(channel - 1);
    return true;
}

void Stm32F1Dma::transfer(int index)
{
    auto& c = ch_[static_cast<std::size_t>(index)];
    const unsigned psize = item_size((c.ccr >> 8) & 3u);
    const unsigned msize = item_size((c.ccr >> 10) & 3u);
    const bool to_peripheral = (c.ccr & CCR_DIR) != 0; // DIR: read from memory
    const std::uint32_t from = to_peripheral ? c.memory : c.peripheral;
    const std::uint32_t to = to_peripheral ? c.peripheral : c.memory;
    const unsigned from_size = to_peripheral ? msize : psize;
    const unsigned to_size = to_peripheral ? psize : msize;
    Bus& bus = host_.dma_bus();
    std::uint32_t value = 0;
    // Different sizes: the item is truncated or zero-extended (RM0008 table 76).
    if (!bus.read(from, from_size, value) || !bus.write(to, to_size, value & size_mask(to_size))) {
        isr_ |= teif(index) | gif(index);
        c.ccr &= ~CCR_EN; // a transfer error disables the channel
        host_.dma_diagnostic("DMA1 channel " + std::to_string(index + 1) + ": transfer error (no memory or peripheral at the address).");
        update_irq(index);
        return;
    }
    if (c.ccr & CCR_PINC) {
        c.peripheral += psize;
    }
    if (c.ccr & CCR_MINC) {
        c.memory += msize;
    }
    --c.cndtr;
    const std::uint32_t done = c.reload - c.cndtr;
    if (c.reload / 2 > 0 && done == c.reload / 2) {
        isr_ |= htif(index) | gif(index);
    }
    if (c.cndtr == 0) {
        isr_ |= tcif(index) | gif(index);
        if (c.ccr & CCR_CIRC) {
            c.cndtr = c.reload;
            c.peripheral = c.cpar;
            c.memory = c.cmar;
        }
    }
    update_irq(index);
}

void Stm32F1Dma::memory_to_memory(int index)
{
    auto& c = ch_[static_cast<std::size_t>(index)];
    while ((c.ccr & CCR_EN) && c.cndtr > 0) {
        transfer(index);
        if (c.ccr & CCR_CIRC) {
            break; // circular memory-to-memory is not allowed (RM0008 13.3.3)
        }
    }
}

void Stm32F1Dma::update_irq(int index)
{
    const auto& c = ch_[static_cast<std::size_t>(index)];
    const bool level = ((c.ccr & CCR_TCIE) && (isr_ & tcif(index))) || ((c.ccr & CCR_HTIE) && (isr_ & htif(index)))
        || ((c.ccr & CCR_TEIE) && (isr_ & teif(index)));
    host_.dma_irq(11 + index, level); // DMA1_Channel1..7: IRQ 11..17
}

bool Stm32F1Dma::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    const std::uint32_t word = read32(offset & ~3u);
    const unsigned shift = 8 * (offset & 3u);
    value = (word >> shift) & size_mask(size);
    return true;
}

bool Stm32F1Dma::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    const unsigned shift = 8 * (offset & 3u);
    const std::uint32_t mask = size_mask(size) << shift;
    write32(offset & ~3u, (value << shift) & mask, mask);
    return true;
}

std::uint32_t Stm32F1Dma::read32(std::uint32_t offset)
{
    if (offset == 0x00) {
        return isr_;
    }
    if (offset < 0x08 || offset >= 0x08 + 0x14 * channels) {
        return 0; // IFCR is write-only
    }
    const auto& c = ch_[(offset - 0x08) / 0x14];
    switch ((offset - 0x08) % 0x14) {
    case 0x0: return c.ccr;
    case 0x4: return c.cndtr;
    case 0x8: return c.cpar;
    case 0xC: return c.cmar;
    default: return 0;
    }
}

void Stm32F1Dma::write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask)
{
    auto merge = [mask, value](std::uint32_t old) { return (old & ~mask) | (value & mask); };
    if (offset == 0x04) { // IFCR: CGIFx clears the channel's four flags
        const std::uint32_t clear = value & mask;
        for (int i = 0; i < channels; ++i) {
            const std::uint32_t bits = (clear >> (4 * i)) & 15u;
            if (bits) {
                isr_ &= ~(((bits & 1u) ? 15u : bits) << (4 * i));
                update_irq(i);
            }
        }
        return;
    }
    if (offset < 0x08 || offset >= 0x08 + 0x14 * channels) {
        return;
    }
    const int index = static_cast<int>((offset - 0x08) / 0x14);
    auto& c = ch_[static_cast<std::size_t>(index)];
    const bool enabled = (c.ccr & CCR_EN) != 0;
    switch ((offset - 0x08) % 0x14) {
    case 0x0: {
        c.ccr = merge(c.ccr) & 0x7FFFu;
        if (!enabled && (c.ccr & CCR_EN)) {
            c.peripheral = c.cpar; // the channel starts from the programmed addresses
            c.memory = c.cmar;
            if (c.ccr & CCR_MEM2MEM) {
                memory_to_memory(index);
            } else {
                host_.dma_channel_enabled(index + 1);
            }
        }
        update_irq(index);
        break;
    }
    case 0x4:
        if (!enabled) { // read-only while the channel is enabled
            c.cndtr = c.reload = merge(c.cndtr) & 0xFFFFu;
        }
        break;
    case 0x8:
        if (!enabled) {
            c.cpar = merge(c.cpar);
        }
        break;
    case 0xC:
        if (!enabled) {
            c.cmar = merge(c.cmar);
        }
        break;
    default: break;
    }
}

} // namespace simnodus::emu
