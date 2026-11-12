// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The USART of the STM32F1 and STM32F4 (RM0008 27.6, RM0090 30.6: the same
// registers; the F4 adds oversampling by 8), shared by both machines
// (experiment E-12, SN-294; it was part of stm32f103.cpp). Internal to
// src/emu.
//
// Frames (SN-175): with BRR set, a byte goes out on the TX pin bit by bit at
// PCLK / BRR (start, 8 or 9 data bits with parity, stop bits); TXE, TC and the
// transmit interrupts follow the shift register.
// Receiving from the pin (SN-192): input edges of the RX pin are kept with
// the times they happened, like the hardware's center sample: a falling edge
// starts a frame, the start bit must still be low half a bit later, data bits
// are read at their centers, a low stop bit sets FE, a wrong parity PE. A
// frame is decided `rx_lag_ns` after its stop bit's center (inputs reach the
// chip late); the byte then joins the received bytes like a terminal byte.
#pragma once

#include "reg_device.hpp"
#include "timed_device.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::emu {

// How the machine wants its USARTs to behave.
struct UsartOptions {
    bool frames = true;              // false: bytes move at once, without frames on the pins
    std::uint64_t rx_lag_ns = 0;     // a frame is decided this long after its stop bit's center
    std::uint32_t rx_pins = ~0u;     // bit n-1 set: USART n samples its RX pin
    std::uint64_t rx_min_bit_ns = 0; // bits shorter than this are not decoded; 0: any baud rate
    std::uint64_t exact_input_pins = 0; // bit port * 16 + pin (ports A to D): edges there come at their exact times
};

class UsartHost {
public:
    virtual ~UsartHost() = default;
    virtual std::uint64_t usart_now_cycles() const = 0;
    virtual std::uint32_t usart_hclk_hz() const = 0;
    // HCLK cycles per cycle of the bus clock of USART `number` (its APB divider).
    virtual std::uint32_t usart_apb_divider(int number) const = 0;
    // The pins of USART `number` as the chip routes them now; port -1: none.
    virtual std::pair<int, int> usart_tx_pin(int number) const = 0;
    virtual std::pair<int, int> usart_rx_pin(int number) const = 0;
    virtual void usart_irq(int irq, bool level) = 0;
    // The TX line's level: now, or time-stamped at a bit edge.
    virtual void usart_line(int port, int pin, int number, bool level) = 0;
    virtual void usart_line_at(int port, int pin, int number, bool level, std::uint64_t cycle) = 0;
    // A byte whose frame ended at `cycle`.
    virtual void usart_byte(int number, std::uint8_t value, std::uint64_t cycle) = 0;
    // A DMA request of the receiver or the transmitter; true if a transfer was made.
    virtual bool usart_dma(int number, bool transmit) = 0;
    virtual void usart_schedule_changed() = 0;
    virtual void usart_diagnostic(const std::string& text) = 0;
};

class Stm32Usart : public RegDevice, public TimedDevice {
public:
    // `oversampling_by_8`: the chip has CR1.OVER8 and CR3.ONEBIT (the STM32F4).
    // `name`: "UART4" where the chip calls it so; "USART<number>" otherwise.
    Stm32Usart(UsartHost& host, const UsartOptions& options, int number, int irq, bool oversampling_by_8 = false,
        const char* name = nullptr)
        : host_(host), options_(options), number_(number), irq_(irq), over8_(oversampling_by_8)
    {
        if (name) {
            std::snprintf(name_, sizeof name_, "%s", name);
        } else {
            std::snprintf(name_, sizeof name_, "USART%d", number);
        }
        reset();
    }
    const char* name() const override { return name_; }

    void reset() override
    {
        sr_ = 0xC0; // TXE, TC
        dr_ = 0;
        brr_ = cr1_ = cr2_ = cr3_ = gtpr_ = 0;
        rx_.clear();
        rx_last_ready_ = 0;
        rx_edges_.clear();
        rx_base_ = true;
        rx_scan_ = 0;
        shifting_ = tdr_full_ = false;
        line_ = true;
        update();
    }

