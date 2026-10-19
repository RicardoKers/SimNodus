// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "measurements.hpp"

#include <algorithm>
#include <cmath>

namespace simnodus::sim {

std::optional<double> value_at(const std::vector<Sample>& samples, double time)
{
    if (samples.empty() || time < samples.front().time || time > samples.back().time) {
        return std::nullopt;
    }
    const auto it =
        std::lower_bound(samples.begin(), samples.end(), time, [](const Sample& s, double t) { return s.time < t; });
    if (it == samples.begin()) {
        return it->value;
    }
    const auto& b = *it;
    const auto& a = *(it - 1);
    const double span = b.time - a.time;
    return span > 0 ? a.value + (b.value - a.value) * (time - a.time) / span : b.value;
}

std::optional<Measurements> measure(const std::vector<Sample>& samples, double from, double to)
{
    std::vector<Sample> window;
    for (const auto& s : samples) {
        if (s.time >= from && s.time <= to) {
            window.push_back(s);
        }
    }
    if (window.size() < 2) {
        return std::nullopt;
    }
    Measurements m;
    m.count = window.size();
    m.min = m.max = window.front().value;
    double area = 0.0;
    double square = 0.0;
    for (std::size_t i = 0; i < window.size(); ++i) {
        m.min = std::min(m.min, window[i].value);
        m.max = std::max(m.max, window[i].value);
        if (i > 0) {
            const double dt = window[i].time - window[i - 1].time;
            const double a = window[i - 1].value;
            const double b = window[i].value;
            area += 0.5 * (a + b) * dt;
            square += (a * a + a * b + b * b) / 3.0 * dt; // exact for a linear segment
        }
    }
    const double span = window.back().time - window.front().time;
    m.mean = span > 0 ? area / span : window.front().value;
    m.rms = span > 0 ? std::sqrt(std::max(0.0, square / span)) : std::abs(window.front().value);
    m.peak_to_peak = m.max - m.min;

    // Frequency and duty from crossings of the mid level (with 5 % hysteresis).
    if (m.peak_to_peak > 0) {
        const double mid = 0.5 * (m.min + m.max);
        const double band = 0.05 * m.peak_to_peak;
        bool high = window.front().value > mid;
        std::vector<double> rising;
        double high_time = 0.0;
        for (std::size_t i = 1; i < window.size(); ++i) {
            const double dt = window[i].time - window[i - 1].time;
            if (high) {
                high_time += dt;
            }
            const double v = window[i].value;
            if (!high && v > mid + band) {
                high = true;
                // Interpolated crossing of the mid level.
                const double a = window[i - 1].value;
                const double t = a != v ? window[i - 1].time + (mid - a) / (v - a) * dt : window[i].time;
                rising.push_back(t);
            } else if (high && v < mid - band) {
                high = false;
            }
        }
        if (rising.size() >= 2) {
            const double period = (rising.back() - rising.front()) / static_cast<double>(rising.size() - 1);
            if (period > 0) {
                m.frequency = 1.0 / period;
            }
            m.duty = span > 0 ? high_time / span : 0.0;
        }
    }
    return m;
}

std::optional<double> find_trigger(const std::vector<Sample>& samples, double level, bool rising, double from, double to)
{
    std::optional<double> found;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        const auto& a = samples[i - 1];
        const auto& b = samples[i];
        if (b.time < from) {
            continue;
        }
        if (a.time > to) {
            break;
        }
        const bool crosses = rising ? (a.value < level && b.value >= level) : (a.value > level && b.value <= level);
        if (crosses) {
            const double t = a.value != b.value ? a.time + (level - a.value) / (b.value - a.value) * (b.time - a.time)
                                                : b.time;
            if (t >= from && t <= to) {
                found = t;
            }
        }
    }
    return found;
}

std::vector<Sample> thin(const std::vector<Sample>& samples, double from, double to, double bucket)
{
    auto first = std::lower_bound(samples.begin(), samples.end(), from,
        [](const Sample& s, double t) { return s.time < t; });
    if (first != samples.begin()) {
        --first;
    }
    auto last = std::upper_bound(samples.begin(), samples.end(), to,
        [](double t, const Sample& s) { return t < s.time; });
    if (last != samples.end()) {
        ++last;
    }
    if (bucket <= 0.0) {
        return {first, last};
    }
    std::vector<Sample> result;
    auto it = first;
    while (it != last) {
        const double key = std::floor(it->time / bucket);
        const auto begin = it;
        auto lo = it, hi = it;
        while (it != last && std::floor(it->time / bucket) == key) {
            if (it->value < lo->value) {
                lo = it;
            }
            if (it->value > hi->value) {
                hi = it;
            }
            ++it;
        }
        const auto end_sample = it - 1;
        auto previous = last;
        for (const auto& k : {begin, std::min(lo, hi), std::max(lo, hi), end_sample}) {
            if (k != previous) {
                result.push_back(*k);
                previous = k;
            }
        }
    }
    return result;
}

bool logic_level_at(bool initial, const std::vector<std::pair<double, bool>>& changes, double time)
{
    bool level = initial;
    for (const auto& [t, next] : changes) {
        if (t > time) {
            break;
        }
        level = next;
    }
    return level;
}

LogicMeasurements measure_logic(bool initial, const std::vector<std::pair<double, bool>>& changes, double from,
    double to)
{
    LogicMeasurements m;
    if (to <= from) {
        return m;
    }
    bool level = logic_level_at(initial, changes, from);
    double since = from;      // start of the current level inside the window
    bool complete = false;    // the current level began at a transition inside the window
    double high = 0;
    std::vector<double> rises;
    double high_sum = 0, low_sum = 0;
    int highs = 0, lows = 0;
    for (const auto& [t, next] : changes) {
        if (t <= from || next == level) {
            continue;
        }
        if (t > to) {
            break;
        }
        if (level) {
            high += t - since;
        }
        if (complete) {
            (level ? high_sum : low_sum) += t - since;
            ++(level ? highs : lows);
        }
        if (next) {
            ++m.rising;
            rises.push_back(t);
        } else {
            ++m.falling;
        }
        level = next;
        since = t;
        complete = true;
    }
    if (level) {
        high += to - since;
    }
    m.duty = high / (to - from);
    if (rises.size() >= 2) {
        m.period = (rises.back() - rises.front()) / static_cast<double>(rises.size() - 1);
        m.frequency = 1.0 / *m.period;
    }
    if (highs > 0) {
        m.high_time = high_sum / highs;
    }
    if (lows > 0) {
        m.low_time = low_sum / lows;
    }
    return m;
}

} // namespace simnodus::sim
