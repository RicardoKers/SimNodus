// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/backend_contracts.hpp"
#include <chrono>
#include <string>
namespace simnodus::ngspice {
enum class WorkerStatus { idle, pending, ready, timeout, malformed, lost, stale };
const char* worker_status_name(WorkerStatus status) noexcept;
struct WorkerReply { AnalogObservation analog{}; Nanoseconds samples = 0; };
bool parse_worker_reply(std::string_view text, WorkerReply& result) noexcept;
class WorkerReader {
public:
    explicit WorkerReader(std::uintptr_t handle) noexcept;
    ~WorkerReader();
    WorkerReader(const WorkerReader&) = delete;
    WorkerReader& operator=(const WorkerReader&) = delete;
    bool valid() const noexcept { return valid_; }
    bool arm(bool allow_buffered);
    WorkerStatus poll();
    WorkerStatus status() const noexcept { return status_; }
    const WorkerReply& result() const noexcept { return result_; }
    const std::string& raw() const noexcept { return text_; }
private:
    std::uintptr_t handle_;
    bool valid_;
    WorkerStatus status_ = WorkerStatus::idle;
    std::chrono::steady_clock::time_point deadline_;
    std::string text_;
    WorkerReply result_;
};
} // namespace simnodus::ngspice