    void receive(std::uint8_t value)
    {
        const std::uint64_t now = host_.usart_now_cycles();
        std::uint64_t ready = now;
        if (const std::uint64_t frame = frame_cycles(); frame > 0) {
            ready = std::max(now, rx_last_ready_) + frame; // one frame after the previous byte
        }
        rx_last_ready_ = ready;
        rx_.push_back({value, ready, 0});
        host_.usart_schedule_changed();
        update();
    }

    // A level change on a pin at `cycle` (not after now); only the RX pin counts.
    void pin_changed(int port, int pin, bool level, std::uint64_t cycle)
    {
        if (!(options_.rx_pins & (1u << (number_ - 1))) || host_.usart_rx_pin(number_) != std::pair{port, pin}) {
            return;
        }
        const auto at = std::upper_bound(rx_edges_.begin(), rx_edges_.end(), cycle,
            [](std::uint64_t c, const std::pair<std::uint64_t, bool>& e) { return c < e.first; });
        rx_edges_.insert(at, {cycle, level});
        host_.usart_schedule_changed(); // the run loop asks next_event() again
    }

    std::uint64_t next_event() const override
    {
        std::uint64_t next = shifting_ ? next_edge_ : no_event;
        if (const auto start = rx_start()) {
            next = std::min(next, rx_decision(*start));
        }
        // A received byte is an event only while the receiver can take it (UE, RE,
        // DR empty); otherwise it waits without stopping the core again and again.
        if (!rx_.empty() && !(sr_ & 0x20u) && (cr1_ & (1u << 13)) && (cr1_ & (1u << 2))) {
            next = std::min(next, rx_.front().ready);
        }
        return next;
    }

    void on_time(std::uint64_t cycle) override
    {
        while (shifting_ && next_edge_ <= cycle) {
            const std::uint64_t at = next_edge_;
            if (++symbol_ < symbols_.size()) {
                set_line(symbols_[symbol_].first, at);
                next_edge_ = at + symbols_[symbol_].second;
                continue;
            }
            shifting_ = false; // the stop bit ended: the byte is on the line
            host_.usart_byte(number_, frame_value_, at);
            if (tdr_full_) {
                tdr_full_ = false;
                start_frame(tdr_, at);
                sr_ |= 0x80u; // TXE: the next byte moved to the shift register
            } else {
                sr_ |= 0x40u; // TC
            }
        }
        decode_rx(cycle);
        update_rx(cycle);
        update();
    }

