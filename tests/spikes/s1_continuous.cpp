// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Spike S1 (SN-103): continuous interactive ngspice run. See
// docs/experiments/S1-continuous-run.md for the questions and results.
//
// usage: s1_continuous <library> <scripts-dir> <output.json> <mode> [key=value...]
//   growth   tstep=1e-3 stop=1000 maxstep=1e-6 sim=4 save=all|probes interp=0|1
//            unpaced run; memory against simulated time
//   pacing   tstep=1e-3 stop=1000 maxstep=1e-4 speed=1 wall=10 save=probes
//            paced run; a "user" thread presses the button on a wall-clock
//            schedule; achieved speed, latency and memory
//   segments maxstep=1e-4 sim=6 segment=1 save=probes
//            deterministic button schedule; one continuous reference run, then
//            the same time span in restarted segments with capacitor initial
//            conditions; continuity error, restart cost and memory
//   soak     maxstep=1e-4 sim=120 segment=1 save=probes
//            long segmented run without stored traces; memory per segment
#include "sim/ngspice_runtime.hpp"
#include "sim/process_memory.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#endif

using namespace simnodus::sim;
using Clock = std::chrono::steady_clock;

namespace {

std::map<std::string, std::string> options;

double option(const std::string& key, double fallback)
{
    const auto found = options.find(key);
    return found == options.end() ? fallback : std::stod(found->second);
}

std::string text_option(const std::string& key, const std::string& fallback)
{
    const auto found = options.find(key);
    return found == options.end() ? fallback : found->second;
}

std::string number(double value)
{
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    return buffer;
}

double megabytes(std::uint64_t bytes)
{
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

// Push button charges C1 through R1; when released, C1 discharges through a
// red LED. tau(charge) = 10 ms. Only C1 carries state across segments.
std::vector<std::string> circuit(double tstep, double maxstep, double stop, double vc0, const std::string& save, bool interp)
{
    std::vector<std::string> lines = {
        "* S1 interactive circuit",
        "Vbtn btn 0 DC 0 external",
        "Vcc vcc 0 DC 3.3",
        "S1 vcc a btn 0 swbtn",
        ".model swbtn SW(VT=0.5 VH=0.1 RON=0.1 ROFF=1e9)",
        "R1 a out 1k",
        "C1 out 0 10u IC=" + number(vc0),
        "Rled out led 100",
        "Dled led 0 dred",
        ".model dred D(IS=1e-20 N=1.6 RS=2)",
        "Rleak out 0 100k",
    };
    if (save == "probes") {
        lines.push_back(".save out btn");
    }
    lines.push_back(std::string(".options reltol=1e-4") + (interp ? " interp" : ""));
    lines.push_back(".tran " + number(tstep) + " " + number(stop) + " 0 " + number(maxstep) + " uic");
    lines.push_back(".end");
    return lines;
}

int find_vector(const std::vector<std::string>& names, const std::string& node)
{
    for (std::size_t i = 0; i < names.size(); ++i) {
        std::string name = names[i];
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == node || name == "v(" + node + ")") {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Deterministic schedule used by the segment comparison: pressed for 50 ms
// every 200 ms, starting at 20 ms.
double scheduled_button(double t)
{
    const double phase = std::fmod(t - 0.02, 0.2);
    return t >= 0.02 && phase < 0.05 ? 1.0 : 0.0;
}

struct Trace {
    std::vector<double> t;
    std::vector<double> v;
};

double interpolate(const Trace& trace, double at)
{
    const auto it = std::lower_bound(trace.t.begin(), trace.t.end(), at);
    if (it == trace.t.begin()) {
        return trace.v.front();
    }
    if (it == trace.t.end()) {
        return trace.v.back();
    }
    const std::size_t hi = static_cast<std::size_t>(it - trace.t.begin());
    const std::size_t lo = hi - 1;
    const double span = trace.t[hi] - trace.t[lo];
    return span > 0 ? trace.v[lo] + (trace.v[hi] - trace.v[lo]) * (at - trace.t[lo]) / span : trace.v[hi];
}

struct MemoryPoint {
    double sim = 0;
    double wall = 0;
    ProcessMemory memory;
};

std::string memory_json(const std::vector<MemoryPoint>& points)
{
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < points.size(); ++i) {
        out << (i ? "," : "") << "[" << number(points[i].sim) << "," << number(points[i].wall) << ","
            << number(megabytes(points[i].memory.resident_bytes)) << ","
            << number(megabytes(points[i].memory.private_bytes)) << "]";
    }
    out << "]";
    return out.str();
}

// Least-squares slope of private MB against simulated seconds.
double slope(const std::vector<MemoryPoint>& points)
{
    if (points.size() < 2) {
        return 0;
    }
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (const auto& p : points) {
        const double y = megabytes(p.memory.private_bytes);
        sx += p.sim;
        sy += y;
        sxx += p.sim * p.sim;
        sxy += p.sim * y;
    }
    const double n = static_cast<double>(points.size());
    const double d = n * sxx - sx * sx;
    return d != 0 ? (n * sxy - sx * sy) / d : 0;
}

int growth(NgspiceSession& session, std::ostringstream& json)
{
    const double maxstep = option("maxstep", 1e-6);
    const double sim = option("sim", 4);
    const std::string save = text_option("save", "all");
    const bool interp = option("interp", 0) != 0;
    std::atomic<double> now{0};
    std::atomic<long long> points{0};
    session.set_source_handler([](const std::string&, double t) { return scheduled_button(t); });
    session.set_data_handler([&](const AcceptedPoint& p) {
        now = p.time;
        ++points;
    });
    const auto before = current_process_memory();
    const double tstep = option("tstep", 1e-3);
    const double stop = option("stop", 1000);
    if (!session.load_circuit(circuit(tstep, maxstep, stop, 0, save, interp)) || !session.start_background()) {
        return 1;
    }
    const auto start = Clock::now();
    std::vector<MemoryPoint> samples;
    double next = 0;
    while (now < sim && std::chrono::duration<double>(Clock::now() - start).count() < 300) {
        if (!session.background_running()) {
            std::fprintf(stderr, "background run ended at t = %g\n", now.load());
            return 1;
        }
        if (now >= next) {
            samples.push_back({now, std::chrono::duration<double>(Clock::now() - start).count(), current_process_memory()});
            next += sim / 40;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    session.halt_background(std::chrono::seconds(10));
    const double wall = std::chrono::duration<double>(Clock::now() - start).count();
    samples.push_back({now, wall, current_process_memory()});
    json << "\"tstep\": " << number(tstep) << ", \"stop\": " << number(stop) << ", \"maxstep\": " << number(maxstep) << ", \"save\": \"" << save << "\", \"interp\": " << interp
         << ", \"sim_seconds\": " << number(now) << ", \"wall_seconds\": " << number(wall)
         << ", \"accepted_points\": " << points << ", \"points_per_sim_second\": " << number(points / now)
         << ", \"speed_unpaced\": " << number(now / wall)
         << ", \"private_mb_before\": " << number(megabytes(before.private_bytes))
         << ", \"private_mb_per_sim_second\": " << number(slope(samples))
         << ", \"bytes_per_point\": " << number(slope(samples) * 1024 * 1024 / (points / now))
         << ", \"memory\": " << memory_json(samples);
    return 0;
}

int pacing(NgspiceSession& session, std::ostringstream& json)
{
    const double maxstep = option("maxstep", 1e-4);
    const double speed = option("speed", 1);
    const double wall_limit = option("wall", 10);
    const std::string save = text_option("save", "probes");
    // hires=1 raises the Windows timer resolution to 1 ms for the run.
    const bool hires = option("hires", 0) != 0;
#ifdef _WIN32
    if (hires) {
        timeBeginPeriod(1);
    }
#endif
    std::atomic<bool> pressed{false};
    std::atomic<double> now{0};
    std::atomic<long long> points{0};
    std::atomic<double> max_lag{0};
    std::atomic<double> sleep_total{0};
    std::atomic<double> max_oversleep{0};
    // Latency measurement: the user thread records when it changed the
    // button; the engine thread records the first accepted point that shows it.
    std::mutex latency_mutex;
    std::vector<double> wall_latency;
    std::vector<double> sim_latency;
    Clock::time_point changed_at{};
    double changed_sim = 0;
    bool waiting = false;
    double expected = 0;
    int btn_index = -2;

    Clock::time_point start;
    session.set_source_handler([&](const std::string&, double) { return pressed ? 1.0 : 0.0; });
    session.set_data_handler([&](const AcceptedPoint& p) {
        if (btn_index == -2) {
            btn_index = find_vector(session.vector_names(), "btn");
        }
        now = p.time;
        ++points;
        if (btn_index >= 0) {
            std::lock_guard lock(latency_mutex);
            if (waiting && p.values[static_cast<std::size_t>(btn_index)] == expected) {
                wall_latency.push_back(std::chrono::duration<double>(Clock::now() - changed_at).count());
                sim_latency.push_back(p.time - changed_sim);
                waiting = false;
            }
        }
        // Pace: never let simulated time run ahead of wall time x speed.
        const auto target = start + std::chrono::duration_cast<Clock::duration>(
                                        std::chrono::duration<double>(p.time / speed));
        const auto current = Clock::now();
        if (target > current + std::chrono::milliseconds(1)) {
            std::this_thread::sleep_until(target);
            const auto woke = Clock::now();
            sleep_total = sleep_total + std::chrono::duration<double>(woke - current).count();
            max_oversleep = std::max(max_oversleep.load(), std::chrono::duration<double>(woke - target).count());
        } else if (current > target) {
            max_lag = std::max(max_lag.load(), std::chrono::duration<double>(current - target).count());
        }
    });
    const double tstep = option("tstep", 1e-3);
    const double stop = option("stop", 1000);
    if (!session.load_circuit(circuit(tstep, maxstep, stop, 0, save, false))) {
        return 1;
    }
    start = Clock::now();
    if (!session.start_background()) {
        return 1;
    }
    std::vector<MemoryPoint> samples;
    int presses = 0;
    for (int tick = 0;; ++tick) {
        const double wall = std::chrono::duration<double>(Clock::now() - start).count();
        if (wall >= wall_limit) {
            break;
        }
        if (!session.background_running()) {
            std::fprintf(stderr, "background run ended at t = %g\n", now.load());
            return 1;
        }
        // Toggle every 250 ms of wall time, like a user tapping the button.
        if (tick % 25 == 12) {
            std::lock_guard lock(latency_mutex);
            pressed = !pressed;
            presses += pressed ? 1 : 0;
            changed_at = Clock::now();
            changed_sim = now;
            expected = pressed ? 1.0 : 0.0;
            waiting = true;
        }
        if (tick % 50 == 0) {
            samples.push_back({now, wall, current_process_memory()});
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const double wall = std::chrono::duration<double>(Clock::now() - start).count();
    const double sim = now;
    session.halt_background(std::chrono::seconds(10));
    samples.push_back({sim, wall, current_process_memory()});
#ifdef _WIN32
    if (hires) {
        timeEndPeriod(1);
    }
#endif
    std::lock_guard lock(latency_mutex);
    auto stats = [](std::vector<double> v) {
        if (v.empty()) {
            return std::string("null");
        }
        std::sort(v.begin(), v.end());
        return "{\"n\": " + std::to_string(v.size()) + ", \"median\": " + number(v[v.size() / 2])
            + ", \"max\": " + number(v.back()) + "}";
    };
    json << "\"tstep\": " << number(tstep) << ", \"stop\": " << number(stop) << ", \"maxstep\": " << number(maxstep) << ", \"speed\": " << number(speed) << ", \"save\": \"" << save
         << "\", \"wall_seconds\": " << number(wall) << ", \"sim_seconds\": " << number(sim)
         << ", \"achieved_factor\": " << number(sim / wall) << ", \"achieved_ratio\": " << number(sim / wall / speed)
         << ", \"hires_timer\": " << hires << ", \"max_lag_seconds\": " << number(max_lag)
         << ", \"max_oversleep_seconds\": " << number(max_oversleep) << ", \"sleep_fraction\": " << number(sleep_total / wall)
         << ", \"accepted_points\": " << points << ", \"presses\": " << presses
         << ", \"latency_wall\": " << stats(wall_latency) << ", \"latency_sim\": " << stats(sim_latency)
         << ", \"private_mb_per_sim_second\": " << number(slope(samples)) << ", \"memory\": " << memory_json(samples);
    return 0;
}

int segments(NgspiceSession& session, std::ostringstream& json)
{
    const double maxstep = option("maxstep", 1e-4);
    const double sim = option("sim", 6);
    const double segment = option("segment", 1);
    const std::string save = text_option("save", "probes");
    int out_index = -2;
    double offset = 0;
    Trace current;
    session.set_source_handler([&](const std::string&, double t) { return scheduled_button(t + offset); });
    session.set_data_handler([&](const AcceptedPoint& p) {
        if (out_index == -2) {
            out_index = find_vector(session.vector_names(), "out");
        }
        if (out_index >= 0) {
            current.t.push_back(p.time + offset);
            current.v.push_back(p.values[static_cast<std::size_t>(out_index)]);
        }
    });
    auto run = [&](double stop, double vc0) {
        out_index = -2;
        if (!session.load_circuit(circuit(std::min(maxstep, stop), maxstep, stop, vc0, save, false)) || !session.start_background()
            || !session.wait_background(std::chrono::seconds(300))) {
            return false;
        }
        session.command("destroy all");
        session.command("remcirc");
        session.take_output();
        return true;
    };

    // Reference: one continuous run over the whole span.
    auto t0 = Clock::now();
    if (!run(sim, 0)) {
        return 1;
    }
    const double reference_wall = std::chrono::duration<double>(Clock::now() - t0).count();
    const Trace reference = current;
    const auto memory_after_reference = current_process_memory();

    // Segmented: restart every `segment` seconds with C1's final voltage.
    current = {};
    std::vector<double> restart_cost;
    std::vector<MemoryPoint> samples;
    t0 = Clock::now();
    double vc = 0;
    int count = 0;
    for (offset = 0; offset < sim - 1e-12; offset += segment, ++count) {
        const auto restart = Clock::now();
        const std::size_t before = current.t.size();
        if (!run(std::min(segment, sim - offset), vc)) {
            return 1;
        }
        if (current.t.size() == before) {
            return 1;
        }
        restart_cost.push_back(std::chrono::duration<double>(Clock::now() - restart).count());
        vc = current.v.back();
        samples.push_back({offset + segment, std::chrono::duration<double>(Clock::now() - t0).count(),
            current_process_memory()});
    }
    const double segmented_wall = std::chrono::duration<double>(Clock::now() - t0).count();

    // Continuity: compare on a 0.5 ms grid; separate the 5 ms after each restart.
    double worst = 0, worst_near = 0, worst_far = 0;
    double worst_at = 0;
    for (double t = 0; t <= sim; t += 5e-4) {
        const double e = std::abs(interpolate(current, t) - interpolate(reference, t));
        const bool after_restart = t >= segment && std::fmod(t, segment) < 5e-3;
        double& bucket = after_restart ? worst_near : worst_far;
        bucket = std::max(bucket, e);
        if (e > worst) {
            worst = e;
            worst_at = t;
        }
    }
    // The run time beyond simulation work is the per-restart overhead.
    const double per_second = reference_wall / sim;
    std::vector<double> overhead;
    for (double c : restart_cost) {
        overhead.push_back(c - per_second * segment);
    }
    std::sort(overhead.begin(), overhead.end());
    json << "\"maxstep\": " << number(maxstep) << ", \"segment\": " << number(segment) << ", \"sim_seconds\": "
         << number(sim) << ", \"segments\": " << count << ", \"reference_wall\": " << number(reference_wall)
         << ", \"segmented_wall\": " << number(segmented_wall)
         << ", \"restart_overhead_median\": " << number(overhead[overhead.size() / 2])
         << ", \"restart_overhead_max\": " << number(overhead.back())
         << ", \"max_error_volts\": " << number(worst) << ", \"max_error_at\": " << number(worst_at)
         << ", \"max_error_after_restart\": " << number(worst_near)
         << ", \"max_error_elsewhere\": " << number(worst_far)
         << ", \"reference_points\": " << reference.t.size() << ", \"segmented_points\": " << current.t.size()
         << ", \"private_mb_after_reference\": " << number(megabytes(memory_after_reference.private_bytes))
         << ", \"private_mb_per_sim_second\": " << number(slope(samples)) << ", \"memory\": " << memory_json(samples);
    return 0;
}

int soak(NgspiceSession& session, std::ostringstream& json)
{
    const double maxstep = option("maxstep", 1e-4);
    const double sim = option("sim", 120);
    const double segment = option("segment", 1);
    const std::string save = text_option("save", "probes");
    int out_index = -2;
    double offset = 0;
    double last_out = 0;
    session.set_source_handler([&](const std::string&, double t) { return scheduled_button(t + offset); });
    session.set_data_handler([&](const AcceptedPoint& p) {
        if (out_index == -2) {
            out_index = find_vector(session.vector_names(), "out");
        }
        if (out_index >= 0) {
            last_out = p.values[static_cast<std::size_t>(out_index)];
        }
    });
    std::vector<double> restart;
    std::vector<MemoryPoint> samples;
    const auto t0 = Clock::now();
    double vc = 0;
    int count = 0;
    for (offset = 0; offset < sim - 1e-12; offset += segment, ++count) {
        out_index = -2;
        const auto load_start = Clock::now();
        if (!session.load_circuit(circuit(maxstep, maxstep, segment, vc, save, false))) {
            return 1;
        }
        const auto loaded = Clock::now();
        if (!session.start_background() || !session.wait_background(std::chrono::seconds(300))) {
            return 1;
        }
        const auto finished = Clock::now();
        session.command("destroy all");
        session.command("remcirc");
        session.take_output();
        const auto cleaned = Clock::now();
        restart.push_back(std::chrono::duration<double>((loaded - load_start) + (cleaned - finished)).count());
        vc = last_out;
        samples.push_back({offset + segment, std::chrono::duration<double>(Clock::now() - t0).count(),
            current_process_memory()});
    }
    const double wall = std::chrono::duration<double>(Clock::now() - t0).count();
    // Slope over the second half, after allocator warm-up.
    const std::vector<MemoryPoint> late(samples.begin() + static_cast<std::ptrdiff_t>(samples.size() / 2), samples.end());
    double peak = 0;
    for (const auto& m : samples) {
        peak = std::max(peak, megabytes(m.memory.private_bytes));
    }
    std::vector<double> sorted = restart;
    std::sort(sorted.begin(), sorted.end());
    json << "\"maxstep\": " << number(maxstep) << ", \"segment\": " << number(segment) << ", \"sim_seconds\": "
         << number(sim) << ", \"segments\": " << count << ", \"wall_seconds\": " << number(wall)
         << ", \"restart_seconds_median\": " << number(sorted[sorted.size() / 2])
         << ", \"restart_seconds_max\": " << number(sorted.back())
         << ", \"private_mb_first\": " << number(megabytes(samples.front().memory.private_bytes))
         << ", \"private_mb_last\": " << number(megabytes(samples.back().memory.private_bytes))
         << ", \"private_mb_peak\": " << number(peak)
         << ", \"private_mb_per_sim_second_late\": " << number(slope(late)) << ", \"memory\": " << memory_json(samples);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5) {
        std::fprintf(stderr, "usage: s1_continuous <library> <scripts-dir> <output.json> <mode> [key=value...]\n");
        return 2;
    }
    for (int i = 5; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto eq = arg.find('=');
        if (eq != std::string::npos) {
            options[arg.substr(0, eq)] = arg.substr(eq + 1);
        }
    }
    std::string error;
    auto session = NgspiceSession::open({argv[1], argv[2]}, error);
    if (!session) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const std::string mode = argv[4];
#ifdef _WIN32
    const char* platform = "windows";
#else
    const char* platform = "linux";
#endif
    std::ostringstream json;
    json << "{\"spike\": \"S1\", \"mode\": \"" << mode << "\", \"platform\": \"" << platform
         << "\", \"ngspice_version\": " << session->version().value_or(0) << ", ";
    int status = 2;
    if (mode == "growth") {
        status = growth(*session, json);
    } else if (mode == "pacing") {
        status = pacing(*session, json);
    } else if (mode == "segments") {
        status = segments(*session, json);
    } else if (mode == "soak") {
        status = soak(*session, json);
    }
    if (status != 0) {
        std::fprintf(stderr, "mode %s failed\n", mode.c_str());
        for (const auto& line : session->take_output()) {
            std::fprintf(stderr, "  %s\n", line.c_str());
        }
        return status;
    }
    json << "}\n";
    std::ofstream(argv[3], std::ios::binary) << json.str();
    std::printf("%s", json.str().substr(0, 600).c_str());
    std::printf("\n");
    return 0;
}
