// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/ngspice/passive_source.hpp"
#include <algorithm>
#include <new>
#include <locale>
#include <set>
#include <sstream>

namespace simnodus::spice {
namespace {
std::string lower(std::string value)
{
    for(auto& c : value) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
bool identifier(std::string_view value)
{
    const auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    return !value.empty() && value.size() <= 64 && alpha(value.front())
        && std::all_of(value.begin(), value.end(), [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); });
}
bool literal(std::string_view value)
{
    if(value.empty() || value.size() > 64) return false;
    std::size_t i = 0, digits = 0;
    bool nonzero = false;
    auto scan = [&] {
        while(i < value.size() && value[i] >= '0' && value[i] <= '9') {
            nonzero |= value[i] != '0'; ++digits; ++i;
        }
    };
    scan();
    if(i < value.size() && value[i] == '.') { ++i; scan(); }
    if(digits == 0 || digits > 16 || !nonzero) return false;
    if(i < value.size() && (value[i] == 'e' || value[i] == 'E')) {
        ++i;
        if(i < value.size() && (value[i] == '+' || value[i] == '-')) ++i;
        const auto start = i;
        unsigned exponent = 0;
        while(i < value.size() && value[i] >= '0' && value[i] <= '9') {
            exponent = exponent * 10 + static_cast<unsigned>(value[i++] - '0');
            if(i - start > 2 || exponent > 18) return false;
        }
        if(i == start) return false;
    }
    const auto suffix = lower(std::string(value.substr(i)));
    return suffix.empty() || suffix == "t" || suffix == "g" || suffix == "meg" || suffix == "k"
        || suffix == "m" || suffix == "u" || suffix == "n" || suffix == "p" || suffix == "f";
}
void require(bool condition, const char* code, std::size_t offset)
{
    if(!condition) throw PassiveSourceError{code, offset};
}
}
PassiveSourceResult inspect_passive_source(std::string_view bytes)
{
    try {
        require(bytes.size() <= passive_source_max_bytes, "bytes", 0);
        PassiveSource result{std::string(bytes), {}};
        std::set<std::string> names;
        PassiveSubcircuit current{};
        unsigned state = 0;
        std::size_t lines = 0;
        for(std::size_t start = 0; start < result.bytes.size();) {
            const auto next = result.bytes.find('\n', start);
            const auto end = next == std::string::npos ? result.bytes.size() : next;
            require(++lines <= 1024 && end - start <= 1024, "budget", start);
            auto line = result.bytes.substr(start, end - start);
            if(!line.empty() && line.back() == '\r') line.pop_back();
            require(std::all_of(line.begin(), line.end(), [](unsigned char c) { return c == '\t' || (c >= 32 && c < 127); }), "encoding", start);
            const auto content = line.find_first_not_of(" \t");
            if(content != std::string::npos && line[content] != '*') {
                std::istringstream input(line);
                input.imbue(std::locale::classic());
                std::vector<std::string> words;
                for(std::string word; input >> word;) words.push_back(std::move(word));
                if(state == 0) {
                    require(words.size() == 5 && lower(words[0]) == ".subckt", "header", start);
                    require(result.subcircuits.size() < 32, "budget", start);
                    require(identifier(words[1]) && names.insert(lower(words[1])).second, "name", start);
                    require(identifier(words[2]) && identifier(words[3]) && lower(words[2]) != lower(words[3]), "terminals", start);
                    const auto equals = words[4].find('=');
                    require(equals != std::string::npos, "parameter", start);
                    const auto parameter = words[4].substr(0, equals), value = words[4].substr(equals + 1);
                    const auto key = lower(parameter);
                    require(identifier(parameter) && key != "m" && key != "temp" && key != "temper"
                        && key != "time" && key != "hertz" && key != "pi" && key != "e" && literal(value), "parameter", start);
                    current = {words[1], {words[2], words[3]}, parameter, value, PassiveKind::resistor, start, 0};
                    state = 1;
                } else if(state == 1) {
                    require(words.size() == 4 && identifier(words[0]), "body", start);
                    const auto kind = lower(words[0])[0];
                    require((kind == 'r' || kind == 'c') && lower(words[1]) == lower(current.terminals[0])
                        && lower(words[2]) == lower(current.terminals[1])
                        && lower(words[3]) == "{" + lower(current.parameter) + "}", "body", start);
                    current.kind = kind == 'r' ? PassiveKind::resistor : PassiveKind::capacitor;
                    state = 2;
                } else {
                    require(words.size() == 2 && lower(words[0]) == ".ends" && lower(words[1]) == lower(current.name), "end", start);
                    current.end = end;
                    result.subcircuits.push_back(std::move(current));
                    state = 0;
                }
            }
            start = next == std::string::npos ? result.bytes.size() : next + 1;
        }
        require(state == 0 && !result.subcircuits.empty(), "incomplete", result.bytes.size());
        return std::make_shared<const PassiveSource>(std::move(result));
    } catch(const PassiveSourceError& error) { return error; }
    catch(const std::bad_alloc&) { return PassiveSourceError{"memory", 0}; }
}
}
