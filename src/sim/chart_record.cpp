// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "sim/chart_record.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace simnodus::sim {

ChartRecord::ChartRecord(std::size_t capacity, double resolution)
    : capacity_(std::max<std::size_t>(capacity, 2)), bucket_time_(resolution > 0 ? resolution : 1e-3)
{
}

void ChartRecord::clear()
{
    buckets_.clear();
    indices_.clear();
}

void ChartRecord::add(double time, double value)
{
    if (!std::isfinite(time) || !std::isfinite(value) || time < 0.0) {
        return;
    }
    if (!buckets_.empty() && time < buckets_.back().last_time) {
        return;
    }
    for (;;) {
        const auto index = static_cast<long long>(std::floor(time / bucket_time_));
        if (!indices_.empty() && indices_.back() == index) {
            auto& b = buckets_.back();
            b.last_time = time;
            b.last = value;
            b.low = std::min(b.low, value);
            b.high = std::max(b.high, value);
            return;
        }
        // A new bucket: within the capacity's span of time from 0, else coarser buckets.
        if (index >= static_cast<long long>(capacity_)) {
            coarsen();
            continue;
        }
        buckets_.push_back({time, time, value, value, value, value});
        indices_.push_back(index);
        return;
    }
}

void ChartRecord::coarsen()
{
    bucket_time_ *= 2.0;
    std::vector<Bucket> merged;
    std::vector<long long> indices;
    merged.reserve(buckets_.size());
    indices.reserve(buckets_.size());
    for (std::size_t i = 0; i < buckets_.size(); ++i) {
        const long long index = indices_[i] / 2;
        if (!indices.empty() && indices.back() == index) {
            auto& b = merged.back();
            const auto& n = buckets_[i];
            b.last_time = n.last_time;
            b.last = n.last;
            b.low = std::min(b.low, n.low);
            b.high = std::max(b.high, n.high);
        } else {
            merged.push_back(buckets_[i]);
            indices.push_back(index);
        }
    }
    buckets_ = std::move(merged);
    indices_ = std::move(indices);
}

std::optional<Sample> ChartRecord::latest() const
{
    if (buckets_.empty()) {
        return std::nullopt;
    }
    return Sample{buckets_.back().last_time, buckets_.back().last};
}

std::vector<Sample> difference(const std::vector<Sample>& plus, const std::vector<Sample>& minus)
{
    if (minus.empty()) {
        return plus;
    }
    std::vector<Sample> out;
    out.reserve(plus.size());
    std::size_t j = 0;
    for (const auto& p : plus) {
        while (j + 1 < minus.size() && minus[j + 1].time <= p.time) {
            ++j;
        }
        double m = minus[j].value;
        if (p.time > minus[j].time && j + 1 < minus.size()) {
            const auto& a = minus[j];
            const auto& b = minus[j + 1];
            const double span = b.time - a.time;
            m = span > 0 ? a.value + (b.value - a.value) * (p.time - a.time) / span : b.value;
        } else if (p.time < minus.front().time) {
            m = minus.front().value;
        }
        out.push_back({p.time, p.value - m});
    }
    return out;
}

std::string chart_csv(const std::vector<ChartRecord>& records, const std::vector<std::string>& names)
{
    std::vector<double> times;
    for (const auto& r : records) {
        for (const auto& b : r.buckets()) {
            times.push_back(b.last_time);
        }
    }
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    std::string out = "time (s)";
    for (std::size_t i = 0; i < records.size(); ++i) {
        out += ',';
        out += i < names.size() ? names[i] : "CH" + std::to_string(i + 1);
    }
    out += '\n';
    std::vector<std::size_t> cursor(records.size(), 0);
    char buffer[64];
    for (const double t : times) {
        std::snprintf(buffer, sizeof buffer, "%.9g", t);
        out += buffer;
        for (std::size_t i = 0; i < records.size(); ++i) {
            out += ',';
            const auto& b = records[i].buckets();
            if (b.empty() || t < b.front().last_time || t > b.back().last_time) {
                continue;
            }
            auto& k = cursor[i];
            while (k + 1 < b.size() && b[k + 1].last_time <= t) {
                ++k;
            }
            double v = b[k].last;
            if (b[k].last_time < t && k + 1 < b.size()) {
                const double span = b[k + 1].last_time - b[k].last_time;
                v += span > 0 ? (b[k + 1].last - b[k].last) * (t - b[k].last_time) / span : 0.0;
            }
            std::snprintf(buffer, sizeof buffer, "%.6g", v);
            out += buffer;
        }
        out += '\n';
    }
    return out;
}

} // namespace simnodus::sim
