// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "esp32c3.hpp"

#include "esp32c3_rom.hpp"
#include "sha.hpp"
#include "timed_device.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <deque>
#include <cstdio>

namespace simnodus::emu {

namespace {

// Memory map (TRM chapter 3).
constexpr std::uint32_t sram0_ibus = 0x4037C000u, sram0_size = 0x4000u;
constexpr std::uint32_t sram1_ibus = 0x40380000u, sram1_dbus = 0x3FC80000u, sram1_size = 0x60000u;
constexpr std::uint32_t rtc_fast = 0x50000000u, rtc_fast_size = 0x2000u;

// Peripherals (components/soc/esp32c3/register/soc/reg_base.h).
constexpr std::uint32_t uart0_base = 0x60000000u, uart1_base = 0x60010000u;
constexpr std::uint32_t gpio_base = 0x60004000u;
constexpr std::uint32_t rtc_cntl_base = 0x60008000u, efuse_base = 0x60008800u;
constexpr std::uint32_t usb_serial_jtag_base = 0x60043000u;
constexpr std::uint32_t spi1_base = 0x60002000u, spi0_base = 0x60003000u;
constexpr std::uint32_t extmem_base = 0x600C4000u, mmu_table_base = 0x600C5000u;
constexpr std::uint32_t flash_ibus = 0x42000000u, flash_dbus = 0x3C000000u;
constexpr std::uint32_t cache_window_size = 0x800000u, cache_page = 0x10000u;

RiscvCpu::Config core_config()
{
    RiscvCpu::Config config;
    config.reset_pc = esp32c3_rom_base;
    config.mvendorid = 0x612; // Espressif's JEDEC bank and number
    config.interrupt_lines = 0xFFFFFFFEu;
    config.counters = false;   // the cycle counter is the vendor CSR mpccr
    config.vectored_only = true;
    config.mie_gates = false;  // the interrupt controller enables lines
    config.interrupt_latency = 3;
    config.misaligned_access = true; // Arduino-ESP32's newlib strcmp loads words at odd addresses
    return config;
}

std::string hex(std::uint32_t value)
{
    char text[12];
    std::snprintf(text, sizeof text, "0x%08X", static_cast<unsigned>(value));
    return text;
}

} // namespace

// ---------------------------------------------------------------------------
// Register blocks.

class Esp32C3::Block : public Device {
public:
    Block(Esp32C3& machine, const char* name, std::uint32_t size = 0x1000)
        : machine_(machine), name_(name), regs_(size / 4, 0)
    {
    }
    const char* name() const override { return name_; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override
    {
        const std::uint32_t word = read_word(offset & ~3u);
        value = word >> (8 * (offset & 3));
        if (size < 4) {
            value &= (1u << (8 * size)) - 1;
        }
        machine_.trace_access(*this, offset, false, value);
        return true;
    }
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override
    {
        std::uint32_t word = value;
        if (size < 4) {
            const std::uint32_t shift = 8 * (offset & 3);
            const std::uint32_t mask = ((1u << (8 * size)) - 1) << shift;
            word = (regs_[(offset & ~3u) / 4] & ~mask) | ((value << shift) & mask);
        }
        machine_.trace_access(*this, offset, true, value);
        write_word(offset & ~3u, word);
        return true;
    }
    void reset() override
    {
        std::fill(regs_.begin(), regs_.end(), 0);
        on_reset();
    }
    // A byte from outside (serial ports: the host's side).
    virtual void receive(std::uint8_t byte) { (void)byte; }
    // A pad's level changed (receivers that decode their input signal).
    virtual void pad_changed() {}

protected:
    // Plain storage unless a peripheral says otherwise.
    virtual std::uint32_t read_word(std::uint32_t offset) { return regs_[offset / 4]; }
    virtual void write_word(std::uint32_t offset, std::uint32_t value) { regs_[offset / 4] = value; }
    virtual void on_reset() {}
    std::uint32_t& reg(std::uint32_t offset) { return regs_[offset / 4]; }
    std::uint32_t reg(std::uint32_t offset) const { return regs_[offset / 4]; }
    void emit(int port, std::uint8_t value) { machine_.output_byte(port, value); }
    FlashChip& flash_chip() { return *machine_.flash_chip_; }
    std::uint32_t& mmu(std::size_t page) { return machine_.mmu_[page]; }
    void map_page(std::size_t page) { machine_.map_page(page); }
    void invalidate_cache() { machine_.invalidate_cache(); }
    void set_cpu_hz(std::uint32_t hz) { machine_.set_cpu_hz(hz); }
    RiscvCpu& cpu() { return machine_.cpu_; }
    std::uint64_t now_ns() const { return machine_.time_ns(); }
    void reschedule() { machine_.cpu_.request_stop(); } // a timed event moved
    void raise(int source, bool level) { machine_.set_interrupt_source(source, level); }
    void request_reset(ResetKind kind, std::uint32_t reason = 0) { machine_.request_reset(kind, reason); }
    void refresh_pads(); // the GPIO evaluates every pad again
    void pads_changed(); // the UARTs' receivers look at their input
    void enter_sleep(bool deep, std::uint64_t wake_ns) { machine_.enter_sleep(deep, wake_ns); }
    void note(const std::string& text) { machine_.diagnostic(text); }
    void drive_signal(int signal, bool level) { machine_.set_signal(signal, level); }
    bool input_signal(int signal) const; // through the GPIO matrix (FUNCn_IN_SEL_CFG)
    void claim_signal(int signal) { machine_.signal_driven_[static_cast<std::size_t>(signal)] = true; }
    std::optional<double> pad_volts(int gpio) const { return machine_.pin_voltage(gpio); }
    // Memory seen by a DMA engine: `size` 1, 2 or 4 bytes.
    std::uint32_t load(std::uint32_t address, unsigned size)
    {
        std::uint32_t value = 0;
        machine_.bus_.read(address, size, value);
        return value;
    }
    void store(std::uint32_t address, unsigned size, std::uint32_t value) { machine_.bus_.write(address, size, value); }
    // A register of another block (no trace).
    std::uint32_t peek(std::uint32_t address)
    {
        std::uint32_t value = 0;
        const auto saved = std::move(machine_.trace_);
        machine_.bus_.read(address, 4, value);
        machine_.trace_ = saved;
        return value;
    }

    Esp32C3& machine_;

private:
    const char* name_;
    std::vector<std::uint32_t> regs_;
};

// ---------------------------------------------------------------------------
// The SPI NOR flash: a GigaDevice GD25Q-like part (JEDEC ID C8 40 xx, the
// capacity byte from the flash size), as on ESP32-C3 modules. Programming and
// erasing finish at once; the content is the machine's flash memory.

class Esp32C3::FlashChip {
public:
    explicit FlashChip(std::vector<std::uint8_t>& memory) : memory_(memory) {}

    // One transaction: a command byte, an optional address, bytes sent to the
    // chip (`mosi`) and `miso_count` bytes read back into `miso`.
    void command(std::uint8_t cmd, bool has_address, std::uint32_t address, const std::uint8_t* mosi,
        std::size_t mosi_count, std::uint8_t* miso, std::size_t miso_count)
    {
        std::fill(miso, miso + miso_count, std::uint8_t{0xFF});
        const std::uint32_t size = static_cast<std::uint32_t>(memory_.size());
        switch (cmd) {
        case 0x9F: { // RDID
            unsigned capacity = 0;
            while ((1u << capacity) < size) {
                ++capacity;
            }
            const std::uint8_t id[3] = {0xC8, 0x40, static_cast<std::uint8_t>(capacity)};
            for (std::size_t i = 0; i < miso_count; ++i) {
                miso[i] = id[i % 3];
            }
            break;
        }
        case 0x05: fill(miso, miso_count, static_cast<std::uint8_t>(status_ | (wel_ ? 2 : 0))); break; // RDSR
        case 0x35: fill(miso, miso_count, static_cast<std::uint8_t>(status_ >> 8)); break;             // RDSR2
        case 0x15: fill(miso, miso_count, 0); break;                                                    // RDSR3
        case 0x01: // WRSR (one or two bytes)
            if (mosi_count >= 1) {
                status_ = static_cast<std::uint16_t>((status_ & 0xFF00) | (mosi[0] & 0xFC));
            }
            if (mosi_count >= 2) {
                status_ = static_cast<std::uint16_t>((status_ & 0x00FF) | (mosi[1] << 8));
            }
            wel_ = false;
            break;
        case 0x31: // WRSR2
            if (mosi_count >= 1) {
                status_ = static_cast<std::uint16_t>((status_ & 0x00FF) | (mosi[0] << 8));
            }
            wel_ = false;
            break;
        case 0x06: wel_ = true; break;  // WREN
        case 0x04: wel_ = false; break; // WRDI
        case 0x03: case 0x0B: case 0x3B: case 0x6B: case 0xBB: case 0xEB: // reads, any width
            for (std::size_t i = 0; i < miso_count; ++i) {
                miso[i] = memory_[(address + i) % size];
            }
            break;
        case 0x02: // page program: bits only go from 1 to 0, within one 256-byte page
            if (wel_ && has_address) {
                for (std::size_t i = 0; i < mosi_count; ++i) {
                    const std::uint32_t at = ((address & ~0xFFu) | ((address + i) & 0xFFu)) % size;
                    memory_[at] &= mosi[i];
                }
                written_ = true;
                ++writes_;
            }
            wel_ = false;
            break;
        case 0x20: erase(address, 0x1000); break;  // sector
        case 0x52: erase(address, 0x8000); break;  // 32 KiB block
        case 0xD8: erase(address, 0x10000); break; // 64 KiB block
        case 0x60: case 0xC7: erase(0, size); break; // chip
        case 0xAB: fill(miso, miso_count, static_cast<std::uint8_t>(0x15)); break; // release from deep power-down, ID
        case 0x4B: // unique ID after four dummy bytes
            for (std::size_t i = 0; i < miso_count; ++i) {
                miso[i] = static_cast<std::uint8_t>(0x5A ^ (i * 0x11));
            }
            break;
        default: break; // deep power-down, suspend, resume, reset, SFDP: no effect
        }
    }
    bool written() const { return written_; }
    std::uint64_t writes() const { return writes_; } // programs and erases so far
    std::uint8_t status_low() const { return static_cast<std::uint8_t>(status_ | (wel_ ? 2 : 0)); }
    void write_status(std::uint16_t value)
    {
        status_ = static_cast<std::uint16_t>(value & 0xFFFC);
        wel_ = false;
    }
    void set_write_enable(bool enable) { wel_ = enable; }

private:
    static void fill(std::uint8_t* out, std::size_t count, std::uint8_t value) { std::fill(out, out + count, value); }
    void erase(std::uint32_t address, std::uint32_t length)
    {
        if (wel_) {
            const std::uint32_t start = (address & ~(length - 1)) % static_cast<std::uint32_t>(memory_.size());
            std::fill(memory_.begin() + start, memory_.begin() + std::min<std::size_t>(memory_.size(), start + length), std::uint8_t{0xFF});
            written_ = true;
            ++writes_;
        }
        wel_ = false;
    }

    std::vector<std::uint8_t>& memory_;
    std::uint16_t status_ = 0;
    bool wel_ = false;
    bool written_ = false;
    std::uint64_t writes_ = 0;
};

namespace {

// A watchdog's four stages (TRM chapter "Watchdog Timers"): each stage runs
// for its timeout in ticks, then takes its action and the next stage starts;
// a feed starts stage 0 again. Actions: 0 none, 1 interrupt, 2 CPU reset,
// 3 system reset, 4 RTC reset (RTC watchdog only).
struct Wdt {
    bool enabled = false;
    std::uint32_t actions[4] = {};
    std::uint64_t timeouts[4] = {}; // ticks
    std::uint64_t tick_ps = 0;      // picoseconds per tick
    int stage = 0;
    std::uint64_t stage_start = 0;  // ns

    bool running() const
    {
        return enabled && tick_ps != 0 && (actions[0] | actions[1] | actions[2] | actions[3]) != 0;
    }
    void feed(std::uint64_t now)
    {
        stage = 0;
        stage_start = now;
    }
    std::uint64_t next_ns() const
    {
        if (!running()) {
            return no_event;
        }
        const std::uint64_t ticks = std::max<std::uint64_t>(timeouts[stage], 1);
        return stage_start + (ticks * tick_ps + 999) / 1000;
    }
    // The action of the stage that ran out (if it did), and on to the next stage.
    std::uint32_t expire(std::uint64_t now, int* expired_stage = nullptr)
    {
        if (now < next_ns()) {
            return 0;
        }
        if (expired_stage) {
            *expired_stage = stage;
        }
        const std::uint32_t action = actions[stage];
        stage = (stage + 1) % 4;
        stage_start = now;
        return action;
    }
};

// UART at the pin level (SN-244, SN-253, SN-254). One frame is a start bit,
// 5-8 data bits (LSB first), an optional parity bit and 1, 1.5 or 2 stop bits
// at the baud rate of CLKDIV and CLK_CONF (CONF0 gives the format).
// Transmit: the 128-byte TX FIFO sends one frame after the other on the
// U0TXD/U1TXD signal (6, 9), so a pad routed to it (GPIO21 by IO_MUX function
// 0, or any pad through the matrix) shows the bits; the byte reaches the
// terminal when its stop bits end. STATUS.TXFIFO_CNT counts the bytes still
// waiting; TXFIFO_EMPTY while no more than CONF1.TXFIFO_EMPTY_THRHD wait,
// TX_DONE when the last frame ends.
// Receive: bytes from outside (the terminal, a link to the computer) arrive
// one frame apart; the U0RXD/U1RXD signal (GPIO20 by IO_MUX function 0, or a
// pad through the matrix) is decoded too, sampled in the middle of each bit,
// once the line has been idle (high) for a frame, so a floating pad gives
// nothing. Received bytes wait in the RX FIFO (FIFO_REG, STATUS.RXFIFO_CNT);
// RXFIFO_FULL when the count reaches CONF1.RXFIFO_FULL_THRHD, RXFIFO_TOUT as
// soon as bytes wait with CONF1.RX_TOUT_EN set (the chip waits for the line to
// be idle); FRM_ERR when a stop bit is low.
// Autobaud (CONF0.AUTOBAUD_EN, SN-253): RXD_CNT counts the received frames'
// edges and LOWPULSE/HIGHPULSE give one bit of the present divider, so the
// ROM's download mode detects the baud rate it already uses. Interrupt
// sources 21 (UART0) and 22 (UART1).
class Uart : public Esp32C3::Block, public Esp32C3::Timed {
public:
    Uart(Esp32C3& machine, const char* name, int port)
        : Block(machine, name), port_(port), signal_(port == 0 ? 6 : 9)
    {
        claim_signal(signal_);
    }
    void receive(std::uint8_t byte) override
    {
        if (line_.empty()) {
            line_end_ = std::max(line_end_, now_ns()) + frame_ns();
            reschedule();
        }
        line_.push_back(byte);
    }
    std::uint64_t next_ns() const override
    {
        return std::min({line_.empty() ? no_event : line_end_, tx_next_, rx_next_});
    }
    void on_ns(std::uint64_t now) override
    {
        while (!line_.empty() && line_end_ <= now) {
            const std::uint8_t byte = line_.front();
            line_.pop_front();
            deliver(byte);
            if (!line_.empty()) {
                line_end_ += frame_ns();
            }
        }
        while (tx_next_ <= now) {
            transmit_step();
        }
        if (rx_next_ <= now) {
            receive_step();
        }
    }
    // Decoding the input signal: off for a port whose bytes come straight
    // from another board, and for bits shorter than `min_bit_ns` (the
    // circuit's step cannot show them; 0 for pins with exact edges).
    void set_pin_rx(bool on, std::uint64_t min_bit_ns)
    {
        pin_rx_ = on;
        min_bit_ns_ = min_bit_ns;
    }
    // A pad changed: the receiver looks at its input signal.
    void pad_changed() override
    {
        const bool level = input_signal(signal_);
        if (level == rx_level_ || !pin_rx_ || bit_ns() < static_cast<double>(min_bit_ns_)) {
            rx_level_ = level;
            return;
        }
        rx_level_ = level;
        const std::uint64_t now = now_ns();
        if (level) {
            high_since_ = now;
        } else if (rx_next_ == no_event && (rx_synced_ || (high_since_ != no_event && now >= high_since_ + frame_ns()))) {
            // A start bit: the first data bit's middle is 1.5 bits later.
            rx_bit_ns_ = bit_ns();
            rx_bits_ = 0;
            rx_value_ = 0;
            rx_next_ = now + static_cast<std::uint64_t>(1.5 * rx_bit_ns_);
            reschedule();
        }
    }

protected:
    int data_bits() const { return 5 + static_cast<int>((reg(0x20) >> 2) & 3); }
    bool parity() const { return (reg(0x20) & 2) != 0; }
    double stop_bits() const
    {
        const std::uint32_t stop = (reg(0x20) >> 4) & 3;
        return stop == 2 ? 1.5 : stop == 3 ? 2.0 : 1.0;
    }
    double bit_ns() const
    {
        const std::uint32_t clk_conf = reg(0x78);
        const std::uint32_t sel = (clk_conf >> 20) & 3; // 1 APB, 2 RC_FAST, 3 XTAL
        const double apb = machine_.cpu_hz() >= 80'000'000 ? 80e6 : machine_.cpu_hz();
        const double sclk = (sel == 1 ? apb : sel == 2 ? 17.5e6 : 40e6) / ((clk_conf & 0xFF) + 1.0);
        const std::uint32_t clkdiv = reg(0x14);
        const double divider = (clkdiv & 0xFFF) + ((clkdiv >> 20) & 0xF) / 16.0;
        const double baud = divider >= 1 ? sclk / divider : 115200.0;
        return 1e9 / baud;
    }
    // One frame on the line: start, data, parity and stop bits.
    std::uint64_t frame_ns() const
    {
        return static_cast<std::uint64_t>((1 + data_bits() + (parity() ? 1 : 0) + stop_bits()) * bit_ns());
    }

