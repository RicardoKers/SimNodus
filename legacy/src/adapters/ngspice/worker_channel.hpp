// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/backend_contracts.hpp"
#include <string_view>
namespace simnodus::ngspice {
class WorkerChannel {
public:
    explicit WorkerChannel(std::uintptr_t handle) noexcept;
    ~WorkerChannel();
    WorkerChannel(const WorkerChannel&) = delete;
    WorkerChannel& operator=(const WorkerChannel&) = delete;
    bool valid() const noexcept { return valid_; }
    bool advance(Nanoseconds target);
    bool high();
    bool inspect();
private:
    bool write(std::string_view command);
    std::uintptr_t handle_;
    bool valid_;
};
} // namespace simnodus::ngspice
