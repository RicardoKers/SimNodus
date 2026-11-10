// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The record of a chart recorder channel (SN-281, AN-11): the whole run from
// its start in bounded memory. Samples fall into buckets of equal time; each
// bucket keeps its first, lowest, highest and last value. When the run
// outgrows the buckets, neighbouring buckets merge and the bucket time
// doubles, so a run of hours costs as much memory as one of seconds.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "sim/sample_ring.hpp"

namespace simnodus::sim {

class ChartRecord {
public:
    struct Bucket {
        double first_time = 0.0;
        double last_time = 0.0;
        double first = 0.0;
        double low = 0.0;
        double high = 0.0;
        double last = 0.0;
    };

    // `capacity` buckets at most; the first bucket time is `resolution` seconds.
    explicit ChartRecord(std::size_t capacity = 4096, double resolution = 1e-3);

    // Samples in increasing time; an older sample than the last one is ignored.
    void add(double time, double value);
    void clear();

    bool empty() const { return buckets_.empty(); }
    // Buckets in increasing time; buckets without samples are left out.
    const std::vector<Bucket>& buckets() const { return buckets_; }
    double bucket_time() const { return bucket_time_; }
    std::size_t capacity() const { return capacity_; }
    std::optional<Sample> latest() const;
    // Time of the last sample added (0 for none).
    double end_time() const { return buckets_.empty() ? 0.0 : buckets_.back().last_time; }

private:
    void coarsen();

    std::size_t capacity_;
    double bucket_time_;
    std::vector<Bucket> buckets_;
    std::vector<long long> indices_; // bucket index (time / bucket_time) of each entry of buckets_
};

// The difference plus - minus of two sample series of increasing time: at the
// times of `plus`, with `minus` interpolated linearly (held at its ends).
// An empty `minus` stands for ground (0 V).
std::vector<Sample> difference(const std::vector<Sample>& plus, const std::vector<Sample>& minus);

// CSV text of chart records (SN-281): a header "time (s)" and the channel
// names, then one row per time at which a record has a bucket's last sample;
// each channel interpolated linearly at that time, empty outside its record.
std::string chart_csv(const std::vector<ChartRecord>& records, const std::vector<std::string>& names);

} // namespace simnodus::sim
