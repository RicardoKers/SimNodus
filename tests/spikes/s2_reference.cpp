// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Spike S2 (SN-104): run reference netlists through a run-time loaded ngspice
// and write the probed vectors, resampled on a uniform grid, as JSON so that
// results of different platforms and ngspice versions can be compared.
//
// usage: s2_reference <library> <scripts-dir> <output.json> <netlist.cir>...
#include "sim/ngspice_runtime.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace simnodus::sim;

namespace {

constexpr int grid_points = 501;

struct Netlist {
    std::string name;
    std::vector<std::string> lines;
    std::vector<std::string> probes;
};

Netlist read_netlist(const std::filesystem::path& file)
{
    Netlist result{file.stem().string(), {}, {}};
    std::ifstream input(file);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("* probes:", 0) == 0) {
            std::istringstream names(line.substr(9));
            std::string name;
            while (names >> name) {
                result.probes.push_back(name);
            }
        }
        result.lines.push_back(line);
    }
    return result;
}

double interpolate(const std::vector<double>& t, const std::vector<double>& v, double at)
{
    if (at <= t.front()) {
        return v.front();
    }
    if (at >= t.back()) {
        return v.back();
    }
    std::size_t lo = 0;
    std::size_t upper = t.size() - 1;
    // Binary search for the first sample at or after `at`.
    while (lo + 1 < upper) {
        const std::size_t mid = (lo + upper) / 2;
        (t[mid] < at ? lo : upper) = mid;
    }
    const std::size_t hi = upper;
    const double span = t[hi] - t[lo];
    return span > 0.0 ? v[lo] + (v[hi] - v[lo]) * (at - t[lo]) / span : v[hi];
}

std::string number(double value)
{
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.12g", value);
    return buffer;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5) {
        std::fprintf(stderr, "usage: s2_reference <library> <scripts-dir> <output.json> <netlist.cir>...\n");
        return 2;
    }
    std::string error;
    const auto load_start = std::chrono::steady_clock::now();
    auto session = NgspiceSession::open({argv[1], argv[2]}, error);
    const double load_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - load_start).count();
    if (!session) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::ostringstream json;
#ifdef _WIN32
    const char* platform = "windows";
#else
    const char* platform = "linux";
#endif
    json << "{\n  \"spike\": \"S2\",\n  \"platform\": \"" << platform << "\",\n  \"ngspice_version\": "
         << (session->version() ? std::to_string(*session->version()) : "null")
         << ",\n  \"load_seconds\": " << number(load_seconds) << ",\n  \"grid_points\": " << grid_points
         << ",\n  \"circuits\": {";
    int failures = 0;
    for (int i = 4; i < argc; ++i) {
        const auto netlist = read_netlist(argv[i]);
        const auto start = std::chrono::steady_clock::now();
        const bool ok = session->load_circuit(netlist.lines) && session->command("run");
        const double run_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const auto time = session->vector("time");
        if (!ok || time.size() < 2) {
            std::fprintf(stderr, "%s: run failed\n", netlist.name.c_str());
            for (const auto& line : session->take_output()) {
                std::fprintf(stderr, "  %s\n", line.c_str());
            }
            ++failures;
            continue;
        }
        json << (i > 4 ? "," : "") << "\n    \"" << netlist.name << "\": {\n      \"accepted_points\": "
             << time.size() << ",\n      \"run_seconds\": " << number(run_seconds) << ",\n      \"stop_time\": "
             << number(time.back()) << ",\n      \"vectors\": {";
        for (std::size_t p = 0; p < netlist.probes.size(); ++p) {
            const auto values = session->vector(netlist.probes[p]);
            json << (p ? "," : "") << "\n        \"" << netlist.probes[p] << "\": [";
            if (values.size() != time.size()) {
                std::fprintf(stderr, "%s: vector %s missing\n", netlist.name.c_str(), netlist.probes[p].c_str());
                ++failures;
            } else {
                for (int g = 0; g < grid_points; ++g) {
                    const double at = time.back() * g / (grid_points - 1);
                    json << (g ? "," : "") << number(interpolate(time, values, at));
                }
            }
            json << "]";
        }
        json << "\n      }\n    }";
        std::printf("%s: %zu points in %.3f s\n", netlist.name.c_str(), time.size(), run_seconds);
        session->command("destroy all");
        session->command("remcirc");
        session->take_output();
    }
    json << "\n  }\n}\n";
    std::ofstream(argv[3], std::ios::binary) << json.str();
    std::printf("ngspice-%d, %d failure(s), wrote %s\n", session->version().value_or(0), failures, argv[3]);
    return failures == 0 ? 0 : 1;
}
