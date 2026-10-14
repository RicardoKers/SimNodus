// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/joint_session.hpp"
#include <chrono>
#include <filesystem>
#include <string_view>

namespace simnodus::renode {
enum class ResultStatus { pending, ready, timeout, malformed, backend_error, io_error, stale, consumed };
const char* status_name(ResultStatus status) noexcept;
struct CancellationResult {
    Nanoseconds start = 0, end = 0, requested = 0, unused = 0, sink = 0;
    CpuOutcome outcome() const noexcept { return {start, end, requested, unused, true, {&sink, 1}}; }
};
ResultStatus parse_cancellation(std::string_view text, CancellationResult& result) noexcept;

class ResultDeadline {
public:
    using Clock = std::chrono::steady_clock;
    ResultDeadline(Clock::time_point start, std::chrono::milliseconds budget) : end_(start + budget) {}
    bool expired(Clock::time_point now) const noexcept { return now >= end_; }
private:
    Clock::time_point end_;
};

// Reads only the owned, atomically published cancellation-result file.
// This does not authorize commands, cancel Renode or establish a joint commit.
class CancellationReader {
public:
    explicit CancellationReader(std::filesystem::path path, std::chrono::milliseconds budget);
    ResultStatus poll();
    ResultStatus status() const noexcept { return state_; }
    const CancellationResult& result() const noexcept { return result_; }
    unsigned retries() const noexcept { return retries_; }
private:
    std::filesystem::path path_;
    ResultDeadline deadline_;
    CancellationResult result_;
    ResultStatus state_ = ResultStatus::pending;
    unsigned retries_ = 0;
};
// Ready is not atomically published by the bridge; an empty/prefix read remains
// pending within the same deadline. It never acknowledges CPU execution.
class ReadyReader {
public:
    explicit ReadyReader(std::filesystem::path grant_path, std::chrono::milliseconds budget);
    ResultStatus poll();
    ResultStatus status() const noexcept { return state_; }
private:
    std::filesystem::path path_;
    ResultDeadline deadline_;
    ResultStatus state_ = ResultStatus::pending;
    unsigned retries_ = 0;
};
// The owned bridge writes each reply once with one file handle. Exclusive open
// fences that writer's close, so a numeric prefix is never accepted mid-write.
class DebugReplyReader {
public:
    DebugReplyReader(std::filesystem::path path, bool notification, ResultDeadline deadline);
    ResultStatus poll();
    ResultStatus status() const noexcept { return state_; }
    Nanoseconds observed() const noexcept { return observed_; }
    unsigned retries() const noexcept { return retries_; }
private:
    std::filesystem::path path_;
    bool notification_;
    ResultDeadline deadline_;
    ResultStatus state_ = ResultStatus::pending;
    Nanoseconds observed_ = 0;
    unsigned retries_ = 0;
};
} // namespace simnodus::renode
