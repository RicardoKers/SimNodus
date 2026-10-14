// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "engineering.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>

namespace simnodus::model {
namespace {

struct Multiplier {
    std::string_view text;
    double scale;
};

// Longest spellings first so that "meg" wins over "m".
constexpr std::array<Multiplier, 12> multipliers{{
    {"meg", 1e6},
    {"\xC2\xB5", 1e-6}, // micro sign (UTF-8)
    {"\xCE\xBC", 1e-6}, // Greek mu (UTF-8)
    {"f", 1e-15},
    {"p", 1e-12},
    {"n", 1e-9},
    {"u", 1e-6},
    {"m", 1e-3},
    {"k", 1e3},
    {"M", 1e6},
    {"G", 1e9},
    {"T", 1e12},
}};

std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Removes a trailing unit spelling; returns true if something was removed.
bool strip_unit(std::string_view& s, std::string_view unit)
{
    if (unit.empty()) {
        return false;
    }
    // Units are case-sensitive ("1f" with unit "F" stays femto), except the
    // spelled-out "ohm".
    std::array<std::string_view, 5> spellings{unit, {}, {}, {}, {}};
    const bool ohm = iequals(unit, "Ohm");
    if (ohm) {
        spellings = {"ohms", "ohm", "\xCE\xA9", "\xE2\x84\xA6", "R"}; // Ω (Greek), Ω (ohm sign)
    }
    for (const auto& spelling : spellings) {
        if (spelling.empty() || s.size() <= spelling.size()) {
            continue;
        }
        const auto tail = s.substr(s.size() - spelling.size());
        const bool spelled = ohm && spelling.size() > 2 && spelling.front() == 'o';
        if (spelled ? iequals(tail, spelling) : tail == spelling) {
            s.remove_suffix(spelling.size());
            return true;
        }
    }
    return false;
}

std::optional<double> parse_number(std::string_view s)
{
    if (s.empty()) {
        return std::nullopt;
    }
    // std::from_chars rejects a leading '+'.
    if (s.front() == '+') {
        s.remove_prefix(1);
    }
    double value = 0.0;
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (error != std::errc() || end != s.data() + s.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

// Locale-independent "%.*g" (Qt sets the C locale on Linux).
std::string to_text(double value, int digits)
{
    char buffer[64];
    const auto [end, status] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, digits);
    return status == std::errc() ? std::string(buffer, end) : std::string("nan");
}

} // namespace

std::optional<double> parse_engineering(std::string_view text, std::string_view unit)
{
    std::string_view s = trim(text);
    strip_unit(s, unit);
    s = trim(s);
    if (s.empty()) {
        return std::nullopt;
    }
    if (auto plain = parse_number(s)) {
        return plain;
    }
    // Find the first multiplier after the leading number characters.
    for (std::size_t i = 1; i < s.size(); ++i) {
        for (const auto& m : multipliers) {
            if (s.substr(i, m.text.size()) != m.text) {
                continue;
            }
            const auto head = s.substr(0, i);
            const auto tail = s.substr(i + m.text.size());
            if (tail.empty()) {
                if (auto v = parse_number(head)) {
                    return *v * m.scale;
                }
                return std::nullopt;
            }
            // "4k7": the multiplier replaces the decimal point.
            bool digits = true;
            for (char c : tail) {
                digits = digits && std::isdigit(static_cast<unsigned char>(c));
            }
            if (!digits || head.find_first_of(".eE") != std::string_view::npos) {
                return std::nullopt;
            }
            const std::string joined = std::string(head) + "." + std::string(tail);
            if (auto v = parse_number(joined)) {
                return *v * m.scale;
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::string format_engineering(double value, int digits)
{
    if (value == 0.0 || !std::isfinite(value)) {
        return value == 0.0 ? "0" : "nan";
    }
    static constexpr std::array<std::pair<double, const char*>, 9> steps{{
        {1e12, "T"}, {1e9, "G"}, {1e6, "M"}, {1e3, "k"}, {1.0, ""}, {1e-3, "m"}, {1e-6, "u"}, {1e-9, "n"},
        {1e-12, "p"},
    }};
    const double magnitude = std::abs(value);
    double scale = 1e-15;
    const char* suffix = "f";
    for (const auto& [s, name] : steps) {
        // Tolerate rounding just below a decade boundary (999.99999 -> 1k).
        if (magnitude >= s * (1.0 - 1e-12)) {
            scale = s;
            suffix = name;
            break;
        }
    }
    return to_text(value / scale, digits) + suffix;
}

std::string spice_number(double value)
{
    if (value == 0.0) {
        return "0"; // never "-0"
    }
    return to_text(value, 12);
}

} // namespace simnodus::model