    void deliver(std::uint8_t byte)
    {
        if (rx_.size() < 128) {
            rx_.push_back(byte);
        } else {
            reg(0x04) |= 1u << 4; // RXFIFO_OVF
            if (!overflow_noted_) {
                overflow_noted_ = true;
                note(std::string(name()) + "'s receive FIFO overflowed: the firmware did not read the bytes in time");
            }
        }
        if (reg(0x20) & (1u << 27)) { // AUTOBAUD_EN
            // Edges of the frame: start bit (0), data LSB first, stop bit (1).
            const std::uint32_t frame = 0x200u | (static_cast<std::uint32_t>(byte) << 1);
            std::uint32_t edges = 1; // the idle line falls at the start bit
            for (int bit = 1; bit < 10; ++bit) {
                edges += ((frame >> bit) ^ (frame >> (bit - 1))) & 1;
            }
            reg(0x30) += edges;
            const std::uint32_t divider = reg(0x14) & 0xFFF;
            const std::uint32_t pulse = (divider ? divider : 347) - 1; // 40 MHz / 115200
            reg(0x28) = pulse; // LOWPULSE
            reg(0x2C) = pulse; // HIGHPULSE
        }
        update();
    }

    std::uint32_t read_word(std::uint32_t offset) override
    {
        switch (offset) {
        case 0x00: { // FIFO: the next received byte
            if (rx_.empty()) {
                return 0;
            }
            const std::uint8_t byte = rx_.front();
            rx_.pop_front();
            update();
            return byte;
        }
        case 0x08: return reg(0x04) & reg(0x0C); // INT_ST
        case 0x1C: // STATUS: RXFIFO_CNT, TXFIFO_CNT, and the lines (RXD bit 15, TXD bit 31)
            return static_cast<std::uint32_t>(rx_.size()) | (static_cast<std::uint32_t>(tx_.size()) << 16)
                | (rx_level_ ? 1u << 15 : 0u) | (tx_level_ ? 1u << 31 : 0u);
        case 0x64: // FSM_STATUS: ST_UTX_OUT (bits 7:4) is 0 only when the transmitter is idle
            return (tx_next_ == no_event ? 0u : 2u << 4) | (rx_next_ == no_event ? 0u : 2u);
        default: return Block::read_word(offset);
        }
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        switch (offset) {
        case 0x00: // FIFO: a byte to send
            if (tx_.size() < 128) {
                tx_.push_back(static_cast<std::uint8_t>(value));
            }
            if (tx_next_ == no_event) {
                start_frame(now_ns());
            }
            break;
        case 0x10: // INT_CLR
            reg(0x04) &= ~value;
            break;
        case 0x20: // CONF0: RXFIFO_RST (bit 17) empties the receiver, TXFIFO_RST (bit 18) the transmitter
            if (value & (1u << 17)) {
                rx_.clear();
            }
            if (value & (1u << 18)) {
                tx_.clear();
            }
            if (!(value & (1u << 27))) { // autobaud off: its counters restart
                reg(0x30) = 0;
            }
            Block::write_word(offset, value);
            break;
        default:
            Block::write_word(offset, value);
        }
        update();
    }
    void on_reset() override
    {
        rx_.clear();
        line_.clear();
        tx_.clear();
        tx_next_ = rx_next_ = no_event;
        rx_synced_ = false;
        high_since_ = now_ns();
        reg(0x24) = 0x60 | (0x60u << 9); // CONF1 reset value: both thresholds 96
        reg(0x04) = 1u << 1;             // TXFIFO_EMPTY
        set_tx_level(true);              // the idle line
    }

private:
    void set_tx_level(bool level)
    {
        tx_level_ = level;
        drive_signal(signal_, level);
    }
    // The next byte of the FIFO starts its frame at `at`: the start bit.
    void start_frame(std::uint64_t at)
    {
        if (tx_.empty()) {
            tx_next_ = no_event;
            return;
        }
        const std::uint8_t byte = tx_.front();
        tx_.pop_front();
        tx_byte_ = byte;
        const int bits = data_bits();
        std::uint32_t frame = byte & ((1u << bits) - 1); // data bits after the start bit
        if (parity()) {
            const bool odd = (reg(0x20) & 1) != 0;
            const bool ones = (std::popcount(frame) & 1) != 0;
            frame |= (ones != odd ? 1u : 0u) << bits;
        }
        tx_frame_ = frame;
        tx_frame_bits_ = bits + (parity() ? 1 : 0);
        tx_bit_ = -1; // the start bit
        tx_bit_ns_ = bit_ns();
        tx_start_ = at;
        set_tx_level(false);
        tx_next_ = at + static_cast<std::uint64_t>(tx_bit_ns_);
        reschedule();
        update();
    }
    // The present bit ended: the next one, the stop bits, or the next frame.
    void transmit_step()
    {
        ++tx_bit_;
        if (tx_bit_ < tx_frame_bits_) {
            set_tx_level(((tx_frame_ >> tx_bit_) & 1) != 0);
            tx_next_ = tx_start_ + static_cast<std::uint64_t>((tx_bit_ + 2) * tx_bit_ns_);
        } else if (tx_bit_ == tx_frame_bits_) {
            set_tx_level(true); // the stop bits
            tx_next_ = tx_start_ + static_cast<std::uint64_t>((tx_frame_bits_ + 1 + stop_bits()) * tx_bit_ns_);
        } else {
            emit(port_, tx_byte_);
            const std::uint64_t end = tx_next_;
            tx_next_ = no_event;
            if (tx_.empty()) {
                reg(0x04) |= 1u << 14; // TX_DONE
                update();
            } else {
                start_frame(end);
            }
        }
    }
    // The receiver samples its signal in the middle of a bit.
    void receive_step()
    {
        const bool level = input_signal(signal_);
        const int bits = data_bits();
        const int total = bits + (parity() ? 1 : 0);
        if (rx_bits_ < total) {
            if (rx_bits_ < bits && level) {
                rx_value_ |= 1u << rx_bits_;
            }
            ++rx_bits_;
            rx_next_ += static_cast<std::uint64_t>(rx_bit_ns_);
            return;
        }
        rx_next_ = no_event; // the stop bit's middle
        if (level) {
            rx_synced_ = true;
            deliver(static_cast<std::uint8_t>(rx_value_));
        } else {
            rx_synced_ = false; // a framing error (or a break): wait for an idle line
            high_since_ = no_event;
            reg(0x04) |= 1u << 3; // FRM_ERR
            update();
        }
        rx_level_ = level;
    }
    void update()
    {
        std::uint32_t raw = reg(0x04) & ~(1u << 1);
        if (tx_.size() <= ((reg(0x24) >> 9) & 0x1FF)) {
            raw |= 1u << 1; // TXFIFO_EMPTY
        }
        const std::uint32_t threshold = reg(0x24) & 0x1FF;
        if (!rx_.empty() && rx_.size() >= threshold) {
            raw |= 1u << 0; // RXFIFO_FULL
        }
        if (!rx_.empty() && (reg(0x24) & (1u << 21))) {
            raw |= 1u << 8; // RXFIFO_TOUT
        }
        reg(0x04) = raw;
        raise(port_ == 0 ? 21 : 22, (raw & reg(0x0C)) != 0);
    }

    int port_;
    int signal_; // U0TXD/U0RXD 6, U1TXD/U1RXD 9 in the GPIO matrix
    std::deque<std::uint8_t> rx_;
    std::deque<std::uint8_t> line_; // bytes from outside on the way, the first one ends at line_end_
    std::uint64_t line_end_ = 0;
    bool overflow_noted_ = false;
    // Transmitter: the frame being sent.
    std::deque<std::uint8_t> tx_;
    std::uint64_t tx_next_ = no_event; // the present bit ends
    std::uint64_t tx_start_ = 0;
    double tx_bit_ns_ = 0;
    std::uint32_t tx_frame_ = 0;
    int tx_frame_bits_ = 0;
    int tx_bit_ = 0;
    std::uint8_t tx_byte_ = 0;
    bool tx_level_ = true;
    // Receiver from the pin.
    bool pin_rx_ = true;
    std::uint64_t min_bit_ns_ = 0;
    bool rx_level_ = false;
    bool rx_synced_ = false;
    std::uint64_t high_since_ = no_event;
    std::uint64_t rx_next_ = no_event; // the next sample
    double rx_bit_ns_ = 0;
    int rx_bits_ = 0;
    std::uint32_t rx_value_ = 0;
};

// USB-Serial-JTAG's CDC serial port: the host reads every byte at once (the
// IN endpoint always has room) and its bytes wait in the OUT FIFO (EP1 reads
// them, EP1_CONF.SERIAL_OUT_EP_DATA_AVAIL tells). Interrupts (source 26):
// SERIAL_OUT_RECV_PKT while bytes wait, SERIAL_IN_EMPTY after each flush.
class UsbSerialJtag : public Esp32C3::Block {
public:
    explicit UsbSerialJtag(Esp32C3& machine) : Block(machine, "USB_SERIAL_JTAG") {}
    void receive(std::uint8_t byte) override
    {
        rx_.push_back(byte);
        reg(0x08) |= 1u << 2; // INT_RAW: SERIAL_OUT_RECV_PKT
        update();
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        switch (offset) {
        case 0x0: { // EP1: the next byte from the host
            if (rx_.empty()) {
                return 0;
            }
            const std::uint8_t byte = rx_.front();
            rx_.pop_front();
            return byte;
        }
        case 0x4: // EP1_CONF: SERIAL_IN_EP_DATA_FREE, SERIAL_OUT_EP_DATA_AVAIL
            return (1u << 1) | (rx_.empty() ? 0 : 1u << 2);
        case 0xC: // INT_ST
            return reg(0x08) & reg(0x10);
        default:
            return Block::read_word(offset);
        }
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        switch (offset) {
        case 0x0: // EP1: a byte for the host
            emit(2, static_cast<std::uint8_t>(value));
            return;
        case 0x4: // EP1_CONF: WR_DONE flushes the IN FIFO
            if (value & 1) {
                reg(0x08) |= 1u << 3; // SERIAL_IN_EMPTY
            }
            break;
        case 0x14: // INT_CLR
            reg(0x08) &= ~value;
            if (!rx_.empty()) {
                reg(0x08) |= 1u << 2; // still bytes to read
            }
            break;
        default:
            Block::write_word(offset, value);
        }
        update();
    }
    void on_reset() override
    {
        rx_.clear();
        reg(0x08) = 1u << 3; // IN FIFO empty
    }

private:
    void update() { raise(26, (reg(0x08) & reg(0x10)) != 0); }

    std::deque<std::uint8_t> rx_;
};

// IO_MUX: per pad function (MCU_SEL; 1 is the GPIO matrix), pulls, input
// enable; every change re-evaluates the pads.
class IoMux : public Esp32C3::Block {
public:
    explicit IoMux(Esp32C3& machine) : Block(machine, "IO_MUX") {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override;
    void on_reset() override
    {
        // Reset values (TRM): GPIO function, input enabled, the strapping
        // pads pulled up; USB D+ (GPIO19) pulled up by the USB PHY.
        for (std::uint32_t pin = 0; pin < Esp32C3::gpio_count; ++pin) {
            reg(0x04 + 4 * pin) = (1u << 12) | (1u << 9) | (2u << 10);
        }
        for (std::uint32_t pin : {2u, 8u, 9u}) {
            reg(0x04 + 4 * pin) |= 1u << 8;
        }
        // GPIO12-17 (SPI flash), GPIO18/19 (USB), GPIO20/21 (UART0): function 0.
        for (std::uint32_t pin = 12; pin < 22; ++pin) {
            reg(0x04 + 4 * pin) &= ~(7u << 12);
        }
    }
};

} // namespace

// GPIO matrix and pads (TRM chapter "IO MUX and GPIO Matrix"). A pad in the
// GPIO function (IO_MUX MCU_SEL 1) whose FUNCn_OUT_SEL is 128 drives
// GPIO_OUT's bit while GPIO_ENABLE's bit is set (PINn.PAD_DRIVER: open
// drain); another output signal, or another IO_MUX function, belongs to a
// peripheral. GPIO_IN reads the pads with input enabled: a driven level, or
// the circuit's level, or the pull. PINn.INT_TYPE (1 rising, 2 falling,
// 3 any, 4 low, 5 high) with INT_ENA bit 13 sets GPIO_STATUS and raises
// source 16.
class Esp32C3::Gpio : public Esp32C3::Block {
public:
    Gpio(Esp32C3& machine, std::uint32_t strapping) : Block(machine, "GPIO"), strapping_(strapping) {}
    void set_strapping(std::uint32_t strapping) { strapping_ = strapping; } // sampled at the next reset

    Esp32C3::PinState state(int pin) const { return states_[static_cast<std::size_t>(pin)]; }
    bool pad(int pin) const { return pads_[static_cast<std::size_t>(pin)]; }
    void set_input(int pin, std::optional<bool> level)
    {
        external_[static_cast<std::size_t>(pin)] = level;
        evaluate(pin);
    }
    void evaluate_all()
    {
        for (int pin = 0; pin < Esp32C3::gpio_count; ++pin) {
            evaluate(pin);
        }
    }
    // Input signal `signal` of the matrix: FUNCn_IN_SEL_CFG (0x154 + 4n) picks a
    // pad (IN_SEL bits 4:0; 0x1E constant 1, 0x1F constant 0) with SIG_IN_SEL
    // (bit 6) set, inverted by bit 5. A pad reads 0 unless its input is enabled.
    bool input_signal(int signal)
    {
        const std::uint32_t cfg = reg(0x154 + 4 * static_cast<std::uint32_t>(signal));
        const std::uint32_t sel = cfg & 0x1F;
        bool level = false;
        if (!(cfg & (1u << 6))) { // straight from the pad whose IO_MUX function is the signal
            for (int pin = 0; pin < Esp32C3::gpio_count; ++pin) {
                if (direct_input(pin) == signal
                    && static_cast<int>((peek(0x60009004u + 4 * static_cast<std::uint32_t>(pin)) >> 12) & 7) == direct_function(pin)) {
                    level = states_[static_cast<std::size_t>(pin)].input && pads_[static_cast<std::size_t>(pin)];
                }
            }
        } else if (sel == 0x1E) {
            level = true;
        } else if (sel < static_cast<std::uint32_t>(Esp32C3::gpio_count)) {
            level = states_[sel].input && pads_[sel];
        }
        return level != ((cfg & (1u << 5)) != 0);
    }
    // Only the pads routed to `signal`, through the matrix or IO_MUX.
    void signal_changed(int signal)
    {
        for (std::uint32_t pin = 0; pin < Esp32C3::gpio_count; ++pin) {
            if ((reg(0x554 + 4 * pin) & 0xFF) == static_cast<std::uint32_t>(signal)
                || direct_output(static_cast<int>(pin)) == signal) {
                evaluate(static_cast<int>(pin));
            }
        }
    }
    // The signal of IO_MUX function 2 of a pad: GP-SPI's pins (FSPIQ GPIO2,
    // FSPIHD 4, FSPIWP 5, FSPICLK 6, FSPID 7, FSPICS0 10), -1 for others.
    static int direct_signal(int pin)
    {
        switch (pin) {
        case 2: return 64;
        case 4: return 66;
        case 5: return 67;
        case 6: return 63;
        case 7: return 65;
        case 10: return 68;
        default: return -1;
        }
    }
    // IO_MUX's own functions: GP-SPI's pins (function 2) both ways, UART0's
    // TX on GPIO21 and RX on GPIO20 (function 0, U0TXD/U0RXD signal 6).
    static int direct_function(int pin) { return pin == 20 || pin == 21 ? 0 : 2; }
    static int direct_output(int pin) { return pin == 21 ? 6 : pin == 20 ? -1 : direct_signal(pin); }
    static int direct_input(int pin) { return pin == 20 ? 6 : pin == 21 ? -1 : direct_signal(pin); }


protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x3C) { // GPIO_IN
            std::uint32_t bits = 0;
            for (int pin = 0; pin < Esp32C3::gpio_count; ++pin) {
                if (states_[static_cast<std::size_t>(pin)].input && pads_[static_cast<std::size_t>(pin)]) {
                    bits |= 1u << pin;
                }
            }
            return bits;
        }
        if (offset == 0x5C) { // GPIO_PCPU_INT
            return reg(0x44) & enabled_mask();
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        switch (offset) {
        case 0x08: reg(0x04) |= value; break;   // OUT_W1TS
        case 0x0C: reg(0x04) &= ~value; break;  // OUT_W1TC
        case 0x24: reg(0x20) |= value; break;   // ENABLE_W1TS
        case 0x28: reg(0x20) &= ~value; break;  // ENABLE_W1TC
        case 0x48: reg(0x44) |= value; break;   // STATUS_W1TS
        case 0x4C:                              // STATUS_W1TC
            reg(0x44) &= ~value;
            relevel();
            break;
        case 0x38: return; // GPIO_STRAP is read-only
        default: Block::write_word(offset, value);
        }
        evaluate_all();
    }
    void on_reset() override
    {
        reg(0x38) = strapping_; // GPIO_STRAP_REG
        for (std::uint32_t pin = 0; pin < Esp32C3::gpio_count; ++pin) {
            reg(0x554 + 4 * pin) = 0x80; // FUNCn_OUT_SEL_CFG: simple GPIO output
        }
        states_.fill({});
        pads_.fill(false);
        primed_ = false;
    }

private:
    std::uint32_t enabled_mask() const
    {
        std::uint32_t mask = 0;
        for (std::uint32_t pin = 0; pin < Esp32C3::gpio_count; ++pin) {
            if (reg(0x74 + 4 * pin) & (1u << 13)) { // INT_ENA: the CPU
                mask |= 1u << pin;
            }
        }
        return mask;
    }
    void evaluate(int pin)
    {
        const std::uint32_t n = static_cast<std::uint32_t>(pin);
        const std::uint32_t iomux = peek(0x60009004u + 4 * n);
        const std::uint32_t out_sel = reg(0x554 + 4 * n);
        Esp32C3::PinState state;
        state.input = (iomux & (1u << 9)) != 0;
        state.pull_up = (iomux & (1u << 8)) != 0;
        state.pull_down = (iomux & (1u << 7)) != 0;
        state.open_drain = (reg(0x74 + 4 * n) & (1u << 2)) != 0;
        const bool gpio_function = ((iomux >> 12) & 7) == 1;
        const bool enabled = ((reg(0x20) >> n) & 1) != 0;
        if (gpio_function && (out_sel & 0xFF) == 128) {
            state.output = enabled != ((out_sel & (1u << 10)) != 0);
            state.level = (((reg(0x04) >> n) & 1) != 0) != ((out_sel & (1u << 8)) != 0);
        } else if (gpio_function && machine_.signal_driven_[out_sel & 0x7F] && (out_sel & 0xFF) < 128) {
            // A modelled peripheral's signal through the matrix: it drives the
            // pad (or GPIO_ENABLE does, with OEN_SEL).
            state.peripheral = true;
            state.output = (out_sel & (1u << 9)) ? enabled != ((out_sel & (1u << 10)) != 0) : true;
            state.level = machine_.signal_level_[out_sel & 0x7F] != ((out_sel & (1u << 8)) != 0);
        } else if (static_cast<int>((iomux >> 12) & 7) == direct_function(pin) && direct_output(pin) >= 0
            && machine_.signal_driven_[static_cast<std::size_t>(direct_output(pin))]) {
            // GP-SPI or UART0's TX through IO_MUX: the peripheral drives the pad.
            state.peripheral = true;
            state.output = true;
            state.level = machine_.signal_level_[static_cast<std::size_t>(direct_output(pin))];
        } else {
            // A peripheral's signal through the matrix (its own output enable
            // unless OEN_SEL picks GPIO_ENABLE), or another IO_MUX function.
            state.peripheral = !gpio_function || !(out_sel & (1u << 9)) || enabled;
        }
        std::optional<bool> external = external_[n];
        bool pad = state.pull_up && !state.pull_down;
        if (external) {
            pad = *external;
        }
        if (state.output && !(state.open_drain && state.level)) {
            pad = state.level;
        }
        Esp32C3::PinState& old = states_[n];
        const bool changed = !primed_ || old.output != state.output || old.level != state.level
            || old.open_drain != state.open_drain || old.peripheral != state.peripheral || old.input != state.input
            || old.pull_up != state.pull_up || old.pull_down != state.pull_down;
        const bool hook = primed_;
        if (changed) {
            old = state;
        }
        const bool before = pads_[n];
        pads_[n] = pad;
        if (before != pad) {
            pads_changed();
            const std::uint32_t type = (reg(0x74 + 4 * n) >> 7) & 7;
            if ((type == 1 && pad) || (type == 2 && !pad) || type == 3) {
                reg(0x44) |= 1u << n; // GPIO_STATUS
            }
        }
        if (pin == Esp32C3::gpio_count - 1) {
            primed_ = true;
        }
        relevel();
        // Last: a digital chip on the pin's net may answer at once (an I2C ACK)
        // and set the pad's level again through set_input().
        if (changed) {
            pin_changed(pin, state, hook);
        }
    }
    // Level-triggered pads keep their status bit while the level lasts.
    void relevel()
    {
        for (std::uint32_t n = 0; n < Esp32C3::gpio_count; ++n) {
            const std::uint32_t type = (reg(0x74 + 4 * n) >> 7) & 7;
            if ((type == 4 && !pads_[n]) || (type == 5 && pads_[n])) {
                reg(0x44) |= 1u << n;
            }
        }
        raise(16, (reg(0x44) & enabled_mask()) != 0);
    }
    void pin_changed(int pin, const Esp32C3::PinState& state, bool hook)
    {
        machine_.pin_events_.push_back({now_ns(), pin, state});
        if (machine_.pin_hook_ && hook) {
            machine_.pin_hook_(pin, now_ns());
        }
    }

