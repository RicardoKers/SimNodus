// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Bounded acquisition buffer for one channel (SIM-02, AN-09). Analog samples
// are irregular in time (SPICE steps), so each sample keeps its timestamp.
// Writers and readers may be on different threads.
#pragma once

#include <cstddef>
#include <mutex>
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
    // so that a plot can draw into the window's left edge).
    std::vector<Sample> since(double from) const
    {
        std::lock_guard lock(mutex_);
        const std::size_t start = (head_ + data_.size() - size_) % data_.size();
        // Times increase along the ring: binary search over logical positions.
        std::size_t lo = 0;
        std::size_t hi = size_;
        while (lo < hi) {
            const std::size_t mid = (lo + hi) / 2;
            if (data_[(start + mid) % data_.size()].time < from) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        const std::size_t first = lo > 0 ? lo - 1 : 0;
        std::vector<Sample> result;
        result.reserve(size_ - first);
        for (std::size_t i = first; i < size_; ++i) {
            result.push_back(data_[(start + i) % data_.size()]);
        }
        return result;
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
    mutable std::mutex mutex_;
    std::vector<Sample> data_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
    std::size_t dropped_ = 0;
};

} // namespace simnodus::sim