    void update()
    {
        const bool enabled = (cr1_ & (1u << 13)) != 0;
        update_rx(host_.usart_now_cycles());
        // DMA (CR3.DMAR, CR3.DMAT): the channel moves received bytes out of DR and
        // writes bytes to send into it; each transfer re-enters update(), hence the guard.
        if (enabled && !in_dma_) {
            in_dma_ = true;
            while ((cr3_ & (1u << 6)) && (sr_ & 0x20u) && host_.usart_dma(number_, false)) {
            }
            while ((cr3_ & (1u << 7)) && (cr1_ & (1u << 3)) && (sr_ & 0x80u) && host_.usart_dma(number_, true)) {
            }
            in_dma_ = false;
        }
        // TX pin: the line rests high while the transmitter is on.
        const auto [port, pin] = host_.usart_tx_pin(number_);
        if (port != tx_port_ || pin != tx_pin_) {
            if (tx_port_ >= 0) {
                host_.usart_line(tx_port_, tx_pin_, number_, false);
            }
            tx_port_ = port;
            tx_pin_ = pin;
        }
        if (tx_port_ >= 0) {
            // The line rests high while the transmitter is on; a frame drives its bits.
            host_.usart_line(tx_port_, tx_pin_, number_, enabled && (cr1_ & (1u << 3)) && line_);
        }
        const bool irq = enabled
            && (((cr1_ & (1u << 7)) && (sr_ & 0x80u)) || ((cr1_ & (1u << 6)) && (sr_ & 0x40u))
                || ((cr1_ & (1u << 5)) && (sr_ & 0x28u)) || ((cr1_ & (1u << 4)) && (sr_ & 0x10u))
                || ((cr1_ & (1u << 8)) && (sr_ & 0x01u)));
        host_.usart_irq(irq_, irq);
    }

protected:
    bool read32(std::uint32_t offset, std::uint32_t& value) override
    {
        on_time(host_.usart_now_cycles());
        switch (offset) {
        case 0x00: value = sr_; return true;
        case 0x04:
            value = dr_;
            if (!peeking()) {
                sr_ &= ~0x2Fu; // reading DR clears RXNE (and PE, FE, NE, ORE after an SR read)
                update();
            }
            return true;
        case 0x08: value = brr_; return true;
        case 0x0C: value = cr1_; return true;
        case 0x10: value = cr2_; return true;
        case 0x14: value = cr3_; return true;
        case 0x18: value = gtpr_; return true;
        default: value = 0; return true;
        }
    }
    bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) override
    {
        on_time(host_.usart_now_cycles());
        switch (offset) {
        case 0x00: // rc_w0 bits: CTS, LBD, TC, RXNE
            sr_ &= ~(~value & mask & 0x360u);
            break;
        case 0x04: {
            const auto byte = static_cast<std::uint16_t>(value & 0x1FFu);
            const bool sending = (cr1_ & (1u << 13)) && (cr1_ & (1u << 3));
            if (sending && frame_cycles() > 0) {
                sr_ &= ~0x40u; // TC: a frame is under way
                if (!shifting_) {
                    start_frame(byte, host_.usart_now_cycles()); // straight into the shift register: TXE stays set
                } else {
                    tdr_ = byte;
                    tdr_full_ = true;
                    sr_ &= ~0x80u; // TXE: the data register holds the next byte
                }
            } else {
                if (sending) {
                    host_.usart_byte(number_, static_cast<std::uint8_t>(byte), host_.usart_now_cycles());
                }
                sr_ |= 0xC0u; // no baud rate set: transmitted at once
            }
            break;
        }
        case 0x08: brr_ = merge(brr_, value, mask) & 0xFFFFu; break;
        case 0x0C: cr1_ = merge(cr1_, value, mask) & (over8_ ? 0xBFFFu : 0x3FFFu); break;
        case 0x10: cr2_ = merge(cr2_, value, mask) & 0x7F7Fu; break;
        case 0x14: cr3_ = merge(cr3_, value, mask) & (over8_ ? 0xFFFu : 0x7FFu); break;
        case 0x18: gtpr_ = merge(gtpr_, value, mask) & 0xFFFFu; break;
        default: break;
        }
        update();
        return true;
    }

