// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "devices/device.hpp"

#include <algorithm>
#include <array>

namespace simnodus::devices {

namespace {

// Finds the indexes of named pins; false with `error` if one is missing.
template <std::size_t N>
bool find_pins(const std::vector<std::string>& pins, const std::array<const char*, N>& names,
    std::array<std::size_t, N>& index, const std::string& kind, std::string& error)
{
    for (std::size_t i = 0; i < N; ++i) {
        const auto it = std::find(pins.begin(), pins.end(), names[i]);
        if (it == pins.end()) {
            error = kind + ": pin " + names[i] + " is missing";
            return false;
        }
        index[i] = static_cast<std::size_t>(it - pins.begin());
    }
    return true;
}

// 74HC595 (NXP datasheet): 8-bit serial-in shift register with an output
// storage register. DS shifts in at rising SHCP (Q0 first), the storage
// register takes the shift register at rising STCP, MR low clears the shift
// register, OE high puts Q0-Q7 in high impedance. Q7S, the last stage, is
// always driven, for chaining.
class Hc595 : public Device {
public:
    static constexpr std::array<const char*, 14> names{
        "DS", "SHCP", "STCP", "MR", "OE", "Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7", "Q7S"};
    enum { DS, SHCP, STCP, MR, OE, Q0, Q7 = Q0 + 7, Q7S };

    bool init(const std::vector<std::string>& pins, std::string& error)
    {
        if (!find_pins(pins, names, index_, "74HC595", error)) {
            return false;
        }
        role_.assign(pins.size(), -1);
        for (std::size_t i = 0; i < names.size(); ++i) {
            role_[index_[i]] = static_cast<int>(i);
        }
        return true;
    }

    void set_input(std::size_t pin, bool level, std::uint64_t) override
    {
        if (pin >= role_.size() || role_[pin] < 0) {
            return;
        }
        const int role = role_[pin];
        const bool rising = level && !inputs_[static_cast<std::size_t>(role)];
        inputs_[static_cast<std::size_t>(role)] = level;
        switch (role) {
        case SHCP:
            if (rising && inputs_[MR]) {
                shift_ = static_cast<std::uint8_t>((shift_ << 1) | (inputs_[DS] ? 1u : 0u));
            }
            break;
        case STCP:
            if (rising) {
                storage_ = shift_;
            }
            break;
        case MR:
            if (!level) {
                shift_ = 0;
            }
            break;
        default: break;
        }
    }

    void initial_input(std::size_t pin, bool level) override
    {
        if (pin < role_.size() && role_[pin] >= 0) {
            inputs_[static_cast<std::size_t>(role_[pin])] = level;
        }
    }

    Output output(std::size_t pin) const override
    {
        if (pin >= role_.size()) {
            return {};
        }
        const int role = role_[pin];
        if (role >= Q0 && role <= Q7) {
            if (inputs_[OE]) {
                return {}; // OE high: high impedance
            }
            return {true, ((storage_ >> (role - Q0)) & 1u) != 0, false};
        }
        if (role == Q7S) {
            return {true, (shift_ & 0x80u) != 0, false};
        }
        return {};
    }

private:
    std::array<std::size_t, names.size()> index_{};
    std::vector<int> role_;
    std::array<bool, names.size()> inputs_{false, false, false, true, false};
    std::uint8_t shift_ = 0;
    std::uint8_t storage_ = 0;
};

} // namespace

std::unique_ptr<Device> make_device(const std::string& kind, const std::vector<std::string>& pins,
    const std::map<std::string, std::string>& parameters, std::string& error)
{
    (void)parameters;
    if (kind == "74hc595") {
        auto device = std::make_unique<Hc595>();
        if (!device->init(pins, error)) {
            return nullptr;
        }
        return device;
    }
    error = "unknown device kind \"" + kind + "\"";
    return nullptr;
}

} // namespace simnodus::devices
