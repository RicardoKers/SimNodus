// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "esp32c3.hpp"

#include "esp32c3_rom.hpp"
#include "sha.hpp"
#include "timed_device.hpp"

#include <algorithm>
#include <array>
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
    void request_reset(ResetKind kind) { machine_.request_reset(kind); }
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
        }
        wel_ = false;
    }

    std::vector<std::uint8_t>& memory_;
    std::uint16_t status_ = 0;
    bool wel_ = false;
    bool written_ = false;
};

namespace {

// UART: the transmit FIFO empties at once (no baud-rate timing yet).
class Uart : public Esp32C3::Block {
public:
    Uart(Esp32C3& machine, const char* name, int port) : Block(machine, name), port_(port) {}

protected:
    std::uint32_t read_word(std::uint32_t offset) override
    {
        if (offset == 0x1C) { // UART_STATUS_REG: both FIFOs empty
            return 0;
        }
        return Block::read_word(offset);
    }
    void write_word(std::uint32_t offset, std::uint32_t value) override;

private:
    int port_;
};

// USB-Serial-JTAG: the IN endpoint always has room (a host that reads at once).
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

class Gpio : public Esp32C3::Block {
public:
    Gpio(Esp32C3& machine, std::uint32_t strapping) : Block(machine, "GPIO"), strapping_(strapping) {}

protected:
    void on_reset() override { reg(0x38) = strapping_; } // GPIO_STRAP_REG

private:
    std::uint32_t strapping_;
};

class RtcCntl : public Esp32C3::Block {
public:
    explicit RtcCntl(Esp32C3& machine) : Block(machine, "RTC_CNTL", 0x800) {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
        if (offset == 0x000) { // RTC_CNTL_OPTIONS0: SW_SYS_RST (bit 31), SW_PROCPU_RST (bit 5)
            if (value & (1u << 31)) {
                request_reset(Esp32C3::ResetKind::System);
            } else if (value & (1u << 5)) {
                request_reset(Esp32C3::ResetKind::Cpu);
            }
            value &= ~((1u << 31) | (1u << 5));
        }
        Block::write_word(offset, value);
    }
    void on_reset() override { reg(0x38) = 1; } // RTC_CNTL_RESET_STATE_REG: power-on reset
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
            if (!has_command) {
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
class Timg : public Esp32C3::Block {
public:
    Timg(Esp32C3& machine, const char* name) : Block(machine, name) {}

protected:
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
        Block::write_word(offset, offset == 0x68 ? value & ~(1u << 15) : value);
    }
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
// CPU_PER_CONF (80 or 160 MHz from the PLL).
class System : public Esp32C3::Block {
public:
    explicit System(Esp32C3& machine) : Block(machine, "SYSTEM") {}

protected:
    void write_word(std::uint32_t offset, std::uint32_t value) override
    {
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

void Uart::write_word(std::uint32_t offset, std::uint32_t value)
{
    if (offset == 0x0) { // UART_FIFO_REG
        emit(port_, static_cast<std::uint8_t>(value));
        return;
    }
    Block::write_word(offset, value);
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

    attach(uart0_base, std::make_unique<Uart>(*this, "UART0", 0));
    attach(uart1_base, std::make_unique<Uart>(*this, "UART1", 1));
    attach(gpio_base, std::make_unique<Gpio>(*this, options.strapping));
    auto rtc_cntl = std::make_unique<RtcCntl>(*this);
    rtc_cntl_ = rtc_cntl.get();
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
    attach(0x6001F000u, std::make_unique<Timg>(*this, "TIMG0"));
    attach(0x60020000u, std::make_unique<Timg>(*this, "TIMG1"));
    // Every other peripheral: registers that read back what was written.
    struct Plain {
        std::uint32_t base;
        const char* name;
    };
    static constexpr Plain plain[] = {
        {0x60005000u, "FE2"}, {0x60006000u, "FE"},
        {0x60009000u, "IO_MUX"}, {0x6000E000u, "RTC_I2C"}, {0x60013000u, "I2C0"}, {0x60014000u, "UHCI0"},
        {0x60016000u, "RMT"}, {0x60019000u, "LEDC"}, {0x6001C000u, "NRX"}, {0x6001D000u, "BB"},
         {0x60024000u, "SPI2"},
        {0x6002B000u, "TWAI"}, {0x6002D000u, "I2S"}, {0x6003A000u, "AES"},
        {0x6003C000u, "RSA"}, {0x6003D000u, "DS"}, {0x6003E000u, "HMAC"}, {0x6003F000u, "GDMA"},
        {0x60040000u, "APB_SARADC"}, {0x600C1000u, "SENSITIVE"},
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

void Esp32C3::request_reset(ResetKind kind)
{
    reset_requested_ = true;
    requested_kind_ = kind;
    cpu_.request_stop();
}

void Esp32C3::reset(ResetKind kind)
{
    reset_requested_ = false;
    ++resets_;
    for (const auto& block : blocks_) {
        if (kind == ResetKind::PowerOn || block.get() != rtc_cntl_) {
            block->reset(); // the RTC domain keeps its state except at power-on
        }
    }
    if (kind == ResetKind::PowerOn) {
        for (std::uint32_t base : {sram1_ibus, sram0_ibus, rtc_fast}) {
            const std::uint32_t size = base == sram1_ibus ? sram1_size : base == sram0_ibus ? sram0_size : rtc_fast_size;
            bus_.fill(base, 0, size);
        }
    }
    // RTC_CNTL_RESET_STATE_REG: RESET_CAUSE_PROCPU
    std::uint32_t reason = 0;
    rtc_cntl_->read(0x38, 4, reason);
    reason = (reason & ~0x3Fu) | (kind == ResetKind::PowerOn ? 0x1u : kind == ResetKind::System ? 0x3u : 0xCu);
    rtc_cntl_->write(0x38, 4, reason);
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

void Esp32C3::run_until_ns(std::uint64_t ns)
{
    while (time_ns() < ns && cpu_.state() != RvState::Halted) {
        if (reset_requested_) {
            reset(requested_kind_);
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
        reset(requested_kind_);
    }
    run_timers();
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
    Block* target = port == 2 ? usb_ : nullptr;
    if (target) {
        for (const char c : bytes) {
            target->receive(static_cast<std::uint8_t>(c));
        }
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