    std::uint32_t strapping_;
    std::array<Esp32C3::PinState, Esp32C3::gpio_count> states_{};
    std::array<std::optional<bool>, Esp32C3::gpio_count> external_{};
    std::array<bool, Esp32C3::gpio_count> pads_{};
    bool primed_ = false;
};

void Esp32C3::set_signal(int signal, bool level)
{
    if (signal_level_[static_cast<std::size_t>(signal)] != level) {
        signal_level_[static_cast<std::size_t>(signal)] = level;
        gpio_->signal_changed(signal);
    }
}

bool Esp32C3::Block::input_signal(int signal) const
{
    return machine_.gpio_ && machine_.gpio_->input_signal(signal);
}

void Esp32C3::Block::pads_changed()
{
    for (Block* uart : machine_.uart_) {
        if (uart) {
            uart->pad_changed();
        }
    }
}

void Esp32C3::Block::refresh_pads()
{
    if (machine_.gpio_) {
        machine_.gpio_->evaluate_all();
    }
}

namespace {

void IoMux::write_word(std::uint32_t offset, std::uint32_t value)
{
    Block::write_word(offset, value);
    refresh_pads();
}

// RTC_CNTL: software resets (OPTIONS0: SW_SYS_RST bit 31, SW_PROCPU_RST
// bit 5), the reset reason, and the RTC watchdog (WDTCONFIG0-4, WDTFEED;
// stage timeouts in RTC slow clock ticks, 136 kHz; its interrupt is source 27).
// The super watchdog is taken as fed (ESP-IDF turns on its automatic feed).
class RtcCntl : public Esp32C3::Block, public Esp32C3::Timed {
public:
    explicit RtcCntl(Esp32C3& machine) : Block(machine, "RTC_CNTL", 0x800) {}

    std::uint64_t next_ns() const override { return wdt_.next_ns(); }
    void on_ns(std::uint64_t now) override
    {
        int stage = 0;
        const std::uint32_t action = wdt_.expire(now, &stage);
        if (action != 0) {
            note("RTC watchdog: stage " + std::to_string(stage) + " ran out ("
                + (action == 1 ? "interrupt" : action == 2 ? "CPU reset" : action == 3 ? "system reset" : "RTC reset") + ")");
        }
        switch (action) {
        case 1: // interrupt
            reg(0x44) |= 1u << 3; // INT_RAW: WDT
            update();
            break;
        case 2: request_reset(Esp32C3::ResetKind::Cpu, 0xD); break;        // RTCWDT_CPU_RST
        case 3: request_reset(Esp32C3::ResetKind::System, 0x9); break;     // RTCWDT_SYS_RST
        case 4: request_reset(Esp32C3::ResetKind::PowerOn, 0x10); break;   // RTCWDT_RTC_RST
        default: break;
        }
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x48) { // INT_ST
            return reg(0x44) & reg(0x40);
        }
        if (offset == 0x00C) { // TIME_UPDATE: TIME_VALID
            return Block::read_word(offset) | (1u << 30);
        }
        return Block::read_word(offset);
    }

public:
    // After a sleep that the RTC timer ended: the cause, and SLP_WAKEUP for
    // a light sleep (a deep sleep wakes through a reset).
    void woke(bool light)
    {
        reg(0x0F8) = 1u << 3; // SLP_WAKEUP_CAUSE: RTC_TIMER_TRIG_EN
        if (light) {
            reg(0x44) |= 1u; // INT_RAW: SLP_WAKEUP
            update();
        }
    }
    // WDTCONFIG0.PAUSE_IN_SLP: the RTC watchdog stops while the chip sleeps.
    bool watchdog_pauses_in_sleep() const { return (reg(0x090) & (1u << 9)) != 0; }
    void pause(std::uint64_t ns) override { wdt_.stage_start += ns; }

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        switch (offset) {
        case 0x000: // OPTIONS0
            if (value & (1u << 31)) {
                request_reset(Esp32C3::ResetKind::System);
            } else if (value & (1u << 5)) {
                request_reset(Esp32C3::ResetKind::Cpu);
            }
            Block::write_word(offset, value & ~((1u << 31) | (1u << 5)));
            break;
        case 0x0A4: // WDTFEED
            if (value & (1u << 31)) {
                wdt_.feed(now_ns());
            }
            break;
        case 0x00C: // TIME_UPDATE: latch the RTC timer into TIME_LOW0/HIGH0
            if (value & (1u << 31)) {
                const std::uint64_t ticks = rtc_ticks(now_ns());
                reg(0x010) = static_cast<std::uint32_t>(ticks);
                reg(0x014) = static_cast<std::uint32_t>(ticks >> 32) & 0xFFFF;
            }
            Block::write_word(offset, value & ~(1u << 31));
            break;
        case 0x018: // STATE0: SLEEP_EN starts sleeping
            Block::write_word(offset, value & ~(1u << 31));
            if (value & (1u << 31)) {
                const bool deep = (reg(0x088) & (1u << 31)) != 0; // DIG_PWC.DG_WRAP_PD_EN
                std::uint64_t wake = no_event;
                const std::uint32_t triggers = reg(0x03C) >> 15; // WAKEUP_STATE.WAKEUP_ENA
                if ((triggers & (1u << 3)) && (reg(0x008) & (1u << 16))) { // RTC_TIMER_TRIG_EN, MAIN_TIMER_ALARM_EN
                    const std::uint64_t target = (std::uint64_t{reg(0x008) & 0xFFFF} << 32) | reg(0x004);
                    wake = std::max(now_ns(), ns_at_tick(target));
                }
                enter_sleep(deep, wake);
            }
            break;
        case 0x090: case 0x094: case 0x098: case 0x09C: case 0x0A0: // WDTCONFIG0-4
            Block::write_word(offset, value);
            configure();
            break;
        case 0x04C: // INT_CLR
            reg(0x44) &= ~value;
            update();
            break;
        case 0x100: reg(0x40) |= value; update(); break;  // INT_ENA_W1TS
        case 0x104: reg(0x40) &= ~value; update(); break; // INT_ENA_W1TC
        case 0x040: Block::write_word(offset, value); update(); break;
        default:
            Block::write_word(offset, value);
        }
        reschedule();
    }
    void on_reset() override
    {
        reg(0x38) = 1; // RTC_CNTL_RESET_STATE_REG: power-on reset
        wdt_ = Wdt{};
    }

private:
    void configure()
    {
        const std::uint32_t config0 = reg(0x090);
        const bool was = wdt_.enabled;
        wdt_.enabled = (config0 & (1u << 31)) != 0;
        for (int stage = 0; stage < 4; ++stage) {
            wdt_.actions[stage] = (config0 >> (28 - 3 * stage)) & 7;
            wdt_.timeouts[stage] = reg(0x094 + 4 * static_cast<std::uint32_t>(stage));
        }
        wdt_.tick_ps = 1'000'000'000'000ull / 136'000; // RC_SLOW, nominal
        if (wdt_.enabled && !was) {
            wdt_.feed(now_ns());
        }
    }
    void update() { raise(27, (reg(0x44) & reg(0x40)) != 0); }
    // The RTC timer counts the slow clock (RC_SLOW, nominally 136 kHz).
    static std::uint64_t rtc_ticks(std::uint64_t ns)
    {
        return ns / 1'000'000'000ull * 136'000 + ns % 1'000'000'000ull * 136'000 / 1'000'000'000ull;
    }
    static std::uint64_t ns_at_tick(std::uint64_t ticks)
    {
        return ticks / 136'000 * 1'000'000'000ull + (ticks % 136'000 * 1'000'000'000ull + 135'999) / 136'000;
    }

    Wdt wdt_;
};

// SPI0 and SPI1 memory controllers (TRM chapter "SPI0 and SPI1"). SPI1 runs
// the firmware's flash commands: the fixed ones of SPI_MEM_CMD_REG and user
// transactions (SPI_MEM_USR) built from USER, USER1, USER2, the data lengths
// and the W0-W15 buffer. SPI0 serves the cache; its commands only complete.
class SpiMem : public Esp32C3::Block {
public:
    SpiMem(Esp32C3& machine, const char* name, bool flash_commands)
        : Block(machine, name), flash_commands_(flash_commands)
    {
    }

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset != 0x00) { // SPI_MEM_CMD_REG
            Block::write_word(offset, value);
            return;
        }
        if (flash_commands_) {
            execute(value);
        }
        Block::write_word(offset, 0); // every command bit clears when done
    }

private:
    static constexpr std::uint32_t addr_reg = 0x04, user_reg = 0x18, user1_reg = 0x1C, user2_reg = 0x20,
                                   mosi_dlen = 0x24, miso_dlen = 0x28, rd_status = 0x2C, w0 = 0x58;

    void buffer_out(std::uint8_t* bytes, std::size_t count)
    {
        for (std::size_t i = 0; i < count && i < 64; ++i) {
            bytes[i] = static_cast<std::uint8_t>(reg(w0 + static_cast<std::uint32_t>(i & ~3u)) >> (8 * (i & 3)));
        }
    }
    void buffer_in(const std::uint8_t* bytes, std::size_t count)
    {
        for (std::size_t i = 0; i < count && i < 64; ++i) {
            std::uint32_t& word = reg(w0 + static_cast<std::uint32_t>(i & ~3u));
            const std::uint32_t shift = 8 * (i & 3);
            word = (word & ~(0xFFu << shift)) | (static_cast<std::uint32_t>(bytes[i]) << shift);
        }
    }
    void execute(std::uint32_t cmd)
    {
        Esp32C3::FlashChip& flash = flash_chip();
        std::uint8_t data[64] = {};
        std::uint8_t none[1] = {};
        const std::uint32_t address = reg(addr_reg) & 0xFFFFFF;
        const std::size_t length = std::min<std::size_t>(64, reg(addr_reg) >> 24);
        if (cmd & (1u << 18)) { // SPI_MEM_USR: a user transaction
            const std::uint32_t user = reg(user_reg);
            const bool has_command = (user & (1u << 31)) != 0;
            const bool has_address = (user & (1u << 30)) != 0;
            const unsigned address_bits = (reg(user1_reg) >> 26) + 1;
            const std::uint32_t user_address = address_bits >= 32 ? reg(addr_reg) : reg(addr_reg) & ((1u << address_bits) - 1);
            const std::size_t mosi = (user & (1u << 27)) ? std::min<std::size_t>(64, ((reg(mosi_dlen) & 0x3FF) + 8) / 8) : 0;
            const std::size_t miso = (user & (1u << 28)) ? std::min<std::size_t>(64, ((reg(miso_dlen) & 0x3FF) + 8) / 8) : 0;
            // The ROM's esp_rom_spiflash_read (esptool's flasher stub reads
            // with it) leaves USR_COMMAND off with the read command in USER2;
            // on the chip the read works, so a transaction with an address
            // sends USER2's command too.
            if (!has_command && !has_address) {
                return;
            }
            buffer_out(data, mosi);
            std::uint8_t in[64] = {};
            flash.command(static_cast<std::uint8_t>(reg(user2_reg) & 0xFF), has_address, user_address, data, mosi, in, miso);
            buffer_in(in, miso);
            return;
        }
        if (cmd & (1u << 31)) { // FLASH_READ
            const std::size_t count = length ? length : std::min<std::size_t>(64, ((reg(miso_dlen) & 0x3FF) + 8) / 8);
            flash.command(0x03, true, address, none, 0, data, count);
            buffer_in(data, count);
        }
        if (cmd & (1u << 30)) { // FLASH_WREN
            flash.set_write_enable(true);
        }
        if (cmd & (1u << 29)) { // FLASH_WRDI
            flash.set_write_enable(false);
        }
        if (cmd & (1u << 28)) { // FLASH_RDID
            flash.command(0x9F, false, 0, none, 0, data, 3);
            buffer_in(data, 3);
        }
        if (cmd & (1u << 27)) { // FLASH_RDSR
            reg(rd_status) = (reg(rd_status) & ~0xFFu) | flash.status_low();
        }
        if (cmd & (1u << 26)) { // FLASH_WRSR
            flash.write_status(static_cast<std::uint16_t>(reg(rd_status)));
        }
        if (cmd & (1u << 25)) { // FLASH_PP: the length in ADDR[31:24]
            buffer_out(data, length);
            flash.command(0x02, true, address, data, length, none, 0);
        }
        if (cmd & (1u << 24)) { // FLASH_SE
            flash.command(0x20, true, address, none, 0, none, 0);
        }
        if (cmd & (1u << 23)) { // FLASH_BE
            flash.command(0xD8, true, address, none, 0, none, 0);
        }
        if (cmd & (1u << 22)) { // FLASH_CE
            flash.command(0x60, false, 0, none, 0, none, 0);
        }
    }

    bool flash_commands_;
};

// EXTMEM: cache control. Every requested operation (invalidate, preload,
// lock, autoload, freeze) is done at once; an invalidation makes the cache
// windows read the flash again.
class Extmem : public Esp32C3::Block {
public:
    explicit Extmem(Esp32C3& machine) : Block(machine, "EXTMEM") {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        switch (offset) {
        case 0x01C: // ICACHE_LOCK_CTRL: LOCK_ENA / UNLOCK_ENA -> LOCK_DONE
            Block::write_word(offset, (value & 3) ? (value & ~3u) | 4 : value);
            return;
        case 0x028: // ICACHE_SYNC_CTRL: INVALIDATE_ENA -> SYNC_DONE
            if (value & 1) {
                invalidate_cache();
            }
            Block::write_word(offset, (value & ~1u) | 2);
            return;
        case 0x034: // ICACHE_PRELOAD_CTRL: PRELOAD_ENA -> PRELOAD_DONE
            Block::write_word(offset, (value & 1) ? (value & ~1u) | 2 : value);
            return;
        case 0x040: // ICACHE_AUTOLOAD_CTRL: AUTOLOAD_ENA -> AUTOLOAD_DONE
            Block::write_word(offset, (value & 4) ? value | 8 : value);
            return;
        case 0x0CC: // ICACHE_FREEZE: FREEZE_ENA -> FREEZE_DONE
            Block::write_word(offset, (value & 1) ? value | 4 : value & ~4u);
            return;
        default:
            Block::write_word(offset, value);
        }
    }
    void on_reset() override
    {
        reg(0x004) = 3;     // ICACHE_CTRL1: both buses shut
        reg(0x01C) = 4;     // lock done
        reg(0x028) = 2;     // sync done
        reg(0x034) = 2;     // preload done
        reg(0x040) = 8;     // autoload done
        reg(0x0B0) = 1;     // CACHE_STATE: idle
    }
};

