// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/backend_contracts.hpp"
#include <charconv>
#include <string_view>
namespace simnodus::gdb {
inline bool parse_fixture_time(std::string_view stream, std::string_view completion,
                               Nanoseconds token, Nanoseconds& result) noexcept
{
    char buffer[21];
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), token);
    if(error != std::errc{}) return false;
    const std::string_view digits(buffer, static_cast<std::size_t>(end - buffer));
    if(!completion.starts_with(digits) || completion.substr(digits.size()) != "^done") return false;
    constexpr std::string_view prefix = R"(@"[Domain = Elapsed Virtual Time: )";
    constexpr std::string_view suffix = R"(\r\n")";
    if(stream.size() != prefix.size() + 18 + suffix.size()
        || !stream.starts_with(prefix) || !stream.ends_with(suffix)) return false;
    const auto value = stream.substr(prefix.size(), 18);
    if(value[2] != ':' || value[5] != ':' || value[8] != '.') return false;
    for(std::size_t i = 0; i < value.size(); ++i) {
        if(i == 2 || i == 5 || i == 8) continue;
        if(value[i] < '0' || value[i] > '9') return false;
    }
    const auto pair = [&](std::size_t i) { return static_cast<Nanoseconds>((value[i]-'0')*10 + value[i+1]-'0'); };
    const auto hours = pair(0), minutes = pair(3), seconds = pair(6);
    if(minutes >= 60 || seconds >= 60) return false;
    Nanoseconds fraction = 0;
    for(std::size_t i = 9; i < 18; ++i) fraction = fraction * 10 + static_cast<Nanoseconds>(value[i]-'0');
    result = ((hours * 60 + minutes) * 60 + seconds) * 1'000'000'000 + fraction;
    return true;
}
} // namespace simnodus::gdb
