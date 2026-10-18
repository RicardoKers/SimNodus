// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "vcd.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

namespace simnodus::sim {

namespace {

// Short identifier codes from the printable ASCII range ('!' to '~').
std::string identifier(std::size_t index)
{
    std::string code;
    do {
        code += static_cast<char>('!' + index % 94);
        index /= 94;
    } while (index > 0);
    return code;
}

// Keywords such as "$end" must not appear inside comment text.
std::string comment_text(std::string text)
{
    std::replace(text.begin(), text.end(), '$', ' ');
    std::replace(text.begin(), text.end(), '\n', ' ');
    return text;
}

} // namespace

std::string vcd_name(const std::string& name)
{
    std::string result;
    for (const char c : name) {
        const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (plain) {
            result += c;
        } else if (!result.empty() && result.back() != '_') {
            result += '_';
        }
    }
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    return result.empty() ? std::string("signal") : result;
}

std::string write_vcd(const std::vector<VcdSignal>& lanes, std::uint64_t start_ns, const std::string& version,
    const std::string& comment)
{
    std::ostringstream out;
    out << "$version " << comment_text(version) << " $end\n";
    if (!comment.empty()) {
        out << "$comment " << comment_text(comment) << " $end\n";
    }
    out << "$timescale 1 ns $end\n";
    out << "$scope module simnodus $end\n";
    std::set<std::string> used;
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        const std::string base = vcd_name(lanes[i].name);
        std::string name = base;
        for (int n = 2; used.count(name); ++n) {
            name = base + "_" + std::to_string(n);
        }
        used.insert(name);
        out << "$var wire 1 " << identifier(i) << " " << name << " $end\n";
    }
    out << "$upscope $end\n$enddefinitions $end\n";

    std::vector<bool> level(lanes.size());
    std::map<std::uint64_t, std::vector<std::pair<std::size_t, bool>>> later; // time -> (signal, level)
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        level[i] = lanes[i].initial;
        for (const auto& [time, value] : lanes[i].changes) {
            if (time <= start_ns) {
                level[i] = value;
            } else {
                later[time].emplace_back(i, value);
            }
        }
    }
    out << "#" << start_ns << "\n$dumpvars\n";
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        out << (level[i] ? '1' : '0') << identifier(i) << "\n";
    }
    out << "$end\n";
    for (const auto& [time, changes] : later) {
        // The last change of a signal at this time wins; unchanged levels are skipped.
        std::map<std::size_t, bool> final_level;
        for (const auto& [i, value] : changes) {
            final_level[i] = value;
        }
        std::string lines;
        for (const auto& [i, value] : final_level) {
            if (value != level[i]) {
                level[i] = value;
                lines += (value ? '1' : '0') + identifier(i) + "\n";
            }
        }
        if (!lines.empty()) {
            out << "#" << time << "\n" << lines;
        }
    }
    return out.str();
}

} // namespace simnodus::sim