// eFuse controller: the values of a chip revision v0.4 with eFuse block
// revision v1.3, an Espressif MAC address 34:85:18:00:C3:01 and otherwise
// blank fuses (no flash encryption, no secure boot, USB-Serial-JTAG enabled).
// Read and program commands finish at once; programming changes nothing yet.
class Efuse : public Esp32C3::Block {
public:
    explicit Efuse(Esp32C3& machine) : Block(machine, "EFUSE", 0x800) {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        Block::write_word(offset, offset == 0x1D4 ? value & ~3u : value); // EFUSE_CMD_REG: READ_CMD, PGM_CMD
    }
    void on_reset() override
    {
        reg(0x044) = 0x1800C301;                 // RD_MAC_SPI_SYS_0: MAC bytes 5..2 (01 C3 00 18)
        reg(0x048) = 0x3485;                     // RD_MAC_SPI_SYS_1: MAC bytes 1..0 (85 34)
        reg(0x050) = (4u << 18) | (3u << 24);    // RD_MAC_SPI_SYS_3: WAFER_VERSION_MINOR_LO 4, BLK_VERSION_MINOR 3
        reg(0x058) = 0;                          // RD_MAC_SPI_SYS_5: WAFER_VERSION_MINOR_HI 0, MAJOR 0
        reg(0x06C) = 1;                          // RD_SYS_PART1_DATA4: BLK_VERSION_MAJOR 1
        reg(0x1FC) = 0x02007200;                 // EFUSE_DATE_REG reset value
    }
};

// Timer group: RTC slow clock calibration (RTCCALICFG). A started
// calibration finishes at once with the count of 40 MHz crystal cycles in
// RTC_CALI_MAX slow cycles, from the nominal frequency of the chosen
// oscillator: RC_SLOW 136 kHz, RC_FAST/256 68.36 kHz, or a 32.768 kHz crystal.
// The watchdog and timer registers only keep their values for now.
class Timg : public Esp32C3::Block, public Esp32C3::Timed {
public:
    Timg(Esp32C3& machine, const char* name, int group) : Block(machine, name), group_(group) {}

    std::uint64_t next_ns() const override { return wdt_.next_ns(); }
    void pause(std::uint64_t ns) override { wdt_.stage_start += ns; }
    void on_ns(std::uint64_t now) override
    {
        int stage = 0;
        const std::uint32_t action = wdt_.expire(now, &stage);
        if (action != 0) {
            note(std::string(name()) + " watchdog: stage " + std::to_string(stage) + " ran out ("
                + (action == 1 ? "interrupt" : action == 2 ? "CPU reset" : "system reset") + ")");
        }
        switch (action) {
        case 1: // interrupt
            reg(0x74) |= 2; // INT_RAW_TIMERS: WDT
            update();
            break;
        case 2: request_reset(Esp32C3::ResetKind::Cpu, group_ == 0 ? 0xB : 0x11); break;    // TGnWDT_CPU_RST
        case 3: request_reset(Esp32C3::ResetKind::System, group_ == 0 ? 0x7 : 0x8); break;  // TGnWDT_SYS_RST
        default: break;
        }
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x78) { // INT_ST_TIMERS
            return reg(0x74) & reg(0x70);
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset == 0x68 && (value & (1u << 31))) { // TIMG_RTCCALICFG_REG: RTC_CALI_START
            const std::uint32_t max = (value >> 16) & 0x7FFF;
            std::uint32_t source = (value >> 13) & 3;
            if (source == 0) { // RTC_CAL_RTC_MUX: the slow clock RTC_CNTL selects
                const std::uint32_t rtc = (peek(0x60008070u) >> 30) & 3; // RTC_CNTL_ANA_CLK_RTC_SEL
                source = rtc == 1 ? 2 : rtc == 2 ? 1 : 0;
            }
            const double hz = source == 1 ? 17'500'000.0 / 256 : source == 2 ? 32'768.0 : 136'000.0;
            const std::uint64_t xtal = static_cast<std::uint64_t>(max * 40'000'000.0 / hz + 0.5);
            Block::write_word(offset, value | (1u << 15)); // RTC_CALI_RDY
            reg(0x6C) = (static_cast<std::uint32_t>(xtal) << 7) | 1; // RTCCALICFG1: value, CYCLING_DATA_VLD
            reg(0x80) &= ~1u;                                          // RTCCALICFG2: no timeout
            return;
        }
        switch (offset) {
        case 0x60: // WDTFEED
            wdt_.feed(now_ns());
            break;
        case 0x7C: // INT_CLR_TIMERS
            reg(0x74) &= ~value;
            update();
            break;
        case 0x48: case 0x4C: case 0x50: case 0x54: case 0x58: case 0x5C: // WDTCONFIG0-5
            Block::write_word(offset, offset == 0x48 ? value & ~(1u << 22) : value); // CONF_UPDATE_EN clears
            configure();
            break;
        case 0x70: // INT_ENA_TIMERS
            Block::write_word(offset, value);
            update();
            break;
        default:
            Block::write_word(offset, offset == 0x68 ? value & ~(1u << 15) : value);
        }
        reschedule();
    }

private:
    void on_reset() override { wdt_ = Wdt{}; }
    void configure()
    {
        const std::uint32_t config0 = reg(0x48);
        const bool was = wdt_.enabled;
        wdt_.enabled = (config0 & (1u << 31)) != 0;
        for (int stage = 0; stage < 4; ++stage) {
            wdt_.actions[stage] = (config0 >> (29 - 2 * stage)) & 3;
            wdt_.timeouts[stage] = reg(0x50 + 4 * static_cast<std::uint32_t>(stage));
        }
        // The clock: APB (80 MHz once the CPU runs from the PLL) or the crystal.
        const std::uint64_t clock = (config0 & (1u << 21)) ? 40'000'000ull
            : std::min<std::uint64_t>(80'000'000ull, machine_.cpu_hz());
        const std::uint64_t prescale = std::max<std::uint32_t>(reg(0x4C) >> 16, 1);
        wdt_.tick_ps = prescale * 1'000'000'000'000ull / clock;
        if (wdt_.enabled && !was) {
            wdt_.feed(now_ns());
        }
    }
    void update() { raise(group_ == 0 ? 33 : 35, (reg(0x74) & reg(0x70) & 2) != 0); }

    int group_;
    Wdt wdt_;
};

// SYSCON (APB_CTRL): RNG_DATA_REG gives a deterministic pseudo-random
// sequence (xorshift32 from a fixed seed), so that two runs are identical;
// the chip's generator mixes physical noise.
class Syscon : public Esp32C3::Block {
public:
    explicit Syscon(Esp32C3& machine) : Block(machine, "SYSCON") {}

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x0B0) { // RNG_DATA_REG
            state_ ^= state_ << 13;
            state_ ^= state_ >> 17;
            state_ ^= state_ << 5;
            return state_;
        }
        return Block::read_word(offset);
    }
    void on_reset() override { state_ = 0x2545F491u; }

private:
    std::uint32_t state_ = 0x2545F491u;
};

// SYSTEM: the CPU clock follows SYSCLK_CONF (source and crystal divider) and
// CPU_PER_CONF (80 or 160 MHz from the PLL); FROM_CPU interrupts; the RTC
// memory CRC before deep sleep finishes at once without computing the CRC
// (so the ROM never runs a deep-sleep wake stub).
class System : public Esp32C3::Block {
public:
    explicit System(Esp32C3& machine) : Block(machine, "SYSTEM") {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset == 0x048 && (value & (1u << 8))) { // RTC_FASTMEM_CONFIG: RTC_MEM_CRC_START
            value |= 1u << 31; // RTC_MEM_CRC_FINISH at once; the CRC is not computed
        }
        Block::write_word(offset, value);
        if (offset >= 0x028 && offset <= 0x034) { // SYSTEM_CPU_INTR_FROM_CPU_n: sources 50..53
            raise(50 + static_cast<int>((offset - 0x028) / 4), (value & 1) != 0);
        }
        if (offset == 0x008 || offset == 0x058) { // SYSTEM_CPU_PER_CONF_REG, SYSTEM_SYSCLK_CONF_REG
            const std::uint32_t source = (reg(0x058) >> 10) & 3;
            const std::uint32_t divider = (reg(0x058) & 0x3FF) + 1;
            std::uint32_t hz = 40'000'000 / divider;
            if (source == 1) {
                hz = (reg(0x008) & 3) == 1 ? 160'000'000 : 80'000'000;
            } else if (source == 2) {
                hz = 17'500'000 / divider; // the RC_FAST oscillator, nominally 17.5 MHz
            }
            set_cpu_hz(hz);
        }
    }
    void on_reset() override
    {
        reg(0x008) = 0x0C; // CPU_PER_CONF reset value (TRM): PLL 480 MHz
        reg(0x058) = 0x01; // SYSCLK_CONF: XTAL, PRE_DIV_CNT 1 per the reset value
    }
};

// SHA accelerator, typical (CPU) mode: SHA_START hashes the 64-byte block in
// the text memory from the mode's initial value, SHA_CONTINUE from the hash
// memory; both finish at once. The hash and text memories hold bytes in
// message order (each 32-bit word is big-endian for the algorithm).
class Sha : public Esp32C3::Block {
public:
    explicit Sha(Esp32C3& machine) : Block(machine, "SHA") {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        Block::write_word(offset, value);
        if (offset != 0x10 && offset != 0x14) { // SHA_START_REG, SHA_CONTINUE_REG
            return;
        }
        const std::uint32_t mode = reg(0x00) & 7;
        const unsigned words = mode == 0 ? 5 : 8;
        ShaState state{};
        if (offset == 0x10) {
            state = mode == 0 ? sha1_initial() : mode == 1 ? sha224_initial() : sha256_initial();
        } else {
            for (unsigned i = 0; i < words; ++i) {
                state[i] = swap(reg(0x40 + 4 * i));
            }
        }
        std::uint8_t block[64];
        for (std::uint32_t i = 0; i < 16; ++i) {
            const std::uint32_t word = reg(0x80 + 4 * i);
            for (std::uint32_t b = 0; b < 4; ++b) {
                block[4 * i + b] = static_cast<std::uint8_t>(word >> (8 * b));
            }
        }
        if (mode == 0) {
            sha1_compress(state, block);
        } else {
            sha256_compress(state, block);
        }
        for (unsigned i = 0; i < 8; ++i) {
            reg(0x40 + 4 * i) = i < words ? swap(state[i]) : 0;
        }
    }

private:
    static std::uint32_t swap(std::uint32_t v)
    {
        return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
    }
};

// SYSTIMER: two 52-bit units counting at 16 MHz while enabled (CONF bits 30
// and 29), read by UNITn_OP.UPDATE into UNITn_VALUE, set by UNITn_LOAD; three
// comparators enabled by CONF bits 24..22. A comparator's unit, period mode
// and period act as TARGETn_CONF holds them; COMPn_LOAD arms it with the
// target value (TARGETn_HI/LO) and starts a period from the present count, as
// turning period mode on does. Alarms set INT_RAW and, through INT_ENA, raise
// sources 37..39.
class Systimer : public Esp32C3::Block, public Esp32C3::Timed {
public:
    explicit Systimer(Esp32C3& machine) : Block(machine, "SYSTIMER") {}

    std::uint64_t next_ns() const override
    {
        std::uint64_t next = no_event;
        for (std::uint32_t t = 0; t < 3; ++t) {
            if (!active(t)) {
                continue;
            }
            const Unit& unit = units_[unit_of(t)];
            const std::uint64_t due = alarm_value(t);
            if (unit.value_at(now_ns()) >= due) {
                return now_ns();
            }
            // Smallest time at which value_at() reaches `due`.
            next = std::min(next, unit.base_ns + ((due - unit.base_value) * 125 + 1) / 2);
        }
        return next;
    }
    void pause(std::uint64_t ns) override
    {
        for (Unit& unit : units_) {
            unit.base_ns += ns; // the counters stood still
        }
    }
    void on_ns(std::uint64_t now) override
    {
        for (std::uint32_t t = 0; t < 3; ++t) {
            if (!active(t) || units_[unit_of(t)].value_at(now) < alarm_value(t)) {
                continue;
            }
            Block::reg(0x68) |= 1u << t; // INT_RAW
            if (period_mode(t) && period(t) != 0) {
                targets_[t].period_next += period(t);
            } else {
                targets_[t].armed = false;
            }
        }
        update_sources();
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        switch (offset) {
        case 0x04: case 0x08: return Block::read_word(offset) | (1u << 29); // UNITn_OP: VALUE_VALID
        case 0x70: return reg(0x68) & reg(0x64);                           // INT_ST
        default: return Block::read_word(offset);
        }
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        const std::uint64_t now = now_ns();
        switch (offset) {
        case 0x00: // CONF: units start and stop counting
            for (std::uint32_t u = 0; u < 2; ++u) {
                Unit& unit = units_[u];
                const bool enabled = (value & (1u << (30 - u))) != 0;
                if (enabled != unit.enabled) {
                    unit.base_value = unit.value_at(now);
                    unit.base_ns = now;
                    unit.enabled = enabled;
                }
            }
            Block::write_word(offset, value);
            break;
        case 0x04: case 0x08: { // UNITn_OP: UPDATE latches the value
            const std::uint32_t u = offset == 0x04 ? 0 : 1;
            if (value & (1u << 30)) {
                const std::uint64_t v = units_[u].value_at(now);
                reg(0x40 + 8 * u) = static_cast<std::uint32_t>(v >> 32) & 0xFFFFF;
                reg(0x44 + 8 * u) = static_cast<std::uint32_t>(v);
            }
            Block::write_word(offset, value & ~(1u << 30));
            break;
        }
        case 0x34: case 0x38: case 0x3C: { // TARGETn_CONF
            const std::uint32_t t = (offset - 0x34) / 4;
            const bool was_periodic = period_mode(t);
            Block::write_word(offset, value);
            if (period_mode(t) && !was_periodic) {
                targets_[t].period_next = units_[unit_of(t)].value_at(now) + period(t);
            }
            break;
        }
        case 0x5C: case 0x60: { // UNITn_LOAD
            const std::uint32_t u = offset == 0x5C ? 0 : 1;
            if (value & 1) {
                Unit& unit = units_[u];
                unit.base_value = (std::uint64_t{reg(0x0C + 8 * u) & 0xFFFFF} << 32) | reg(0x10 + 8 * u);
                unit.base_ns = now;
            }
            break;
        }
        case 0x50: case 0x54: case 0x58: { // COMPn_LOAD: arm comparator n
            const std::uint32_t t = (offset - 0x50) / 4;
            if (value & 1) {
                Target& target = targets_[t];
                target.armed = true;
                target.value = (std::uint64_t{reg(0x1C + 8 * t) & 0xFFFFF} << 32) | reg(0x20 + 8 * t);
                target.period_next = units_[unit_of(t)].value_at(now) + period(t);
            }
            break;
        }
        case 0x64: // INT_ENA
            Block::write_word(offset, value & 7);
            update_sources();
            break;
        case 0x6C: // INT_CLR
            reg(0x68) &= ~value;
            update_sources();
            break;
        default:
            Block::write_word(offset, value);
        }
        reschedule();
    }
    void on_reset() override
    {
        reg(0x00) = 0x46000000; // CONF reset value: unit 0 counts
        units_[0] = {true, 0, 0};
        units_[1] = {false, 0, 0};
        for (Target& target : targets_) {
            target = Target{};
        }
    }

private:
    struct Unit {
        bool enabled = false;
        std::uint64_t base_value = 0;
        std::uint64_t base_ns = 0;
        std::uint64_t value_at(std::uint64_t ns) const
        {
            const std::uint64_t ticks = enabled && ns > base_ns ? (ns - base_ns) * 2 / 125 : 0; // 16 per microsecond
            return (base_value + ticks) & ((std::uint64_t{1} << 52) - 1);
        }
    };
    struct Target {
        bool armed = false;
        std::uint64_t value = 0;       // one-shot target
        std::uint64_t period_next = 0; // next alarm in period mode
    };
    std::uint32_t conf(std::uint32_t t) const { return reg(0x34 + 4 * t); }
    bool period_mode(std::uint32_t t) const { return (conf(t) & (1u << 30)) != 0; }
    std::uint32_t period(std::uint32_t t) const { return conf(t) & 0x3FFFFFF; }
    std::uint32_t unit_of(std::uint32_t t) const { return conf(t) >> 31; }
    std::uint64_t alarm_value(std::uint32_t t) const
    {
        return period_mode(t) ? targets_[t].period_next : targets_[t].value;
    }
    bool active(std::uint32_t t) const
    {
        return targets_[t].armed && (reg(0x00) & (1u << (24 - t))) && units_[unit_of(t)].enabled;
    }
    void update_sources()
    {
        const std::uint32_t status = reg(0x68) & reg(0x64);
        for (std::uint32_t t = 0; t < 3; ++t) {
            raise(37 + static_cast<int>(t), (status >> t) & 1);
        }
    }

    Unit units_[2];
    Target targets_[3];
};

// LED PWM controller (TRM chapter "LED PWM Controller"): four timers counting
// to 2^DUTY_RES at the PWM clock (LEDC_CONF.APB_CLK_SEL: 80 MHz APB, 17.5 MHz
// RC_FAST or the 40 MHz crystal) divided by CLK_DIV (10.8 fixed point); six
// channels, each high while its timer's count is in [HPOINT, HPOINT + DUTY)
// (DUTY has four fraction bits), driving GPIO matrix signals 45..50 while
// SIG_OUT_EN is set (otherwise IDLE_LV). New values take effect with
// PARA_UP / LOW_SPEED_UPDATE or DUTY_START; fades go straight to their start
// duty (not modelled yet). Edges come at their exact times.
class Ledc : public Esp32C3::Block, public Esp32C3::Timed {
public:
    explicit Ledc(Esp32C3& machine) : Block(machine, "LEDC")
    {
        for (int ch = 0; ch < 6; ++ch) {
            claim_signal(45 + ch);
        }
    }

