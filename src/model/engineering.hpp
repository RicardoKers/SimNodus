// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Engineering notation for component values (UI-10).
//
// Accepted: plain numbers ("1000", "1e3", "-2.5"), a multiplier suffix
// ("4.7k", "100n", "2.2u", "2.2µ"), the multiplier as decimal point ("4k7",
// "2u2") and an optional unit after it ("4.7kOhm", "100nF", "5V", "10kΩ").
//
// Multipliers: f p n u/µ m k M/meg G T. Unlike SPICE, an upper-case "M" means
// mega, because that is what students write ("1M" resistor); "m" is milli and
// "meg" is accepted too. Netlists always receive plain exponent numbers, so
// the SPICE meaning of "M" never applies.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace simnodus::model {

// Parses `text`. `unit` (for example "Ohm", "F", "V") may follow the value;
// "Ohm" also accepts "ohm", "R" and "Ω". Returns nullopt for invalid input.
std::optional<double> parse_engineering(std::string_view text, std::string_view unit = {});

// Formats with up to `digits` significant digits and a multiplier suffix,
// for example 4700 -> "4.7k", 1e-7 -> "100n", 0 -> "0". No unit is added.
std::string format_engineering(double value, int digits = 4);

// Plain number for a SPICE netlist ("4700", "1e-07").
std::string spice_number(double value);

} // namespace simnodus::model
