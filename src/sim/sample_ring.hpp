// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Bounded acquisition buffer for one channel (SIM-02, AN-09). Analog samples
// are irregular in time (SPICE steps), so each sample keeps its timestamp.
// Writers and readers may be on different threads.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace simnodus::sim {

struct Sample {
    double time = 0.0;
    double value = 0.0;
};

class SampleRing {
public:
    explicit SampleRing(std::size_t capacity) : data_(capacity == 0 ? 1 : capacity) {}

    void push(double time, double value)
    {
        std::lock_guard lock(mutex_);
        data_[head_] = {time, value};
        head_ = (head_ + 1) % data_.size();
        if (size_ < data_.size()) {
            ++size_;
        } else {
            ++dropped_;
        }
    }

    // Oldest first.
    std::vector<Sample> snapshot() const
    {
        std::lock_guard lock(mutex_);
        std::vector<Sample> result;
        result.reserve(size_);
        const std::size_t start = (head_ + data_.size() - size_) % data_.size();
        for (std::size_t i = 0; i < size_; ++i) {
            result.push_back(data_[(start + i) % data_.size()]);
        }
        return result;
    }

    // Samples with time >= `from`, oldest first (plus the one just before,
    // so that a plot can draw into the window's left edge). With `bucket`
    // (seconds), they are thinned for display: per bucket of time the first,
    // lowest, highest and last sample, in time order. Buckets are fixed in
    // absolute time (k * bucket), so a scrolling plot groups the same samples
    // in every frame and does not flicker.
    std::vector<Sample> since(double from, double bucket = 0.0) const
    {
        std::lock_guard lock(mutex_);
        const std::size_t first = first_since(from);
        std::vector<Sample> result;
        if (bucket <= 0.0) {
            result.reserve(size_ - first);
            for (std::size_t i = first; i < size_; ++i) {
                result.push_back(at(i));
            }
            return result;
        }
        std::size_t i = first;
        while (i < size_) {
            const double key = std::floor(at(i).time / bucket);
            const std::size_t begin = i;
            std::size_t lo = i, hi = i;
            while (i < size_ && std::floor(at(i).time / bucket) == key) {
                if (at(i).value < at(lo).value) {
                    lo = i;
                }
                if (at(i).value > at(hi).value) {
                    hi = i;
                }
                ++i;
            }
            const std::size_t last = i - 1;
            std::size_t previous = static_cast<std::size_t>(-1);
            for (const std::size_t k : {begin, std::min(lo, hi), std::max(lo, hi), last}) {
                if (k != previous) {
                    result.push_back(at(k));
                    previous = k;
                }
            }
        }
        return result;
    }

    // Level changes of the samples with time >= `from` against `threshold`
    // (time, new level), and the level at the first of them; nothing is copied.
    std::vector<std::pair<double, bool>> transitions(double from, double threshold, bool& initial) const
    {
        std::lock_guard lock(mutex_);
        std::vector<std::pair<double, bool>> changes;
        initial = false;
        if (size_ == 0) {
            return changes;
        }
        const std::size_t first = first_since(from);
        bool level = at(first).value > threshold;
        initial = level;
        for (std::size_t i = first + 1; i < size_; ++i) {
            const Sample& s = at(i);
            const bool now = s.value > threshold;
            if (now != level) {
                changes.emplace_back(s.time, now);
                level = now;
            }
        }
        return changes;
    }

    void clear()
    {
        std::lock_guard lock(mutex_);
        head_ = 0;
        size_ = 0;
        dropped_ = 0;
    }

    std::size_t capacity() const { return data_.size(); }

    std::size_t size() const
    {
        std::lock_guard lock(mutex_);
        return size_;
    }

    // Samples overwritten since construction or the last clear().
    std::size_t dropped() const
    {
        std::lock_guard lock(mutex_);
        return dropped_;
    }

private:
    // Logical position i (0 = oldest); the caller holds the lock.
    const Sample& at(std::size_t i) const { return data_[(head_ + data_.size() - size_ + i) % data_.size()]; }

    // Position of the last sample before `from` (or 0); the caller holds the lock.
    std::size_t first_since(double from) const
    {
        // Times increase along the ring: binary search over logical positions.
        std::size_t lo = 0;
        std::size_t hi = size_;
        while (lo < hi) {
            const std::size_t mid = (lo + hi) / 2;
            if (at(mid).time < from) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        return lo > 0 ? lo - 1 : 0;
    }

    mutable std::mutex mutex_;
    std::vector<Sample> data_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::size_t dropped_ = 0;
};

} // namespace simnodus::sim