    std::uint64_t next_ns() const override { return next_; }
    void on_ns(std::uint64_t now) override { refresh(now); }

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        const std::uint64_t now = now_ns();
        if (offset < 0x78) { // channel registers
            const std::uint32_t ch = offset / 0x14;
            const std::uint32_t reg_in_ch = offset % 0x14;
            Block::write_word(offset, reg_in_ch == 0x0C ? value & ~(1u << 31) : value & ~(1u << 4));
            const bool update = (reg_in_ch == 0x00 && (value & (1u << 4))) || (reg_in_ch == 0x0C && (value & (1u << 31)));
            if (update || reg_in_ch == 0x00) {
                Channel& c = channels_[ch];
                const std::uint32_t conf0 = reg(0x14 * ch);
                c.timer = conf0 & 3;
                c.enabled = (conf0 & (1u << 2)) != 0;
                c.idle = (conf0 & (1u << 3)) != 0;
                if (update) {
                    c.hpoint = reg(0x14 * ch + 0x04) & 0x3FFF;
                    c.duty16 = reg(0x14 * ch + 0x08) & 0x7FFFF;
                    reg(0x14 * ch + 0x10) = c.duty16; // DUTY_R
                }
            }
        } else if (offset >= 0xA0 && offset < 0xC0 && (offset & 4) == 0) { // LSTIMERn_CONF
            const std::uint32_t t = (offset - 0xA0) / 8;
            Timer& timer = timers_[t];
            const bool was_held = timer.held;
            Block::write_word(offset, value & ~(1u << 25));
            timer.held = (value & (1u << 23)) || (value & (1u << 22)); // RST or PAUSE
            if (value & (1u << 25) || !timer.configured) { // PARA_UP
                timer.resolution = value & 0xF;
                timer.divider = (value >> 4) & 0x3FFFF;
                timer.configured = true;
            }
            if (was_held && !timer.held) {
                timer.epoch = now; // the count starts from 0
            }
        } else {
            Block::write_word(offset, offset == 0xCC ? 0 : value); // INT_CLR
        }
        refresh(now);
        reschedule();
    }
    void on_reset() override
    {
        for (Channel& c : channels_) {
            c = Channel{};
        }
        for (Timer& t : timers_) {
            t = Timer{};
        }
        next_ = no_event;
    }

private:
    struct Channel {
        std::uint32_t timer = 0;
        bool enabled = false;
        bool idle = false;
        std::uint32_t hpoint = 0;
        std::uint32_t duty16 = 0; // duty in 1/16 of a count
    };
    struct Timer {
        bool configured = false;
        bool held = true;
        std::uint32_t resolution = 0;
        std::uint32_t divider = 0; // 10.8 fixed point
        std::uint64_t epoch = 0;
    };
    double clock_hz() const
    {
        switch (reg(0xD0) & 3) { // LEDC_CONF.APB_CLK_SEL
        case 2: return 17'500'000.0;
        case 3: return 40'000'000.0;
        default: return 80'000'000.0;
        }
    }
    // Level of channel `ch` at `now` and the time of its next edge.
    bool level_at(const Channel& c, std::uint64_t now, std::uint64_t& next) const
    {
        next = no_event;
        if (!c.enabled) {
            return c.idle;
        }
        const Timer& t = timers_[c.timer];
        if (!t.configured || t.held || t.divider < 256 || t.resolution == 0) {
            return false;
        }
        const double tick = static_cast<double>(t.divider) / 256.0 / clock_hz() * 1e9; // ns per count
        const double counts = static_cast<double>(1u << t.resolution);
        const double period = counts * tick;
        const double duty = static_cast<double>(c.duty16) / 16.0;
        if (duty <= 0) {
            return false;
        }
        if (duty >= counts) {
            return true;
        }
        const double rise = static_cast<double>(c.hpoint) * tick;
        const double fall = rise + duty * tick;
        const double elapsed = static_cast<double>(now - t.epoch);
        const double cycle = std::floor(elapsed / period);
        const double pos = elapsed - cycle * period;
        bool high = (pos >= rise && pos < fall) || (fall > period && pos < fall - period);
        // The next edge: the first of rise and fall (mod period) after pos.
        double best = 1e300;
        for (double edge : {rise, std::fmod(fall, period)}) {
            double d = edge - pos;
            if (d <= 0) {
                d += period;
            }
            best = std::min(best, d);
        }
        next = now + std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::ceil(best)));
        return high;
    }
    void refresh(std::uint64_t now)
    {
        next_ = no_event;
        for (int ch = 0; ch < 6; ++ch) {
            std::uint64_t next = no_event;
            const bool level = level_at(channels_[ch], now, next);
            drive_signal(45 + ch, level);
            next_ = std::min(next_, next);
        }
    }

    Channel channels_[6];
    Timer timers_[4];
    std::uint64_t next_ = no_event;
};

// I2C0 as a master (TRM chapter "I2C Controller"): CTR.TRANS_START runs the
// command list COMD0..7 (byte count 7:0, ACK_EN 8, ACK_EXP 9, ACK_VAL 10,
// opcode 13:11: 6 RSTART, 1 WRITE, 3 READ, 2 STOP, 4 END; DONE bit 31) on
// GPIO matrix signals 53 (SCL) and 54 (SDA), released for a 1 like an open
// drain, with bytes through the 32-byte FIFOs at DATA. Timing from CLK_CONF
// (XTAL or RC_FAST, divider) and the SCL/SDA period registers. END pauses
// with END_DETECT until the next TRANS_START; STOP gives TRANS_COMPLETE; an
// unexpected ACK gives NACK and a STOP. A slave that holds SCL low stretches
// the clock until the TO timeout (TIME_OUT). Lines are read back through the
// input matrix. Not modelled: slave mode, arbitration, the FIFO RAM view.
class I2c : public Esp32C3::Block, public Esp32C3::Timed {
public:
    explicit I2c(Esp32C3& machine) : Block(machine, "I2C0")
    {
        claim_signal(scl_signal);
        claim_signal(sda_signal);
    }

    std::uint64_t next_ns() const override { return steps_.empty() ? no_event : steps_.front().at; }
    void on_ns(std::uint64_t now) override
    {
        while (!steps_.empty() && steps_.front().at <= now) {
            const Step step = steps_.front();
            steps_.pop_front();
            perform(step, now);
        }
        if (steps_.empty() && running_) {
            next_command(now);
        }
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        switch (offset) {
        case 0x08: { // SR
            std::uint32_t sr = (ack_bit_ ? 1u : 0u) | (busy_ ? 1u << 4 : 0u);
            sr |= static_cast<std::uint32_t>(rx_.size()) << 8;
            sr |= static_cast<std::uint32_t>(tx_.size()) << 18;
            return sr;
        }
        case 0x1C: { // DATA: the RX FIFO
            if (rx_.empty()) {
                interrupt(1u << 12); // RXFIFO_UDF
                return 0;
            }
            const std::uint8_t byte = rx_.front();
            rx_.pop_front();
            return byte;
        }
        case 0x2C: return reg(0x20) & reg(0x28); // INT_STATUS
        default: return Block::read_word(offset);
        }
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        const std::uint64_t now = now_ns();
        switch (offset) {
        case 0x04: // CTR: TRANS_START (5), FSM_RST (10), CONF_UPGATE (11) clear themselves
            Block::write_word(offset, value & ~((1u << 5) | (1u << 10) | (1u << 11)));
            if (value & (1u << 10)) {
                abort_transfer(now);
            }
            if ((value & (1u << 11)) && (reg(0x80) & 1)) {
                reg(0x80) &= ~1u; // SCL_RST_SLV_EN: the bus-clear pulses are done at once
            }
            if (value & (1u << 5)) {
                start(now);
            }
            break;
        case 0x18: // FIFO_CONF: RX_FIFO_RST (12), TX_FIFO_RST (13)
            Block::write_word(offset, value);
            if (value & (1u << 12)) {
                rx_.clear();
            }
            if (value & (1u << 13)) {
                tx_.clear();
            }
            break;
        case 0x1C: // DATA: the TX FIFO
            if (tx_.size() < fifo_size) {
                tx_.push_back(static_cast<std::uint8_t>(value));
            } else {
                interrupt(1u << 11); // TXFIFO_OVF
            }
            break;
        case 0x24: // INT_CLR
            reg(0x20) &= ~value;
            update_interrupt();
            break;
        case 0x28: // INT_ENA
            Block::write_word(offset, value);
            update_interrupt();
            break;
        default: Block::write_word(offset, value);
        }
        reschedule();
    }
    void on_reset() override
    {
        steps_.clear();
        tx_.clear();
        rx_.clear();
        running_ = busy_ = ack_bit_ = false;
        command_ = 0;
        scl_ = sda_ = true;
        drive_signal(scl_signal, true);
        drive_signal(sda_signal, true);
    }

private:
    static constexpr int scl_signal = 53;
    static constexpr int sda_signal = 54;
    static constexpr std::size_t fifo_size = 32;
    enum class Action { Scl, Sda, SampleBit, SampleAck, ByteDone, CommandDone, Stopped };
    struct Step {
        std::uint64_t at = 0;
        Action action = Action::Scl;
        bool level = true;
    };

    // One source clock period in ns (XTAL 40 MHz or RC_FAST 17.5 MHz, divided).
    double sclk_ns() const
    {
        const std::uint32_t conf = reg(0x54);
        const double hz = (conf & (1u << 20)) ? 17'500'000.0 : 40'000'000.0;
        return 1e9 / hz * static_cast<double>((conf & 0xFF) + 1);
    }
    std::uint64_t ticks(std::uint32_t count) const
    {
        return std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::llround(count * sclk_ns())));
    }
    std::uint64_t low_ns() const { return ticks((reg(0x00) & 0x1FF) + 1); }
    std::uint64_t high_ns() const { return ticks((reg(0x38) & 0x1FF) + ((reg(0x38) >> 9) & 0x7F)); }
    std::uint64_t hold_ns() const { return std::min(ticks((reg(0x30) & 0x1FF) + 1), low_ns() - 1); }
    std::uint64_t sample_ns() const { return std::min(ticks((reg(0x34) & 0x1FF) + 1), high_ns() - 1); }
    std::uint64_t timing(std::uint32_t offset) const { return ticks((reg(offset) & 0x1FF) + 1); }

    void interrupt(std::uint32_t bits)
    {
        reg(0x20) |= bits;
        update_interrupt();
    }
    void update_interrupt() { raise(29, (reg(0x20) & reg(0x28)) != 0); } // ETS_I2C_EXT0_INTR_SOURCE

    void add(std::uint64_t at, Action action, bool level = true) { steps_.push_back({at, action, level}); }
    std::uint64_t last() const { return steps_.empty() ? last_at_ : steps_.back().at; }

    void start(std::uint64_t now)
    {
        if (running_) {
            return;
        }
        running_ = true;
        busy_ = true;
        command_ = 0;
        last_at_ = now;
        interrupt(1u << 9); // TRANS_START
        next_command(now);
    }
    void abort_transfer(std::uint64_t now)
    {
        steps_.clear();
        running_ = busy_ = false;
        command_ = 0;
        last_at_ = now;
        set_line(scl_signal, true);
        set_line(sda_signal, true);
    }
    void set_line(int signal, bool level)
    {
        (signal == scl_signal ? scl_ : sda_) = level;
        drive_signal(signal, level);
    }

    // Queues the steps of COMDn (n = command_) after the last step.
    void next_command(std::uint64_t now)
    {
        if (!running_) {
            return;
        }
        if (command_ >= 8) {
            running_ = false;
            return;
        }
        const std::uint32_t cmd = reg(0x58 + 4 * command_);
        const std::uint32_t op = (cmd >> 11) & 7;
        std::uint64_t t = std::max(now, last_at_);
        switch (op) {
        case 6: // RSTART
            if (!scl_) { // a repeated START: release SDA, then SCL
                t += hold_ns();
                add(t, Action::Sda, true);
                t += low_ns() - hold_ns();
                add(t, Action::Scl, true);
            }
            t += timing(0x44); // SCL_RSTART_SETUP
            add(t, Action::Sda, false);
            t += timing(0x40); // SCL_START_HOLD
            add(t, Action::Scl, false);
            add(t, Action::CommandDone);
            break;
        case 1: // WRITE
            byte_count_ = std::max<std::uint32_t>(cmd & 0xFF, 1);
            write_byte(t);
            break;
        case 3: // READ
            byte_count_ = std::max<std::uint32_t>(cmd & 0xFF, 1);
            read_byte(t, cmd);
            break;
        case 2: // STOP
            stop(t);
            break;
        case 4: // END: hold the bus until the next TRANS_START
            reg(0x58 + 4 * command_) |= 1u << 31;
            running_ = false;
            interrupt(1u << 3); // END_DETECT
            break;
        default: // no command: the list ends
            running_ = false;
            busy_ = !(scl_ && sda_);
            break;
        }
    }
    void write_byte(std::uint64_t t)
    {
        std::uint8_t byte = 0xFF;
        if (tx_.empty()) {
            interrupt(1u << 6); // MST_TXFIFO_UDF
        } else {
            byte = tx_.front();
            tx_.pop_front();
        }
        for (int bit = 7; bit >= 0; --bit) {
            clock_bit(t, ((byte >> bit) & 1) != 0, Action::Scl);
        }
        clock_bit(t, true, Action::SampleAck); // the slave's ACK
        add(t, Action::ByteDone);
    }
    void read_byte(std::uint64_t t, std::uint32_t cmd)
    {
        for (int bit = 7; bit >= 0; --bit) {
            clock_bit(t, true, Action::SampleBit);
        }
        clock_bit(t, (cmd & (1u << 10)) != 0, Action::Scl); // ACK_VAL
        add(t, Action::ByteDone);
    }
    // One bit from SCL low: SDA after the hold time, SCL up, a sample, SCL down.
    void clock_bit(std::uint64_t& t, bool sda, Action sample)
    {
        const std::uint64_t start = t;
        add(start + hold_ns(), Action::Sda, sda);
        add(start + low_ns(), Action::Scl, true);
        if (sample != Action::Scl) {
            add(start + low_ns() + sample_ns(), sample);
        }
        t = start + low_ns() + high_ns();
        add(t, Action::Scl, false);
    }
    void stop(std::uint64_t t)
    {
        if (scl_) { // the bus is free already
            if (sda_) {
                add(t, Action::Stopped);
                return;
            }
        } else {
            t += hold_ns();
            add(t, Action::Sda, false);
            t += low_ns() - hold_ns();
            add(t, Action::Scl, true);
        }
        t += timing(0x4C); // SCL_STOP_SETUP
        add(t, Action::Sda, true);
        t += timing(0x48); // SCL_STOP_HOLD
        add(t, Action::Stopped);
    }

    void perform(const Step& step, std::uint64_t now)
    {
        last_at_ = step.at;
        switch (step.action) {
        case Action::Scl:
            set_line(scl_signal, step.level);
            if (step.level && !input_signal(scl_signal)) {
                stretch(step, now); // a slave holds SCL low
            } else if (step.level) {
                stretching_ = false;
            }
            break;
        case Action::Sda: set_line(sda_signal, step.level); break;
        case Action::SampleBit: shift_ = static_cast<std::uint8_t>((shift_ << 1) | (input_signal(sda_signal) ? 1 : 0)); break;
        case Action::SampleAck: ack_bit_ = input_signal(sda_signal); break;
        case Action::ByteDone: byte_done(now); break;
        case Action::CommandDone:
            reg(0x58 + 4 * command_) |= 1u << 31;
            ++command_;
            break;
        case Action::Stopped:
            reg(0x58 + 4 * command_) |= 1u << 31;
            running_ = busy_ = false;
            command_ = 0;
            interrupt(1u << 7); // TRANS_COMPLETE
            break;
        }
    }
    void byte_done(std::uint64_t now)
    {
        const std::uint32_t cmd = reg(0x58 + 4 * command_);
        const std::uint32_t op = (cmd >> 11) & 7;
        interrupt(1u << 4); // BYTE_TRANS_DONE
        if (op == 3) {
            if (rx_.size() < fifo_size) {
                rx_.push_back(shift_);
            } else {
                interrupt(1u << 2); // RXFIFO_OVF
            }
        } else if ((cmd & (1u << 8)) && ack_bit_ != ((cmd & (1u << 9)) != 0)) {
            // NACK: the master ends the transfer with a STOP.
            interrupt(1u << 10);
            steps_.clear();
            stop(now);
            return;
        }
        if (--byte_count_ > 0) {
            if (op == 3) {
                read_byte(now, cmd);
            } else {
                write_byte(now);
            }
        } else {
            reg(0x58 + 4 * command_) |= 1u << 31;
            ++command_;
        }
    }
    // SCL stays low after release: wait for the slave, up to the timeout.
    void stretch(const Step& step, std::uint64_t now)
    {
        if (!stretching_) {
            stretching_ = true;
            stretch_since_ = now;
        }
        const std::uint32_t to = reg(0x0C);
        const std::uint32_t exponent = (to & (1u << 5)) ? std::min<std::uint32_t>(to & 0x1F, 22) : 22;
        if (now - stretch_since_ > ticks(1u << exponent)) {
            interrupt(1u << 8); // TIME_OUT
            if (!timeout_reported_) {
                timeout_reported_ = true;
                note("I2C0: SCL stays low; check the pull-up resistors on SCL and SDA");
            }
            stretching_ = false;
            abort_transfer(now);
            return;
        }
        // Check again a little later; the later steps move by the same delay.
        const std::uint64_t delay = std::max<std::uint64_t>(ticks(8), 1000);
        for (Step& s : steps_) {
            s.at += delay;
        }
        steps_.push_front({step.at + delay, Action::Scl, true});
    }

    std::deque<Step> steps_;
    std::deque<std::uint8_t> tx_;
    std::deque<std::uint8_t> rx_;
    bool running_ = false;
    bool busy_ = false;
    bool ack_bit_ = false;
    bool scl_ = true;
    bool sda_ = true;
    bool stretching_ = false;
    bool timeout_reported_ = false;
    std::uint64_t stretch_since_ = 0;
    std::uint64_t last_at_ = 0;
    std::uint32_t command_ = 0;
    std::uint32_t byte_count_ = 0;
    std::uint8_t shift_ = 0;
};

// GDMA (TRM chapter "GDMA Controller"): three channels, each with an RX (in)
// and a TX (out) side tied to a peripheral by IN/OUT_PERI_SEL (0: SPI2).
// Descriptors in SRAM: word 0 size 11:0, length 23:12, SUC_EOF 30, OWNER 31;
// word 1 the buffer; word 2 the next descriptor (0 ends the list). Links
// hold the descriptor's address bits 19:0 under 0x3FC00000. The peripheral
// pops bytes from the out side and pushes into the in side as it shifts
// them; interrupts per channel (RAW 0x10n, ST, ENA, CLR) on sources 47-49.
class Gdma : public Esp32C3::Block {
public:
    explicit Gdma(Esp32C3& machine) : Block(machine, "GDMA") {}

