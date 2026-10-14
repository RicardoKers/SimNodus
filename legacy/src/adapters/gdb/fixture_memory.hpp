// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/backend_contracts.hpp"
#include <array>
#include <charconv>
#include <string_view>
namespace simnodus::gdb {
// Exact measured GDB MI memory result for the owned firmware mailbox.
inline bool parse_fixture_memory(std::string_view text, std::array<Nanoseconds, 8>& words, Nanoseconds expected_token = 91) noexcept
{
    char token_buffer[21];
    const auto [end, error] = std::to_chars(token_buffer, token_buffer + sizeof(token_buffer), expected_token);
    if(error != std::errc{}) return false;
    const std::string_view token(token_buffer, static_cast<std::size_t>(end - token_buffer));
    if(!text.starts_with(token)) return false;
    text.remove_prefix(token.size());
    constexpr std::string_view prefix = "^done,memory=[{begin=\"0x20000000\",offset=\"0x00000000\",end=\"0x20000020\",contents=\"";
    constexpr std::string_view suffix = "\"}]";
    if(text.size() != prefix.size() + 64 + suffix.size()
        || !text.starts_with(prefix) || !text.ends_with(suffix)) return false;
    auto nibble = [](char c) -> int {
        if(c >= '0' && c <= '9') return c - '0';
        if(c >= 'a' && c <= 'f') return c - 'a' + 10;
        if(c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::array<Nanoseconds, 8> parsed{};
    for(std::size_t i = 0; i < 32; ++i) {
        const int high = nibble(text[prefix.size() + i * 2]);
        const int low = nibble(text[prefix.size() + i * 2 + 1]);
        if(high < 0 || low < 0) return false;
        parsed[i / 4] |= static_cast<Nanoseconds>(high * 16 + low) << ((i % 4) * 8);
    }
    words = parsed;
    return true;
}
} // namespace simnodus::gdb
