// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace simnodus::resource_policy {
inline bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool digit(char c) { return c >= '0' && c <= '9'; }
inline std::string lower(std::string value)
{
    for(auto& c : value) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
inline bool id(std::string_view value)
{
    return !value.empty() && value.size() <= 64 && value[0] >= 'a' && value[0] <= 'z'
        && std::all_of(value.begin(), value.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || digit(c) || c == '_' || c == '-'; });
}
inline bool reserved(std::string part)
{
    part = lower(part.substr(0, part.find('.')));
    return part == "con" || part == "prn" || part == "aux" || part == "nul"
        || part == "conin$" || part == "conout$"
        || (part.size() == 4 && (part.starts_with("com") || part.starts_with("lpt"))
            && part[3] >= '1' && part[3] <= '9');
}
inline bool path(std::string_view value)
{
    if(value.empty() || value.size() > 240) return false;
    std::size_t start = 0, count = 0;
    while(start <= value.size()) {
        auto end = value.find('/', start);
        if(end == value.npos) end = value.size();
        const auto part = value.substr(start, end - start);
        if(++count > 16 || part.empty() || part.size() > 80 || part.back() == '.'
            || reserved(std::string(part))) return false;
        auto first = [](char c) { return alpha(c) || digit(c) || c == '_' || c == '-'; };
        if(!first(part.front()) || !std::all_of(part.begin(), part.end(),
            [&](char c) { return first(c) || c == '.'; })) return false;
        if(end == value.size()) return true;
        start = end + 1;
    }
    return false;
}
inline bool root_syntax(const std::string& input)
{
    if(input.size() < 3 || input.size() > 4096 || !alpha(input[0]) || input[1] != ':'
        || (input[2] != '/' && input[2] != '\\')) return false;
    auto value = input;
    std::replace(value.begin(), value.end(), '\\', '/');
    if(value.size() == 3) return true;
    std::size_t start = 3, count = 0;
    while(start <= value.size()) {
        auto end = value.find('/', start);
        if(end == value.npos) end = value.size();
        auto part = value.substr(start, end - start);
        if(++count > 64 || part.empty() || part == "." || part == ".."
            || part.back() == '.' || part.back() == ' ' || reserved(part)) return false;
        for(unsigned char c : part)
            if(c < 32 || std::string_view(":~<>\"|?*").find(static_cast<char>(c)) != std::string_view::npos)
                return false;
        if(end == value.size()) return true;
        start = end + 1;
    }
    return false;
}
}