    // The next byte of the out link of the channel serving `peripheral`.
    bool pop(std::uint32_t peripheral, std::uint8_t& byte)
    {
        for (std::uint32_t ch = 0; ch < channels; ++ch) {
            Side& out = out_[ch];
            if (!out.active || (reg(0x100 + stride * ch) & 0x3F) != peripheral) {
                continue;
            }
            while (out.active) {
                const std::uint32_t word0 = load(out.descriptor, 4);
                const std::uint32_t length = (word0 >> 12) & 0xFFF;
                if (out.offset < length) {
                    byte = static_cast<std::uint8_t>(load(load(out.descriptor + 4, 4) + out.offset, 1));
                    if (++out.offset == length) {
                        finish_out(ch, word0);
                    }
                    return true;
                }
                finish_out(ch, word0); // an empty buffer
            }
        }
        return false;
    }
    // A byte into the in link of the channel serving `peripheral`.
    void push(std::uint32_t peripheral, std::uint8_t byte)
    {
        for (std::uint32_t ch = 0; ch < channels; ++ch) {
            Side& in = in_[ch];
            if (!in.active || (reg(0xA0 + stride * ch) & 0x3F) != peripheral) {
                continue;
            }
            const std::uint32_t size = load(in.descriptor, 4) & 0xFFF;
            store(load(in.descriptor + 4, 4) + in.offset, 1, byte);
            if (++in.offset >= size) {
                finish_in(ch, false);
            }
            return;
        }
    }
    // The peripheral's transfer ended: the in link's EOF.
    void end_in(std::uint32_t peripheral)
    {
        for (std::uint32_t ch = 0; ch < channels; ++ch) {
            if (in_[ch].active && (reg(0xA0 + stride * ch) & 0x3F) == peripheral) {
                finish_in(ch, true);
            }
        }
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset < 0x10 * channels && offset % 0x10 == 4) { // INT_ST
            return reg(offset - 4) & reg(offset + 4);
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset < 0x10 * channels) {
            const std::uint32_t ch = offset / 0x10;
            if (offset % 0x10 == 0x0C) { // INT_CLR
                reg(0x10 * ch) &= ~value;
            } else if (offset % 0x10 != 4) {
                Block::write_word(offset, value);
            }
            update(ch);
            return;
        }
        if (offset >= 0x70 && offset < 0x70 + stride * channels) {
            const std::uint32_t ch = (offset - 0x70) / stride;
            const std::uint32_t local = (offset - 0x70) % stride + 0x70;
            if (local == 0x70 && (value & 1)) { // IN_CONF0.IN_RST
                in_[ch] = Side{};
            } else if (local == 0xD0 && (value & 1)) { // OUT_CONF0.OUT_RST
                out_[ch] = Side{};
            } else if (local == 0x80) { // IN_LINK: START 22, STOP 21, RESTART 23
                Block::write_word(offset, value & 0x1FFFFF & ~(1u << 21));
                if (value & (1u << 21)) {
                    in_[ch].active = false;
                }
                if (value & ((1u << 22) | (1u << 23))) {
                    in_[ch] = Side{true, 0x3FC00000u | (value & 0xFFFFF), 0};
                }
                return;
            } else if (local == 0xE0) { // OUT_LINK: START 21, STOP 20, RESTART 22
                Block::write_word(offset, value & 0xFFFFF);
                if (value & (1u << 20)) {
                    out_[ch].active = false;
                }
                if (value & ((1u << 21) | (1u << 22))) {
                    out_[ch] = Side{true, 0x3FC00000u | (value & 0xFFFFF), 0};
                }
                return;
            }
        }
        Block::write_word(offset, value);
    }
    void on_reset() override
    {
        for (std::uint32_t ch = 0; ch < channels; ++ch) {
            in_[ch] = out_[ch] = Side{};
            reg(0xA0 + stride * ch) = 0x3F; // no peripheral
            reg(0x100 + stride * ch) = 0x3F;
        }
    }

private:
    static constexpr std::uint32_t channels = 3;
    static constexpr std::uint32_t stride = 0xC0;
    struct Side {
        bool active = false;
        std::uint32_t descriptor = 0;
        std::uint32_t offset = 0;
    };

    void interrupt(std::uint32_t ch, std::uint32_t bits)
    {
        reg(0x10 * ch) |= bits;
        update(ch);
    }
    void update(std::uint32_t ch) { raise(47 + static_cast<int>(ch), (reg(0x10 * ch) & reg(0x10 * ch + 8)) != 0); }

    void finish_out(std::uint32_t ch, std::uint32_t word0)
    {
        Side& out = out_[ch];
        if (reg(0xD0 + stride * ch) & (1u << 2)) { // OUT_AUTO_WRBACK: the hardware gives the descriptor back
            store(out.descriptor, 4, word0 & ~(1u << 31));
        }
        std::uint32_t bits = 1u << 3; // OUT_DONE
        if (word0 & (1u << 30)) {
            bits |= (1u << 4) | (1u << 8); // OUT_EOF, OUT_TOTAL_EOF
            reg(0xE8 + stride * ch) = out.descriptor; // OUT_EOF_DES_ADDR
        }
        const std::uint32_t next = load(out.descriptor + 8, 4);
        out = next ? Side{true, next, 0} : Side{};
        interrupt(ch, bits);
    }
    void finish_in(std::uint32_t ch, bool eof)
    {
        Side& in = in_[ch];
        std::uint32_t word0 = load(in.descriptor, 4);
        word0 = (word0 & ~((0xFFFu << 12) | (1u << 31) | (1u << 30))) | (in.offset << 12) | (eof ? 1u << 30 : 0u);
        store(in.descriptor, 4, word0);
        std::uint32_t bits = 1u; // IN_DONE
        if (eof) {
            bits |= 1u << 1; // IN_SUC_EOF
            reg(0x88 + stride * ch) = in.descriptor; // IN_SUC_EOF_DES_ADDR
        }
        const std::uint32_t next = load(in.descriptor + 8, 4);
        in = next && !eof ? Side{true, next, 0} : Side{};
        interrupt(ch, bits);
    }

    Side in_[channels];
    Side out_[channels];
};

// GP-SPI2 as a master (TRM chapter "SPI Controller"): CMD.USR runs one
// transaction of CS setup, command (USER2), address (ADDR), dummy cycles and
// data (MS_DLEN bits, full duplex with USER.DOUTDIN, otherwise the MOSI part
// then the MISO part), and CS hold; CMD.USR clears itself and TRANS_DONE is
// raised (interrupt source 19). Data from and to W0-W15 or, with DMA_TX_ENA /
// DMA_RX_ENA, the GDMA channel of peripheral 0. Modes from MISC.CK_IDLE_EDGE
// and USER.CK_OUT_EDGE; the clock from CLOCK (80 MHz APB); bit order from
// CTRL. Signals FSPICLK 63, FSPID 65 (MOSI), FSPIQ 64 (MISO in), FSPICS0-5
// 68-73, through the matrix or IO_MUX. Single-line transfers only.
class Spi2 : public Esp32C3::Block, public Esp32C3::Timed {
public:
    Spi2(Esp32C3& machine, Gdma& dma) : Block(machine, "SPI2"), dma_(dma)
    {
        claim_signal(clk_signal);
        claim_signal(mosi_signal);
        for (int cs = 0; cs < 6; ++cs) {
            claim_signal(cs0_signal + cs);
        }
    }

    std::uint64_t next_ns() const override { return steps_.empty() ? no_event : steps_.front().at; }
    void on_ns(std::uint64_t now) override
    {
        while (!steps_.empty() && steps_.front().at <= now) {
            const Step step = steps_.front();
            steps_.pop_front();
            perform(step, now);
        }
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x40) { // DMA_INT_ST
            return reg(0x3C) & reg(0x34);
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        switch (offset) {
        case 0x00: { // CMD: UPDATE (23) applies at once; USR (24) starts a transaction
            const bool busy = (reg(0x00) & (1u << 24)) != 0;
            Block::write_word(offset, (value & ~((1u << 23) | (1u << 24))) | (busy ? 1u << 24 : 0u));
            if ((value & (1u << 24)) && !busy) {
                reg(0x00) |= 1u << 24;
                begin(now_ns());
            }
            break;
        }
        case 0x38: // DMA_INT_CLR
            reg(0x3C) &= ~value;
            update_interrupt();
            break;
        case 0x34: // DMA_INT_ENA
        case 0x3C: // DMA_INT_RAW: ESP-IDF sets TRANS_DONE so that its ISR starts the first transaction
            Block::write_word(offset, value);
            update_interrupt();
            break;
        case 0x08: // CTRL
        case 0x20: // MISC: the idle levels of SCK and CS follow at once
            Block::write_word(offset, value);
            if (steps_.empty()) {
                idle_lines();
            }
            break;
        default: Block::write_word(offset, value);
        }
        reschedule();
    }
    void on_reset() override
    {
        steps_.clear();
        idle_lines();
    }

private:
    static constexpr int clk_signal = 63;
    static constexpr int miso_signal = 64;
    static constexpr int mosi_signal = 65;
    static constexpr int cs0_signal = 68;
    static constexpr std::uint32_t dma_peripheral = 0;
    enum class Action { Clk, Mosi, Sample, Cs, Done };
    struct Step {
        std::uint64_t at = 0;
        Action action = Action::Clk;
        bool level = false;
    };
    // The bits of one phase: from TX, into RX, or both.
    struct Phase {
        std::vector<bool> out; // empty: MOSI keeps its level
        std::size_t in = 0;    // bits to receive
    };

    bool idle_clock() const { return (reg(0x20) & (1u << 29)) != 0; } // MISC.CK_IDLE_EDGE
    bool cpha() const { return idle_clock() != ((reg(0x10) & (1u << 9)) != 0); } // USER.CK_OUT_EDGE
    bool cs_active_level(int cs) const { return ((reg(0x20) >> (7 + cs)) & 1) != 0; } // MASTER_CS_POL
    int selected_cs() const
    {
        for (int cs = 0; cs < 6; ++cs) {
            if (!(reg(0x20) & (1u << cs))) {
                return cs;
            }
        }
        return -1;
    }
    std::uint64_t half_ns() const
    {
        const std::uint32_t clock = reg(0x0C);
        double hz = 80'000'000.0;
        if (!(clock & (1u << 31))) {
            hz /= static_cast<double>((((clock >> 18) & 0xF) + 1) * (((clock >> 12) & 0x3F) + 1));
        }
        return std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::llround(0.5e9 / hz)));
    }

    void idle_lines()
    {
        drive_signal(clk_signal, idle_clock());
        const bool keep = (reg(0x20) & (1u << 30)) != 0 && active_cs_ >= 0; // CS_KEEP_ACTIVE
        for (int cs = 0; cs < 6; ++cs) {
            drive_signal(cs0_signal + cs, keep && cs == active_cs_ ? cs_active_level(cs) : !cs_active_level(cs));
        }
    }
    void update_interrupt() { raise(19, (reg(0x3C) & reg(0x34)) != 0); } // ETS_SPI2_INTR_SOURCE

    // Bits of a register value sent MSB first per byte, bytes in memory order.
    static void append_bits(std::vector<bool>& bits, std::uint32_t value, std::uint32_t count, bool lsb_first)
    {
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t byte = i / 8;
            const std::uint32_t bit = lsb_first ? i % 8 : 7 - i % 8;
            bits.push_back(((value >> (8 * byte + bit)) & 1) != 0);
        }
    }
    std::uint8_t tx_byte(std::size_t index)
    {
        if (reg(0x30) & (1u << 28)) { // DMA_TX_ENA
            std::uint8_t byte = 0;
            if (!dma_.pop(dma_peripheral, byte)) {
                reg(0x3C) |= 1u << 1; // DMA_OUTFIFO_EMPTY_ERR
            }
            return byte;
        }
        const std::size_t base = (reg(0x10) & (1u << 25)) ? 32 : 0; // USR_MOSI_HIGHPART
        const std::size_t at = (base + index) % 64;
        return static_cast<std::uint8_t>(reg(0x98 + 4 * static_cast<std::uint32_t>(at / 4)) >> (8 * (at % 4)));
    }

    // Builds the whole transaction's steps; data bits from W0-15 or the DMA now.
    void begin(std::uint64_t now)
    {
        const std::uint32_t user = reg(0x10);
        const bool tx_lsb = (reg(0x08) & (1u << 26)) != 0;
        rx_lsb_ = (reg(0x08) & (1u << 25)) != 0;
        const std::uint64_t half = half_ns();
        std::vector<Phase> phases;
        if (user & (1u << 31)) { // USR_COMMAND
            Phase p;
            append_bits(p.out, reg(0x18) & 0xFFFF, ((reg(0x18) >> 28) & 0xF) + 1, tx_lsb);
            phases.push_back(std::move(p));
        }
        if (user & (1u << 30)) { // USR_ADDR
            Phase p;
            append_bits(p.out, reg(0x04), ((reg(0x14) >> 27) & 0x1F) + 1, tx_lsb);
            phases.push_back(std::move(p));
        }
        if (user & (1u << 29)) { // USR_DUMMY
            Phase p;
            p.out.assign((reg(0x14) & 0xFF) + 1, true);
            phases.push_back(std::move(p));
        }
        const std::uint32_t bits = (reg(0x1C) & 0x3FFFF) + 1;
        const bool mosi = (user & (1u << 27)) != 0;
        const bool miso = (user & (1u << 28)) != 0;
        auto data_out = [&](Phase& p) {
            for (std::uint32_t i = 0; i < bits; i += 8) {
                append_bits(p.out, tx_byte(i / 8), std::min<std::uint32_t>(8, bits - i), tx_lsb);
            }
        };
        if ((user & 1) && (mosi || miso)) { // DOUTDIN: full duplex
            Phase p;
            if (mosi) {
                data_out(p);
            }
            p.in = miso ? bits : 0;
            if (!mosi) {
                p.out.assign(bits, true);
            }
            phases.push_back(std::move(p));
        } else {
            if (mosi) {
                Phase p;
                data_out(p);
                phases.push_back(std::move(p));
            }
            if (miso) {
                Phase p;
                p.in = bits;
                phases.push_back(std::move(p));
            }
        }
        rx_bits_ = 0;
        rx_shift_ = 0;
        rx_count_ = 0;
        active_cs_ = selected_cs();
        std::uint64_t t = now;
        if (active_cs_ >= 0) {
            add(t, Action::Cs, cs_active_level(active_cs_));
        }
        if (user & (1u << 7)) { // CS_SETUP
            t += 2 * half * (((reg(0x14) >> 17) & 0x1F) + 1);
        }
        const bool idle = idle_clock();
        for (const Phase& p : phases) {
            const std::size_t count = std::max(p.out.size(), p.in);
            for (std::size_t i = 0; i < count; ++i) {
                const bool out = i < p.out.size() ? p.out[i] : true;
                const bool sample = i < p.in;
                if (!cpha()) { // data before the leading edge, sampled on it
                    if (i < p.out.size()) {
                        add(t, Action::Mosi, out);
                    }
                    add(t + half, Action::Clk, !idle);
                    if (sample) {
                        add(t + half, Action::Sample);
                    }
                    add(t + 2 * half, Action::Clk, idle);
                } else { // data on the leading edge, sampled on the trailing one
                    add(t, Action::Clk, !idle);
                    if (i < p.out.size()) {
                        add(t, Action::Mosi, out);
                    }
                    add(t + half, Action::Clk, idle);
                    if (sample) {
                        add(t + half, Action::Sample);
                    }
                }
                t += 2 * half;
            }
        }
        if (user & (1u << 6)) { // CS_HOLD
            t += 2 * half * ((reg(0x14) >> 22) & 0x1F);
        }
        add(t + half, Action::Done);
    }
    void add(std::uint64_t at, Action action, bool level = false) { steps_.push_back({at, action, level}); }

    void perform(const Step& step, std::uint64_t now)
    {
        (void)now;
        switch (step.action) {
        case Action::Clk: drive_signal(clk_signal, step.level); break;
        case Action::Mosi: drive_signal(mosi_signal, step.level); break;
        case Action::Cs: drive_signal(cs0_signal + active_cs_, step.level); break;
        case Action::Sample: shift_in(input_signal(miso_signal)); break;
        case Action::Done: finish(); break;
        }
    }
    void shift_in(bool bit)
    {
        if (rx_lsb_) {
            rx_shift_ = static_cast<std::uint8_t>(rx_shift_ | (bit ? 1u << rx_bits_ : 0u));
        } else {
            rx_shift_ = static_cast<std::uint8_t>((rx_shift_ << 1) | (bit ? 1 : 0));
        }
        if (++rx_bits_ == 8) {
            store_rx();
        }
    }
    void store_rx()
    {
        if (reg(0x30) & (1u << 27)) { // DMA_RX_ENA
            dma_.push(dma_peripheral, rx_shift_);
        } else {
            const std::size_t base = (reg(0x10) & (1u << 24)) ? 32 : 0; // USR_MISO_HIGHPART
            const std::size_t at = (base + rx_count_) % 64;
            std::uint32_t& word = reg(0x98 + 4 * static_cast<std::uint32_t>(at / 4));
            const std::uint32_t shift = 8 * static_cast<std::uint32_t>(at % 4);
            word = (word & ~(0xFFu << shift)) | (static_cast<std::uint32_t>(rx_shift_) << shift);
        }
        ++rx_count_;
        rx_bits_ = 0;
        rx_shift_ = 0;
    }
    void finish()
    {
        if (rx_bits_ > 0) { // a last partial byte, left aligned for MSB first
            if (!rx_lsb_) {
                rx_shift_ = static_cast<std::uint8_t>(rx_shift_ << (8 - rx_bits_));
            }
            store_rx();
        }
        if (reg(0x30) & (1u << 27)) {
            dma_.end_in(dma_peripheral);
        }
        idle_lines();
        reg(0x00) &= ~(1u << 24); // CMD.USR
        reg(0x3C) |= 1u << 12;    // TRANS_DONE
        update_interrupt();
    }

    Gdma& dma_;
    std::deque<Step> steps_;
    int active_cs_ = -1;
    bool rx_lsb_ = false;
    std::uint32_t rx_bits_ = 0;
    std::uint8_t rx_shift_ = 0;
    std::size_t rx_count_ = 0;
};

