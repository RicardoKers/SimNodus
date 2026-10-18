// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu_runner.hpp"

#include <cctype>

namespace simnodus::mcu {

std::optional<GpioPin> gpio_pin(const std::string& name)
{
    if (name.size() < 3 || name[0] != 'P' || name[1] < 'A' || name[1] > 'G') {
        return std::nullopt;
    }
    int number = 0;
    for (std::size_t i = 2; i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
            return std::nullopt;
        }
        number = number * 10 + (name[i] - '0');
    }
    if (number > 15) {
        return std::nullopt;
    }
    return GpioPin{name[1], number};
}

bool schmitt(bool previous, double volts, const PinModel& model)
{
    if (volts >= model.input_high) {
        return true;
    }
    if (volts <= model.input_low) {
        return false;
    }
    return previous;
}

PinDrive pin_drive(const PinState& state, const PinModel& model)
{
    const double out = 1.0 / model.output_resistance;
    const double pull = 1.0 / model.pull_resistance;
    switch (state.mode) {
    case PinMode::Output:
    case PinMode::Alternate:
        if (state.open_drain) {
            return state.level ? PinDrive{} : PinDrive{out, 0.0};
        }
        return {out, state.level ? model.supply * out : 0.0};
    case PinMode::Input:
        if (state.pull == Pull::Up) {
            return {pull, model.supply * pull};
        }
        if (state.pull == Pull::Down) {
            return {pull, 0.0};
        }
        return {};
    case PinMode::Analog:
    case PinMode::Unknown:
        break;
    }
    return {};
}

} // namespace simnodus::mcu
