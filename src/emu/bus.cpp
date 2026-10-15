// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "bus.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace simnodus::emu {

void Bus::add_memory(const std::string& name, std::uint32_t base, std::uint32_t size, bool writable)
{
    auto storage = std::make_unique<Storage>();
    storage->name = name;
    storage->bytes.assign(size, 0);
    Region region;
    region.base = base;
    region.size = size;
    region.storage = storage.get();
    region.writable = writable;
    storages_.push_back(std::move(storage));
    regions_.push_back(region);
    std::sort(regions_.begin(), regions_.end(), [](const Region& a, const Region& b) { return a.base < b.base; });
}

void Bus::add_alias(const std::string& name, std::uint32_t base, const std::string& target)
{
    for (const auto& storage : storages_) {
        if (storage->name == target) {
            Region region;
            region.base = base;
            region.size = static_cast<std::uint32_t>(storage->bytes.size());
            region.storage = storage.get();
            region.writable = false;
            for (const auto& r : regions_) {
                if (r.storage == storage.get()) {
                    region.writable = r.writable;
                }
            }
            regions_.push_back(region);
            std::sort(regions_.begin(), regions_.end(), [](const Region& a, const Region& b) { return a.base < b.base; });
            (void)name;
            return;
        }
    }
    throw std::invalid_argument("no memory named " + target);
}

void Bus::add_device(std::uint32_t base, std::uint32_t size, Device* device)
{
    Region region;
    region.base = base;
    region.size = size;
    region.device = device;
    regions_.push_back(region);
    std::sort(regions_.begin(), regions_.end(), [](const Region& a, const Region& b) { return a.base < b.base; });
}

void Bus::add_bit_band(std::uint32_t alias_base, std::uint32_t alias_size, std::uint32_t target_base)
{
    Region region;
    region.base = alias_base;
    region.size = alias_size;
    region.bit_band = true;
    region.bit_band_target = target_base;
    regions_.push_back(region);
    std::sort(regions_.begin(), regions_.end(), [](const Region& a, const Region& b) { return a.base < b.base; });
}

const Bus::Region* Bus::find(std::uint32_t address) const
{
    // Few regions (about 40): binary search on the sorted bases.
    auto it = std::upper_bound(regions_.begin(), regions_.end(), address,
        [](std::uint32_t a, const Region& r) { return a < r.base; });
    if (it == regions_.begin()) {
        return nullptr;
    }
    --it;
    if (address - it->base < it->size) {
        return &*it;
    }
    return nullptr;
}

bool Bus::read(std::uint32_t address, unsigned size, std::uint32_t& value)
{
    const Region* region = find(address);
    if (!region || address - region->base > region->size - size) {
        return false;
    }
    if (region->storage) {
        std::uint32_t v = 0;
        std::memcpy(&v, region->storage->bytes.data() + (address - region->base), size);
        value = v;
        return true;
    }
    if (region->device) {
        return region->device->read(address - region->base, size, value);
    }
    return bit_band_read(*region, address, size, value);
}

bool Bus::write(std::uint32_t address, unsigned size, std::uint32_t value)
{
    const Region* region = find(address);
    if (!region || address - region->base > region->size - size) {
        return false;
    }
    if (region->storage) {
        if (!region->writable) {
            return false;
        }
        std::memcpy(region->storage->bytes.data() + (address - region->base), &value, size);
        ++memory_writes_;
        return true;
    }
    if (region->device) {
        return region->device->write(address - region->base, size, value);
    }
    return bit_band_write(*region, address, value);
}

bool Bus::bit_band_read(const Region& region, std::uint32_t address, unsigned size, std::uint32_t& value)
{
    const std::uint32_t word = (address - region.base) >> 2;
    const std::uint32_t target = region.bit_band_target + (word >> 3);
    std::uint32_t byte = 0;
    if (!read(target, 1, byte)) {
        return false;
    }
    value = (byte >> (word & 7u)) & 1u;
    (void)size;
    return true;
}

bool Bus::bit_band_write(const Region& region, std::uint32_t address, std::uint32_t value)
{
    // A read-modify-write of the byte that holds the bit, as the bus matrix does.
    const std::uint32_t word = (address - region.base) >> 2;
    const std::uint32_t target = region.bit_band_target + (word >> 3);
    std::uint32_t byte = 0;
    if (!read(target, 1, byte)) {
        return false;
    }
    const std::uint32_t mask = 1u << (word & 7u);
    byte = (value & 1u) ? (byte | mask) : (byte & ~mask);
    return write(target, 1, byte);
}

bool Bus::load(std::uint32_t address, const std::uint8_t* data, std::uint32_t size)
{
    std::uint8_t* target = memory_at(address, size);
    if (!target) {
        return false;
    }
    std::memcpy(target, data, size);
    ++memory_writes_;
    return true;
}

bool Bus::fill(std::uint32_t address, std::uint8_t value, std::uint32_t size)
{
    std::uint8_t* target = memory_at(address, size);
    if (!target) {
        return false;
    }
    std::memset(target, value, size);
    ++memory_writes_;
    return true;
}

std::uint8_t* Bus::memory_at(std::uint32_t address, std::uint32_t size)
{
    const Region* region = find(address);
    if (!region || !region->storage || address - region->base > region->size - size) {
        return nullptr;
    }
    return region->storage->bytes.data() + (address - region->base);
}

std::string Bus::describe(std::uint32_t address) const
{
    const Region* region = find(address);
    if (!region) {
        return {};
    }
    if (region->storage) {
        return region->storage->name;
    }
    if (region->device) {
        return region->device->name();
    }
    return "bit-band alias";
}

void Bus::reset_devices()
{
    for (const auto& region : regions_) {
        if (region.device) {
            region.device->reset();
        }
    }
}

} // namespace simnodus::emu
