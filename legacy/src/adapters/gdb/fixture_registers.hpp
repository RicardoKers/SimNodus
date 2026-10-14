// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>
namespace simnodus::gdb {
inline bool parse_fixture_registers(std::string_view text, std::array<std::uint32_t, 3>& result, std::uint64_t token = 93) noexcept
{
    auto consume = [&](std::string_view prefix) {
        if(!text.starts_with(prefix)) return false;
        text.remove_prefix(prefix.size()); return true;
    };
    char token_buffer[21];
    const auto [token_end, token_error] = std::to_chars(token_buffer, token_buffer + sizeof(token_buffer), token);
    if(token_error != std::errc{}
        || !consume(std::string_view(token_buffer, static_cast<std::size_t>(token_end-token_buffer)))
        || !consume("^done,register-values=[")) return false;
    constexpr std::array<std::string_view, 3> fields{
        R"({number="0",value="0x)", R"(,{number="13",value="0x)", R"(,{number="14",value="0x)"};
    std::array<std::uint32_t, 3> parsed{};
    for(std::size_t i = 0; i < fields.size(); ++i) {
        if(!consume(fields[i])) return false;
        const auto end_quote = text.find('"');
        if(end_quote == 0 || end_quote > 8) return false;
        const auto [end, error] = std::from_chars(text.data(), text.data()+end_quote, parsed[i], 16);
        if(error != std::errc{} || end != text.data()+end_quote) return false;
        text.remove_prefix(end_quote);
        if(!consume("\"}")) return false;
    }
    if(text != "]") return false;
    result = parsed;
    return true;
}
} // namespace simnodus::gdb