// RMT transmit channels 0 and 1 (TRM chapter "Remote Control Peripheral"):
// symbols in the RMT RAM at +0x400, 48 words per channel (MEM_SIZE blocks
// from the channel's own), each word two parts of duration (15 bits, in
// ticks) and level; a duration of 0 ends the transmission. CHnCONF0.TX_START
// sends from the start of the channel's memory on signal 51 + n, at the
// tick of SYS_CONF's clock (1 APB 80 MHz, 2 RC_FAST, 3 XTAL; SCLK_DIV_NUM)
// divided by DIV_CNT (0: 256). MEM_TX_WRAP_EN wraps at the end of the memory
// and TX_LIM raises TX_THR_EVENT every so many words, so drivers refill one
// half while the other goes out; TX_CONTI_MODE repeats, up to TX_LOOP_NUM
// with TX_LOOP_CNT_EN. Interrupts on source 28. Not modelled: the carrier,
// receive channels, the APB FIFO access, synchronous starts.
class Rmt : public Esp32C3::Block, public Esp32C3::Timed {
public:
    explicit Rmt(Esp32C3& machine) : Block(machine, "RMT")
    {
        claim_signal(51);
        claim_signal(52);
    }

    std::uint64_t next_ns() const override { return std::min(tx_[0].next, tx_[1].next); }
    void on_ns(std::uint64_t now) override
    {
        for (std::uint32_t ch = 0; ch < 2; ++ch) {
            while (tx_[ch].next <= now) {
                step(ch, tx_[ch].next);
            }
        }
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x3C) { // INT_ST
            return reg(0x38) & reg(0x40);
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        const std::uint64_t now = now_ns();
        if (offset == 0x10 || offset == 0x14) { // CH0/1CONF0
            const std::uint32_t ch = (offset - 0x10) / 4;
            // TX_START (0), MEM_RD_RST (1), APB_MEM_RST (2), TX_STOP (7), AFIFO_RST (23)
            // and CONF_UPDATE (24) act once.
            Block::write_word(offset, value & ~((1u << 0) | (1u << 1) | (1u << 2) | (1u << 7) | (1u << 23) | (1u << 24)));
            if (value & (1u << 1)) {
                tx_[ch].word = 0;
                tx_[ch].second = false;
            }
            if (value & (1u << 7)) {
                stop(ch);
            }
            if (value & 1u) {
                start(ch, now);
            }
            if (tx_[ch].next == no_event) {
                drive_signal(51 + static_cast<int>(ch), idle_level(ch, last_[ch]));
            }
        } else if (offset == 0x44) { // INT_CLR
            reg(0x38) &= ~value;
            update();
        } else if (offset == 0x40) { // INT_ENA
            Block::write_word(offset, value);
            update();
        } else if (offset == 0x58 || offset == 0x5C) { // CHn_TX_LIM: LOOP_COUNT_RESET (20) acts once
            Block::write_word(offset, value & ~(1u << 20));
            if (value & (1u << 20)) {
                tx_[(offset - 0x58) / 4].loops = 0;
            }
        } else {
            Block::write_word(offset, value);
        }
        reschedule();
    }
    void on_reset() override
    {
        for (std::uint32_t ch = 0; ch < 2; ++ch) {
            tx_[ch] = Channel{};
            last_[ch] = false;
            drive_signal(51 + static_cast<int>(ch), false);
        }
        reg(0x68) = (1u << 24) | (1u << 31); // SYS_CONF: APB clock
    }

private:
    struct Channel {
        std::uint64_t next = no_event; // the next part starts
        std::uint32_t word = 0;        // in the channel's memory
        bool second = false;           // the word's second part
        std::uint32_t sent = 0;        // words since the start (TX_THR_EVENT)
        std::uint32_t loops = 0;
    };

    double tick_ns(std::uint32_t ch) const
    {
        const std::uint32_t sys = reg(0x68);
        double hz = 80'000'000.0;
        switch ((sys >> 24) & 3) {
        case 2: hz = 17'500'000.0; break;
        case 3: hz = 40'000'000.0; break;
        default: break;
        }
        const std::uint32_t div = (reg(0x10 + 4 * ch) >> 8) & 0xFF;
        return 1e9 / hz * static_cast<double>(((sys >> 4) & 0xFF) + 1) * static_cast<double>(div ? div : 256);
    }
    std::uint32_t memory_words(std::uint32_t ch) const
    {
        const std::uint32_t blocks = std::clamp<std::uint32_t>((reg(0x10 + 4 * ch) >> 16) & 7, 1, 4 - ch);
        return 48 * blocks;
    }
    bool idle_level(std::uint32_t ch, bool last) const
    {
        const std::uint32_t conf = reg(0x10 + 4 * ch);
        return (conf & (1u << 6)) ? (conf & (1u << 5)) != 0 : last; // IDLE_OUT_EN, IDLE_OUT_LV
    }
    void interrupt(std::uint32_t bits)
    {
        reg(0x38) |= bits;
        update();
    }
    void update() { raise(28, (reg(0x38) & reg(0x40)) != 0); } // ETS_RMT_INTR_SOURCE

    void start(std::uint32_t ch, std::uint64_t now)
    {
        Channel& c = tx_[ch];
        c.word = 0;
        c.second = false;
        c.sent = 0;
        c.next = now;
    }
    void stop(std::uint32_t ch)
    {
        tx_[ch].next = no_event;
        tx_[ch].word = 0;
        tx_[ch].second = false;
        drive_signal(51 + static_cast<int>(ch), idle_level(ch, last_[ch]));
    }
    // The part of the symbol that starts at `at`.
    void step(std::uint32_t ch, std::uint64_t at)
    {
        Channel& c = tx_[ch];
        const std::uint32_t symbol = reg(0x400 + 4 * (48 * ch + c.word));
        const std::uint32_t duration = c.second ? (symbol >> 16) & 0x7FFF : symbol & 0x7FFF;
        const bool level = c.second ? (symbol >> 31) != 0 : ((symbol >> 15) & 1) != 0;
        if (duration == 0) { // the end marker
            last_[ch] = level;
            const std::uint32_t lim = reg(0x58 + 4 * ch);
            if (reg(0x10 + 4 * ch) & (1u << 3)) { // TX_CONTI_MODE
                ++c.loops;
                if ((lim & (1u << 19)) && c.loops >= ((lim >> 9) & 0x3FF)) { // TX_LOOP_CNT_EN, TX_LOOP_NUM
                    interrupt(1u << (12 + ch)); // TX_LOOP
                    stop(ch);
                    return;
                }
                start(ch, at);
                if (symbol == 0) { // nothing to send: wait for a new start
                    stop(ch);
                }
                return;
            }
            interrupt(1u << ch); // TX_END
            stop(ch);
            return;
        }
        drive_signal(51 + static_cast<int>(ch), level);
        last_[ch] = level;
        c.next = at + std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::llround(duration * tick_ns(ch))));
        if (!c.second) {
            c.second = true;
            return;
        }
        c.second = false;
        ++c.word;
        ++c.sent;
        const std::uint32_t limit = reg(0x58 + 4 * ch) & 0x1FF;
        if (limit && c.sent % limit == 0) {
            interrupt(1u << (8 + ch)); // TX_THR_EVENT
        }
        if (c.word >= memory_words(ch)) {
            if (reg(0x10 + 4 * ch) & (1u << 4)) { // MEM_TX_WRAP_EN
                c.word = 0;
            } else {
                interrupt(1u << (4 + ch)); // ERR: past the end of the memory
                stop(ch);
            }
        }
    }

    Channel tx_[2];
    bool last_[2] = {false, false};
};

// APB_SARADC, one-shot conversions (ONETIME_SAMPLE: unit and channel in bits
// 28:25, attenuation in 24:23, START bit 29): the result is ready at once in
// SARADC1/2_DATA_STATUS with ADC1/ADC2_DONE in INT_RAW. A linear converter,
// labelled approximate (no offset, no noise, no curve): the inverse of the
// first step of ESP-IDF's calibration with this machine's eFuse (calibration
// version 1, ADC1_CAL_VOL_ATTENn 0: 2000 counts at 400, 550, 750 and
// 1370 mV), so that calibrated readings (MicroPython's read_uv) give the pad's
// voltage to within the calibration's second step (0.1 % at 1.25 V, 1.7 % low
// at 2.4 V with 12 dB); 4095 at 819, 1126, 1536 and 2805 mV. Continuous (DMA) conversions are not modelled
// yet.
class ApbSaradc : public Esp32C3::Block {
public:
    explicit ApbSaradc(Esp32C3& machine) : Block(machine, "APB_SARADC") {}

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x048) { // INT_ST
            return reg(0x044) & reg(0x040);
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset == 0x04C) { // INT_CLR
            reg(0x044) &= ~value;
            return;
        }
        const bool start = offset == 0x020 && (value & (1u << 29)) && !(reg(0x020) & (1u << 29));
        Block::write_word(offset, value);
        if (!start) {
            return;
        }
        const std::uint32_t unit = (value >> 28) & 1;
        const std::uint32_t channel = (value >> 25) & 7;
        const std::uint32_t atten = (value >> 23) & 3;
        const int gpio = unit == 0 ? (channel <= 4 ? static_cast<int>(channel) : -1) : (channel == 0 ? 5 : -1);
        std::uint32_t raw = 0;
        if (gpio >= 0) {
            static constexpr double volts_at_2000[4] = {0.400, 0.550, 0.750, 1.370};
            double volts = 0.0;
            if (const auto v = pad_volts(gpio)) {
                volts = *v;
            } else {
                std::uint32_t in = 0;
                machine_.bus().read(0x6000403Cu, 4, in); // GPIO_IN: the pad's logic level
                volts = (in >> gpio) & 1 ? 3.3 : 0.0;
            }
            const double counts = std::clamp(volts / volts_at_2000[atten] * 2000.0, 0.0, 4095.0);
            raw = static_cast<std::uint32_t>(std::lround(counts));
        }
        if (unit == 0) {
            reg(0x02C) = raw;                    // SARADC1_DATA_STATUS
            reg(0x044) |= 1u << 31;              // ADC1_DONE
        } else {
            reg(0x030) = raw;                    // SARADC2_DATA_STATUS
            reg(0x044) |= 1u << 30;              // ADC2_DONE
        }
    }
};

// The MMU table: 128 entries, each mapping 64 KiB of both cache windows to a
// flash page (bits 7:0) unless bit 8 (invalid) is set.
class MmuTable : public Esp32C3::Block {
public:
    explicit MmuTable(Esp32C3& machine) : Block(machine, "MMU table") {}

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        return offset < 0x200 ? mmu(offset / 4) : Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset < 0x200) {
            mmu(offset / 4) = value & 0x1FF;
            map_page(offset / 4);
            return;
        }
        Block::write_word(offset, value);
    }
};

} // namespace

// Interrupt matrix and the core's interrupt controller (TRM chapter
// "Interrupt Matrix"): each of the 62 sources maps to CPU line 1..31
// (INT_MAP, 0 = not routed); a line is level- or edge-triggered
// (CPU_INT_TYPE; an edge stays pending until CPU_INT_CLEAR), enabled by
// CPU_INT_ENABLE, and taken when its priority (CPU_INT_PRI_n, 1..15; 0 is
// off) is at least CPU_INT_THRESH (ESP-IDF runs with the threshold at 1 and
// raises it to the priority + 1 inside a handler); the highest priority goes
// first, then the lowest number.
class Esp32C3::Interrupts : public Esp32C3::Block {
public:
    explicit Interrupts(Esp32C3& machine) : Block(machine, "INTERRUPT") {}

    void set_source(int source, bool level)
    {
        if (source >= 0 && source < 62 && levels_[source] != level) {
            levels_[source] = level;
            update();
        }
    }
    int choose(std::uint32_t ready)
    {
        int best = -1;
        std::uint32_t best_priority = 0;
        for (int line = 1; line < 32; ++line) {
            const std::uint32_t priority = reg(0x114 + 4 * static_cast<std::uint32_t>(line)) & 15;
            if ((ready >> line) & 1 && (best < 0 || priority > best_priority)) {
                best = line;
                best_priority = priority;
            }
        }
        return best;
    }

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x110) { // CPU_INT_EIP_STATUS
            return pending_;
        }
        if (offset == 0x0F8 || offset == 0x0FC) { // INTR_STATUS_0/1: raw source levels
            std::uint32_t bits = 0;
            for (int i = 0; i < 32; ++i) {
                const int source = (offset == 0x0F8 ? 0 : 32) + i;
                if (source < 62 && levels_[source]) {
                    bits |= 1u << i;
                }
            }
            return bits;
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset == 0x10C) { // CPU_INT_CLEAR: edge-triggered lines
            edges_ &= ~value;
        } else {
            Block::write_word(offset, value);
        }
        update();
    }
    void on_reset() override
    {
        levels_.fill(false);
        line_levels_ = edges_ = pending_ = 0;
    }

private:
    void update()
    {
        std::uint32_t lines = 0;
        for (int source = 0; source < 62; ++source) {
            const std::uint32_t line = reg(4 * static_cast<std::uint32_t>(source)) & 31;
            if (levels_[source] && line != 0) {
                lines |= 1u << line;
            }
        }
        const std::uint32_t type = reg(0x108);
        edges_ |= lines & ~line_levels_ & type; // rising edges latch
        line_levels_ = lines;
        pending_ = (lines & ~type) | (edges_ & type);
        const std::uint32_t threshold = reg(0x194) & 15;
        std::uint32_t ready = 0;
        for (int line = 1; line < 32; ++line) {
            const std::uint32_t priority = reg(0x114 + 4 * static_cast<std::uint32_t>(line)) & 15;
            if ((pending_ >> line) & 1 && priority != 0 && priority >= threshold) {
                ready |= 1u << line;
            }
        }
        cpu().set_interrupt_lines(ready & reg(0x104));
    }

    std::array<bool, 62> levels_{};
    std::uint32_t line_levels_ = 0, edges_ = 0, pending_ = 0;
};

void Esp32C3::set_interrupt_source(int source, bool level)
{
    interrupts_->set_source(source, level);
}


// ---------------------------------------------------------------------------
// Machine.

Esp32C3::Esp32C3() : Esp32C3(Options{}) {}