private:
    // HCLK cycles per bit: BRR is PCLK / baud, and PCLK = HCLK / the APB divider.
    std::uint64_t bit_cycles() const
    {
        // Oversampling by 8 (CR1.OVER8, STM32F4): the mantissa counts eighths
        // and three fraction bits are used.
        const std::uint64_t divider = (over8_ && (cr1_ & (1u << 15))) ? (std::uint64_t{brr_ >> 4} * 8u + (brr_ & 7u))
                                                                      : std::uint64_t{brr_};
        return divider * host_.usart_apb_divider(number_);
    }

    std::uint64_t frame_cycles() const
    {
        if (!options_.frames) {
            return 0; // bytes move at once
        }
        const std::uint64_t bit = bit_cycles();
        const int data = (cr1_ & (1u << 12)) ? 9 : 8;
        return bit * static_cast<std::uint64_t>(1 + data) + bit * stop_halves() / 2;
    }

    int stop_halves() const
    {
        static const int halves[4] = {2, 1, 4, 3}; // CR2.STOP: 1, 0.5, 2, 1.5 stop bits
        return halves[(cr2_ >> 12) & 3u];
    }

    void start_frame(std::uint16_t value, std::uint64_t at)
    {
        const std::uint64_t bit = bit_cycles();
        const int data = (cr1_ & (1u << 12)) ? 9 : 8;
        std::uint32_t bits = value & ((1u << data) - 1u);
        if (cr1_ & (1u << 10)) { // PCE: the last data bit is the parity bit
            const std::uint32_t payload = bits & ((1u << (data - 1)) - 1u);
            int ones = 0;
            for (std::uint32_t v = payload; v; v >>= 1) {
                ones += static_cast<int>(v & 1u);
            }
            const bool odd = (cr1_ & (1u << 9)) != 0;
            const bool parity = odd ? (ones % 2 == 0) : (ones % 2 == 1);
            bits = payload | (parity ? 1u << (data - 1) : 0u);
        }
        symbols_.clear();
        symbols_.emplace_back(false, bit); // start bit
        for (int i = 0; i < data; ++i) {
            symbols_.emplace_back(((bits >> i) & 1u) != 0, bit); // LSB first
        }
        symbols_.emplace_back(true, bit * static_cast<std::uint64_t>(stop_halves()) / 2);
        frame_value_ = static_cast<std::uint8_t>(value & 0xFFu);
        symbol_ = 0;
        shifting_ = true;
        set_line(false, at);
        next_edge_ = at + bit;
        host_.usart_schedule_changed(); // the machine's run loop picks up the next edge
    }

    void set_line(bool level, std::uint64_t at)
    {
        line_ = level;
        if (tx_port_ >= 0 && (cr1_ & (1u << 13)) && (cr1_ & (1u << 3))) {
            host_.usart_line_at(tx_port_, tx_pin_, number_, level, at); // time-stamped at the bit edge
        }
    }

    void update_rx(std::uint64_t cycle)
    {
        const bool enabled = (cr1_ & (1u << 13)) != 0;
        if (enabled && (cr1_ & (1u << 2)) && !(sr_ & 0x20u) && !rx_.empty() && rx_.front().ready <= cycle) {
            dr_ = rx_.front().value;
            sr_ |= 0x20u | rx_.front().errors; // RXNE, with the byte's FE and PE
            rx_.pop_front();
        }
    }

    // Level of the RX line at `cycle` from the kept edges.
    bool rx_level_at(std::uint64_t cycle) const
    {
        bool level = rx_base_;
        for (const auto& [at, value] : rx_edges_) {
            if (at > cycle) {
                break;
            }
            level = value;
        }
        return level;
    }

    // The first falling edge at or after rx_scan_: a frame may start there.
    std::optional<std::uint64_t> rx_start() const
    {
        bool level = rx_base_;
        for (const auto& [at, value] : rx_edges_) {
            if (at >= rx_scan_ && level && !value) {
                return at;
            }
            level = value;
        }
        return std::nullopt;
    }

    std::uint64_t rx_stop_sample(std::uint64_t start) const
    {
        const std::uint64_t bit = std::max<std::uint64_t>(bit_cycles(), 1);
        const int data = (cr1_ & (1u << 12)) ? 9 : 8;
        return start + bit / 2 + bit * static_cast<std::uint64_t>(data + 1);
    }

    std::uint64_t rx_decision(std::uint64_t start) const
    {
        const std::uint64_t lag = options_.rx_lag_ns * host_.usart_hclk_hz() / 1000000000u;
        return rx_stop_sample(start) + lag;
    }

    void decode_rx(std::uint64_t cycle)
    {
        while (const auto start = rx_start()) {
            if (rx_decision(*start) > cycle) {
                break;
            }
            const std::uint64_t bit = bit_cycles();
            bool on = (cr1_ & (1u << 13)) && (cr1_ & (1u << 2)) && bit > 0 && options_.frames;
            const auto [rx_port, rx_number] = host_.usart_rx_pin(number_);
            const bool exact = rx_port >= 0 && rx_port < 4
                && ((options_.exact_input_pins >> (rx_port * 16 + rx_number)) & 1u) != 0;
            if (on && !exact && bit * 1000000000u < options_.rx_min_bit_ns * host_.usart_hclk_hz()) {
                on = false;
                if (!rx_too_fast_reported_) {
                    rx_too_fast_reported_ = true;
                    char text[200];
                    std::snprintf(text, sizeof text,
                        "%s: frames on the RX pin are not decoded at %u baud: the circuit's step cannot show bits that "
                        "short (turn on Exact microcontroller edges or lower the maximum step)",
                        name_, static_cast<unsigned>(host_.usart_hclk_hz() / bit));
                    host_.usart_diagnostic(text);
                }
            }
            if (!on || rx_level_at(*start + bit / 2)) {
                rx_scan_ = *start + 1; // receiver off, or a glitch rather than a start bit
            } else {
                const int data = (cr1_ & (1u << 12)) ? 9 : 8;
                std::uint32_t bits = 0;
                for (int i = 0; i < data; ++i) {
                    if (rx_level_at(*start + bit / 2 + bit * static_cast<std::uint64_t>(i + 1))) {
                        bits |= 1u << i;
                    }
                }
                const std::uint64_t stop = rx_stop_sample(*start);
                std::uint8_t errors = rx_level_at(stop) ? 0 : 0x02u; // FE
                if (cr1_ & (1u << 10)) { // PCE: even (or odd) number of ones with the parity bit
                    int ones = 0;
                    for (std::uint32_t v = bits; v; v >>= 1) {
                        ones += static_cast<int>(v & 1u);
                    }
                    const bool odd = (cr1_ & (1u << 9)) != 0;
                    if ((ones % 2 == 1) != odd) {
                        errors |= 0x01u; // PE
                    }
                    bits &= (1u << (data - 1)) - 1u;
                }
                rx_.push_back({static_cast<std::uint8_t>(bits & 0xFFu), cycle, errors});
                rx_scan_ = stop;
            }
            // Edges before the scan point only matter for the level there.
            while (!rx_edges_.empty() && rx_edges_.front().first < rx_scan_) {
                rx_base_ = rx_edges_.front().second;
                rx_edges_.pop_front();
            }
        }
    }

    UsartHost& host_;
    const UsartOptions& options_;
    int number_;
    int irq_;
    bool over8_;
    char name_[8] = {};
    std::uint32_t sr_ = 0, dr_ = 0, brr_ = 0, cr1_ = 0, cr2_ = 0, cr3_ = 0, gtpr_ = 0;
    struct RxByte {
        std::uint8_t value = 0;
        std::uint64_t ready = 0; // cycle at which its stop bit ends
        std::uint8_t errors = 0; // SR flags that come with it (FE, PE)
    };
    std::deque<RxByte> rx_;
    std::uint64_t rx_last_ready_ = 0;
    std::deque<std::pair<std::uint64_t, bool>> rx_edges_; // RX pin: cycle, new level
    bool rx_base_ = true;       // RX level before the first kept edge
    std::uint64_t rx_scan_ = 0; // a start bit is looked for from here
    bool rx_too_fast_reported_ = false;
    bool shifting_ = false; // a frame is on the line
    std::vector<std::pair<bool, std::uint64_t>> symbols_; // level and length in cycles
    std::size_t symbol_ = 0;
    std::uint64_t next_edge_ = 0;
    std::uint8_t frame_value_ = 0;
    bool line_ = true;      // TX level the frame drives
    bool tdr_full_ = false; // a byte waits in DR for the shift register
    std::uint16_t tdr_ = 0;
    bool in_dma_ = false;
    int tx_port_ = -1;
    int tx_pin_ = 0;
};

} // namespace simnodus::emu
