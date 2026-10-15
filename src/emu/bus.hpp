// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// System bus of the SimNodus STM32 emulator (docs/architecture/MCU_EMULATOR.md,
// section 5): memories with direct storage, memory-mapped devices, and the
// Cortex-M3 bit-band aliases. An access that hits nothing fails, and the core
// turns that into a BusFault as the chip does.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace simnodus::emu {

// A memory-mapped peripheral. Offsets are relative to the device base;
// `size` is 1, 2 or 4 bytes. Returning false makes the access fault.
class Device {
public:
    virtual ~Device() = default;
    virtual const char* name() const = 0;
    virtual bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) = 0;
    virtual bool write(std::uint32_t offset, unsigned size, std::uint32_t value) = 0;
    virtual void reset() {}
};

class Bus {
public:
    // Plain memory. `writable` false: CPU writes fail (the loader may still write).
    // Several regions may share one storage (flash and its alias at 0).
    void add_memory(const std::string& name, std::uint32_t base, std::uint32_t size, bool writable);
    void add_alias(const std::string& name, std::uint32_t base, const std::string& target);
    void add_device(std::uint32_t base, std::uint32_t size, Device* device);
    // Bit-band alias of [target_base, target_base + size / 32): one word per bit.
    void add_bit_band(std::uint32_t alias_base, std::uint32_t alias_size, std::uint32_t target_base);

    // Plain memories are checked first, inline; devices and gaps go the slow way.
    bool read(std::uint32_t address, unsigned size, std::uint32_t& value)
    {
        for (int i = 0; i < fast_count_; ++i) {
            const Fast& f = fast_[static_cast<std::size_t>(i)];
            if (address - f.base <= f.size - size) {
                std::uint32_t v = 0;
                std::memcpy(&v, f.data + (address - f.base), size);
                value = v;
                return true;
            }
        }
        return read_slow(address, size, value);
    }
    bool write(std::uint32_t address, unsigned size, std::uint32_t value)
    {
        for (int i = 0; i < fast_count_; ++i) {
            const Fast& f = fast_[static_cast<std::size_t>(i)];
            if (f.writable && address - f.base <= f.size - size) {
                std::memcpy(f.data + (address - f.base), &value, size);
                ++memory_writes_;
                return true;
            }
        }
        return write_slow(address, size, value);
    }

    // Loader access to memories (ignores `writable`); false if not inside one memory.
    bool load(std::uint32_t address, const std::uint8_t* data, std::uint32_t size);
    bool fill(std::uint32_t address, std::uint8_t value, std::uint32_t size);
    // Direct pointer to `size` bytes of memory, or nullptr (devices, gaps).
    std::uint8_t* memory_at(std::uint32_t address, std::uint32_t size);
    // Name of what is mapped at `address`, or empty.
    std::string describe(std::uint32_t address) const;

    void reset_devices();

    // Increments on every write into a memory region; the core uses it to
    // invalidate decoded instructions of code in SRAM.
    std::uint64_t memory_writes() const { return memory_writes_; }

private:
    struct Storage {
        std::string name;
        std::vector<std::uint8_t> bytes;
    };
    struct Region {
        std::uint32_t base = 0;
        std::uint32_t size = 0;
        Storage* storage = nullptr; // memory, or
        Device* device = nullptr;   // device, or
        std::uint32_t bit_band_target = 0; // bit-band alias when storage and device are null
        bool writable = true;
        bool bit_band = false;
    };
    struct Fast {
        std::uint32_t base = 0;
        std::uint32_t size = 0;
        std::uint8_t* data = nullptr;
        bool writable = false;
    };
    bool read_slow(std::uint32_t address, unsigned size, std::uint32_t& value);
    bool write_slow(std::uint32_t address, unsigned size, std::uint32_t value);
    void add_fast(std::uint32_t base, std::uint32_t size, std::uint8_t* data, bool writable);
    const Region* find(std::uint32_t address) const;
    bool bit_band_read(const Region& region, std::uint32_t address, unsigned size, std::uint32_t& value);
    bool bit_band_write(const Region& region, std::uint32_t address, std::uint32_t value);

    std::vector<std::unique_ptr<Storage>> storages_;
    std::vector<Region> regions_; // sorted by base
    std::array<Fast, 4> fast_{};  // the first memories added (SRAM, flash, its alias)
    int fast_count_ = 0;
    std::uint64_t memory_writes_ = 0;
};

} // namespace simnodus::emu