Esp32C3::Esp32C3(const Options& options) : options_(options), cpu_(bus_, core_config())
{
    // The busiest memories first: the bus checks them in this order.
    bus_.add_memory("SRAM1", sram1_ibus, sram1_size, true);
    bus_.add_alias("SRAM1 (data bus)", sram1_dbus, "SRAM1");
    bus_.add_memory("ROM", esp32c3_rom_base, esp32c3_rom_size, false);
    bus_.add_memory("ROM (data bus)", esp32c3_drom_base, esp32c3_drom_size, false);
    bus_.add_memory("SRAM0", sram0_ibus, sram0_size, true);
    bus_.add_memory("RTC FAST memory", rtc_fast, rtc_fast_size, true);
    // Past SRAM1's end the data bus reads 0 instead of faulting: esptool's
    // flasher stub waits for the flash with a chip structure at the ROM's
    // last words (0x3FCDFFF4), whose status mask it reads from 0x3FCE0008.
    bus_.add_memory("past SRAM1 (reads 0)", sram1_dbus + sram1_size, 0x10000, false);
    cpu_.add_code_region(sram1_ibus, sram1_size);
    cpu_.add_code_alias(sram1_dbus, sram1_ibus, sram1_size);
    cpu_.add_code_region(esp32c3_rom_base, esp32c3_rom_size);
    cpu_.add_code_region(sram0_ibus, sram0_size);
    cpu_.add_code_region(rtc_fast, rtc_fast_size);

    // The flash through the cache: one 8 MiB window seen by both buses.
    bus_.add_memory("flash (instruction bus)", flash_ibus, cache_window_size, false);
    bus_.add_alias("flash (data bus)", flash_dbus, "flash (instruction bus)");
    cache_window_ = bus_.memory_at(flash_ibus, cache_window_size);
    cpu_.add_code_region(flash_ibus, cache_window_size);

    flash_.assign(options.flash_size, 0xFF); // erased
    flash_chip_ = std::make_unique<FlashChip>(flash_);

    for (int port = 0; port < 2; ++port) {
        auto uart = std::make_unique<Uart>(*this, port == 0 ? "UART0" : "UART1", port);
        uart_[port] = uart.get();
        timed_.push_back(uart.get());
        attach(port == 0 ? uart0_base : uart1_base, std::move(uart));
    }
    attach(0x60009000u, std::make_unique<IoMux>(*this));
    auto gpio = std::make_unique<Gpio>(*this, options.strapping);
    gpio_ = gpio.get();
    attach(gpio_base, std::move(gpio));
    auto rtc_cntl = std::make_unique<RtcCntl>(*this);
    rtc_cntl_ = rtc_cntl.get();
    rtc_wake_ = [block = rtc_cntl.get()](bool light) { block->woke(light); };
    rtc_pauses_ = [block = rtc_cntl.get()] { return block->watchdog_pauses_in_sleep(); };
    rtc_timed_ = rtc_cntl.get();
    timed_.push_back(rtc_cntl.get());
    attach(rtc_cntl_base, std::move(rtc_cntl), 0x800);
    attach(efuse_base, std::make_unique<Efuse>(*this), 0x800);
    auto usb = std::make_unique<UsbSerialJtag>(*this);
    usb_ = usb.get();
    attach(usb_serial_jtag_base, std::move(usb));
    attach(spi1_base, std::make_unique<SpiMem>(*this, "SPI1", true));
    attach(spi0_base, std::make_unique<SpiMem>(*this, "SPI0", false));
    attach(extmem_base, std::make_unique<Extmem>(*this));
    attach(mmu_table_base, std::make_unique<MmuTable>(*this));
    attach(0x6003B000u, std::make_unique<Sha>(*this));
    attach(0x600C0000u, std::make_unique<System>(*this));
    attach(0x60026000u, std::make_unique<Syscon>(*this));
    auto interrupts = std::make_unique<Interrupts>(*this);
    interrupts_ = interrupts.get();
    attach(0x600C2000u, std::move(interrupts));
    cpu_.choose_interrupt = [this](std::uint32_t ready) { return interrupts_->choose(ready); };
    auto systimer = std::make_unique<Systimer>(*this);
    timed_.push_back(systimer.get());
    attach(0x60023000u, std::move(systimer));
    attach(0x60040000u, std::make_unique<ApbSaradc>(*this));
    auto gdma = std::make_unique<Gdma>(*this);
    auto spi2 = std::make_unique<Spi2>(*this, *gdma);
    timed_.push_back(spi2.get());
    attach(0x6003F000u, std::move(gdma));
    attach(0x60024000u, std::move(spi2));
    auto rmt = std::make_unique<Rmt>(*this);
    timed_.push_back(rmt.get());
    attach(0x60016000u, std::move(rmt));
    auto i2c = std::make_unique<I2c>(*this);
    timed_.push_back(i2c.get());
    attach(0x60013000u, std::move(i2c));
    auto ledc = std::make_unique<Ledc>(*this);
    timed_.push_back(ledc.get());
    attach(0x60019000u, std::move(ledc));
    for (int group = 0; group < 2; ++group) {
        auto timg = std::make_unique<Timg>(*this, group == 0 ? "TIMG0" : "TIMG1", group);
        timed_.push_back(timg.get());
        attach(group == 0 ? 0x6001F000u : 0x60020000u, std::move(timg));
    }
    // Every other peripheral: registers that read back what was written.
    struct Plain {
        std::uint32_t base;
        const char* name;
    };
    static constexpr Plain plain[] = {
        {0x60005000u, "FE2"}, {0x60006000u, "FE"},
        {0x6000E000u, "RTC_I2C"}, {0x60014000u, "UHCI0"},
        {0x6001C000u, "NRX"}, {0x6001D000u, "BB"},
        {0x6002B000u, "TWAI"}, {0x6002D000u, "I2S"}, {0x6003A000u, "AES"},
        {0x6003C000u, "RSA"}, {0x6003D000u, "DS"}, {0x6003E000u, "HMAC"},
        {0x600C1000u, "SENSITIVE"},
        {0x600CC000u, "XTS_AES"}, {0x600CE000u, "ASSIST_DEBUG"},
        {0x600CF000u, "DEDICATED_GPIO"}, {0x600D0000u, "WORLD_CNTL"},
    };
    for (const Plain& p : plain) {
        attach(p.base, std::make_unique<Block>(*this, p.name));
    }
    // The rest of the peripheral space, 4 KiB at a time, so that no register
    // access faults; the trace names these by address.
    for (std::uint32_t page = 0x60000000u; page < 0x600D1000u; page += 0x1000) {
        if (bus_.describe(page).empty() && bus_.describe(page + 0x800).empty()) {
            unnamed_.push_back("peripheral " + hex(page));
            attach(page, std::make_unique<Block>(*this, unnamed_.back().c_str()));
        }
    }

    cpu_.on_trap = [this](std::uint32_t cause, std::uint32_t epc, std::uint32_t tval) {
        if ((cause & 0x80000000u) == 0) {
            const std::string where = function_at(epc);
            diagnostic("Exception " + std::to_string(cause) + " at PC " + hex(epc) + (where.empty() ? "" : " (in " + where + ")")
                + ", mtval " + hex(tval));
        }
    };
    // Vendor CSRs: mpcer, mpcmr and mpccr count cycles for
    // esp_cpu_get_cycle_count(); the ROM also writes CSR 0 and the
    // undocumented 0x800 and 0x801 at start. CSRs in the ranges the
    // architecture leaves to vendors keep what is written.
    cpu_.read_custom_csr = [this](std::uint16_t csr, std::uint32_t& value) {
        if (csr == 0x7E2 || csr == 0x802) { // mpccr, and the counter the ROM's ets_delay_us reads
            value = static_cast<std::uint32_t>(cpu_.cycles() - (csr == 0x7E2 ? pccr_base_ : upccr_base_));
            return true;
        }
        if (!vendor_csr(csr)) {
            return false;
        }
        const auto it = vendor_csrs_.find(csr);
        value = it == vendor_csrs_.end() ? 0 : it->second;
        trace_csr(csr, false, value);
        return true;
    };
    cpu_.write_custom_csr = [this](std::uint16_t csr, std::uint32_t value) {
        if (csr == 0x7E2 || csr == 0x802) {
            (csr == 0x7E2 ? pccr_base_ : upccr_base_) = cpu_.cycles() - value;
            return true;
        }
        if (!vendor_csr(csr)) {
            return false;
        }
        vendor_csrs_[csr] = value;
        trace_csr(csr, true, value);
        return true;
    };
    reset();
}

Esp32C3::~Esp32C3() = default;

void Esp32C3::attach(std::uint32_t base, std::unique_ptr<Block> block, std::uint32_t size)
{
    bus_.add_device(base, size, block.get());
    blocks_.push_back(std::move(block));
}

void Esp32C3::map_page(std::size_t page)
{
    std::uint8_t* window = cache_window_ + page * cache_page;
    const std::uint32_t entry = mmu_[page];
    const std::size_t source = static_cast<std::size_t>(entry & 0xFF) * cache_page;
    if (entry & 0x100 || source >= flash_.size()) {
        std::fill(window, window + cache_page, std::uint8_t{0xFF}); // unmapped (the chip would raise an MMU fault)
    } else {
        std::copy(flash_.begin() + static_cast<std::ptrdiff_t>(source),
            flash_.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(flash_.size(), source + cache_page)), window);
    }
    cpu_.invalidate_code_cache();
}

void Esp32C3::invalidate_cache()
{
    for (std::size_t page = 0; page < 128; ++page) {
        if (!(mmu_[page] & 0x100)) {
            map_page(page);
        }
    }
}

bool Esp32C3::load_rom(const ElfImage& rom_elf, std::string& error)
{
    if (!build_esp32c3_rom(rom_elf, rom_, error)) {
        return false;
    }
    bus_.load(esp32c3_rom_base, rom_.data(), esp32c3_rom_size);
    bus_.load(esp32c3_drom_base, rom_.data() + esp32c3_drom_offset, esp32c3_drom_size);
    add_symbols(rom_elf);
    reset();
    return true;
}

bool Esp32C3::load_flash(std::uint32_t offset, const std::vector<std::uint8_t>& bytes, std::string& error)
{
    if (offset > flash_.size() || flash_.size() - offset < bytes.size()) {
        error = "The image does not fit in the " + std::to_string(flash_.size() / (1024 * 1024)) + " MB flash.";
        return false;
    }
    std::copy(bytes.begin(), bytes.end(), flash_.begin() + offset);
    return true;
}

void Esp32C3::add_symbols(const ElfImage& elf)
{
    functions_.insert(functions_.end(), elf.functions.begin(), elf.functions.end());
    std::sort(functions_.begin(), functions_.end(), [](const ElfSymbol& a, const ElfSymbol& b) { return a.address < b.address; });
}

std::string Esp32C3::function_at(std::uint32_t address) const
{
    auto it = std::upper_bound(functions_.begin(), functions_.end(), address,
        [](std::uint32_t a, const ElfSymbol& s) { return a < s.address; });
    if (it == functions_.begin()) {
        return {};
    }
    --it;
    return address - it->address < std::max<std::uint32_t>(it->size, 2) ? it->name : std::string{};
}

void Esp32C3::reset()
{
    reset(ResetKind::PowerOn);
    resets_ = 0;
}

void Esp32C3::request_reset(ResetKind kind, std::uint32_t reason)
{
    if (reset_requested_ && requested_kind_ == ResetKind::PowerOn) {
        return; // the stronger reset already waits
    }
    reset_requested_ = true;
    requested_kind_ = kind;
    requested_reason_ = reason;
    cpu_.request_stop();
}

void Esp32C3::reset(ResetKind kind, std::uint32_t reason)
{
    reset_requested_ = false;
    ++resets_;
    if (kind == ResetKind::PowerOn) { // CHIP_PU latches the strapping pins
        const bool low = boot_low_ || download_once_;
        download_once_ = false;
        const std::uint32_t strapping = low ? options_.strapping & ~0x8u : options_.strapping; // bit 3: GPIO9
        gpio_->set_strapping(strapping);
        if (!(strapping & 0x8u)) {
            download_boots_++;
        }
    }
    for (const auto& block : blocks_) {
        if (kind == ResetKind::PowerOn || block.get() != rtc_cntl_) {
            block->reset(); // the RTC domain keeps its state except at power-on
        }
    }
    asleep_ = false;
    if (kind == ResetKind::DeepSleep) {
        bus_.fill(sram1_ibus, 0, sram1_size); // powered down; RTC memory kept
        bus_.fill(sram0_ibus, 0, sram0_size);
    }
    if (kind == ResetKind::PowerOn) {
        for (std::uint32_t base : {sram1_ibus, sram0_ibus, rtc_fast}) {
            const std::uint32_t size = base == sram1_ibus ? sram1_size : base == sram0_ibus ? sram0_size : rtc_fast_size;
            bus_.fill(base, 0, size);
        }
    }
    // RTC_CNTL_RESET_STATE_REG: RESET_CAUSE_PROCPU
    if (reason == 0) {
        reason = kind == ResetKind::PowerOn ? 0x1u : kind == ResetKind::System ? 0x3u : kind == ResetKind::DeepSleep ? 0x5u : 0xCu;
    }
    last_reason_ = reason;
    std::uint32_t state = 0;
    rtc_cntl_->read(0x38, 4, state);
    rtc_cntl_->write(0x38, 4, (state & ~0x3Fu) | reason);
    cpu_.reset();
    for (std::size_t page = 0; page < 128; ++page) {
        mmu_[page] = 0x100; // invalid until the ROM maps the flash
    }
    std::fill(cache_window_, cache_window_ + cache_window_size, std::uint8_t{0xFF});
    cpu_.invalidate_code_cache();
    halt_reported_ = false;
    vendor_csrs_.clear();
    base_ns_ = time_ns();
    base_cycles_ = cpu_.cycles();
    cpu_hz_ = 40'000'000;
    pccr_base_ = upccr_base_ = cpu_.cycles();
}

std::uint64_t Esp32C3::time_ns() const
{
    const std::uint64_t cycles = cpu_.cycles() - base_cycles_;
    return base_ns_ + cycles / cpu_hz_ * 1'000'000'000ull + cycles % cpu_hz_ * 1'000'000'000ull / cpu_hz_;
}

void Esp32C3::enter_sleep(bool deep, std::uint64_t wake_ns)
{
    asleep_ = true;
    deep_sleep_ = deep;
    wake_ns_ = wake_ns;
    sleep_start_ns_ = time_ns();
    cpu_.request_stop();
}

void Esp32C3::wake()
{
    asleep_ = false;
    const std::uint64_t slept = time_ns() - sleep_start_ns_;
    if (rtc_pauses_()) {
        rtc_timed_->pause(slept);
    }
    rtc_wake_(!deep_sleep_);
    if (deep_sleep_) {
        reset(ResetKind::DeepSleep, 0x5);
        return;
    }
    for (Timed* timed : timed_) {
        if (timed != rtc_timed_) {
            timed->pause(slept); // the digital timers stood still
        }
    }
}

void Esp32C3::set_cpu_hz(std::uint32_t hz)
{
    if (hz == cpu_hz_ || hz == 0) {
        return;
    }
    base_ns_ = time_ns();
    base_cycles_ = cpu_.cycles();
    cpu_hz_ = hz;
    cpu_.request_stop(); // the run loop converts its time limit again
}

std::uint64_t Esp32C3::cycles_for_ns(std::uint64_t ns) const
{
    return ns / 1'000'000'000ull * cpu_hz_ + (ns % 1'000'000'000ull * cpu_hz_ + 999'999'999ull) / 1'000'000'000ull;
}

void Esp32C3::run_timers()
{
    for (;;) {
        const std::uint64_t now = time_ns();
        Timed* due = nullptr;
        for (Timed* timed : timed_) {
            if (timed->next_ns() <= now) {
                due = timed;
                break;
            }
        }
        if (!due) {
            return;
        }
        due->on_ns(now);
    }
}

void Esp32C3::set_chip_enable(bool on)
{
    if (on == !held_) {
        return;
    }
    held_ = !on;
    if (on) {
        request_reset(ResetKind::PowerOn); // EN rose: the chip starts
    } else {
        cpu_.request_stop();
    }
}

void Esp32C3::set_uart_pin_rx(int port, bool on, std::uint64_t min_bit_ns)
{
    if (port == 0 || port == 1) {
        static_cast<Uart*>(uart_[port])->set_pin_rx(on, min_bit_ns);
    }
}

std::uint64_t Esp32C3::flash_writes() const
{
    return flash_chip_->writes();
}

void Esp32C3::run_until_ns(std::uint64_t ns)
{
    if (held_) { // EN low: nothing runs, time goes on
        const std::uint64_t now = time_ns();
        if (ns > now) {
            cpu_.advance_idle(cpu_.cycles() + cycles_for_ns(ns - now));
        }
        return;
    }
    while (time_ns() < ns && cpu_.state() != RvState::Halted) {
        if (reset_requested_) {
            reset(requested_kind_, requested_reason_);
        }
        if (asleep_) {
            // Only the RTC domain runs: its watchdog (unless it pauses in
            // sleep), and the wake-up alarm.
            const bool rtc_runs = !rtc_pauses_();
            std::uint64_t until = std::min(ns, wake_ns_);
            if (rtc_runs) {
                until = std::min(until, rtc_timed_->next_ns());
            }
            const std::uint64_t now = time_ns();
            if (until > now) {
                cpu_.advance_idle(cpu_.cycles() + cycles_for_ns(until - now));
            }
            if (rtc_runs && rtc_timed_->next_ns() <= time_ns()) {
                rtc_timed_->on_ns(time_ns());
            }
            if (time_ns() >= wake_ns_) {
                wake();
            }
            continue;
        }
        run_timers();
        std::uint64_t until = ns;
        for (Timed* timed : timed_) {
            until = std::min(until, timed->next_ns());
        }
        const std::uint64_t now = time_ns();
        const std::uint64_t cycles = until > now ? cycles_for_ns(until - now) : 1;
        cpu_.run(cpu_.cycles() + std::max<std::uint64_t>(cycles, 1));
    }
    if (reset_requested_) {
        reset(requested_kind_, requested_reason_);
    }
    if (!asleep_) {
        run_timers(); // asleep, only the RTC domain runs (the loop above)
    }
    if (cpu_.state() == RvState::Halted && !halt_reported_) {
        halt_reported_ = true;
        diagnostic(cpu_.halt_reason());
    }
}

void Esp32C3::output_byte(int port, std::uint8_t value)
{
    output_.push_back({time_ns(), port, value});
}

void Esp32C3::diagnostic(const std::string& text)
{
    diagnostics_.push_back(text);
}

void Esp32C3::send(int port, const std::string& bytes)
{
    Block* target = port == 2 ? usb_ : port == 0 ? uart_[0] : port == 1 ? uart_[1] : nullptr;
    if (target) {
        for (const char c : bytes) {
            target->receive(static_cast<std::uint8_t>(c));
        }
    }
}

Esp32C3::PinState Esp32C3::pin(int gpio) const
{
    return gpio >= 0 && gpio < gpio_count ? gpio_->state(gpio) : PinState{};
}

std::vector<Esp32C3::PinEvent> Esp32C3::take_pin_events()
{
    std::vector<PinEvent> out;
    out.swap(pin_events_);
    return out;
}

void Esp32C3::set_pin_voltage(int gpio, std::optional<double> volts)
{
    if (gpio >= 0 && gpio < gpio_count) {
        volts_[static_cast<std::size_t>(gpio)] = volts;
    }
}

std::optional<double> Esp32C3::pin_voltage(int gpio) const
{
    if (gpio < 0 || gpio >= gpio_count) {
        return std::nullopt;
    }
    if (volts_[static_cast<std::size_t>(gpio)]) {
        return volts_[static_cast<std::size_t>(gpio)];
    }
    return analog_source_ ? analog_source_(gpio, time_ns()) : std::nullopt;
}

bool Esp32C3::pad_level(int gpio) const
{
    return gpio >= 0 && gpio < gpio_count && gpio_->pad(gpio);
}

void Esp32C3::set_pin_input(int gpio, std::optional<bool> level)
{
    if (gpio >= 0 && gpio < gpio_count) {
        gpio_->set_input(gpio, level);
    }
}

std::vector<Esp32C3::Output> Esp32C3::take_output()
{
    std::vector<Output> out;
    out.swap(output_);
    return out;
}

std::vector<std::string> Esp32C3::take_diagnostics()
{
    std::vector<std::string> out;
    out.swap(diagnostics_);
    return out;
}

bool Esp32C3::vendor_csr(std::uint16_t csr)
{
    return csr == 0x000 || (csr >= 0x7C0 && csr <= 0x7FF) || (csr >= 0x800 && csr <= 0x8FF) || (csr >= 0xBC0 && csr <= 0xBFF)
        || (csr >= 0xCC0 && csr <= 0xCFF);
}

void Esp32C3::trace_csr(std::uint16_t csr, bool write, std::uint32_t value)
{
    if (!trace_) {
        return;
    }
    char text[96];
    std::snprintf(text, sizeof text, "CSR 0x%03X        %s %s %s  pc %s", static_cast<unsigned>(csr), write ? "write" : "read ",
        write ? "<-" : "->", hex(value).c_str(), hex(cpu_.pc()).c_str());
    trace_(text);
}

void Esp32C3::trace_access(const Block& block, std::uint32_t offset, bool write, std::uint32_t value)
{
    if (!trace_) {
        return;
    }
    char text[160];
    std::snprintf(text, sizeof text, "%-16s %s +0x%03X %s %s  pc %s", block.name(), write ? "write" : "read ",
        static_cast<unsigned>(offset), write ? "<-" : "->", hex(value).c_str(), hex(cpu_.pc()).c_str());
    const std::string where = function_at(cpu_.pc());
    trace_(where.empty() ? std::string(text) : std::string(text) + " (" + where + ")");
}

} // namespace simnodus::emu
