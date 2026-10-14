// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/backend_contracts.hpp"
#include <cstdint>
#include <string>
#include <string_view>
namespace simnodus::renode {
// Only the measured fixture commands; never forward arbitrary monitor text.
bool start_command(std::string_view path, Nanoseconds duration, std::string& text);
class ControlChannel {
public:
    explicit ControlChannel(std::uintptr_t inherited_handle) noexcept;
    ~ControlChannel();
    ControlChannel(const ControlChannel&) = delete;
    ControlChannel& operator=(const ControlChannel&) = delete;
    bool valid() const noexcept { return valid_; }
    bool start(std::string_view path, Nanoseconds duration);
    bool cancel();
    bool sample(std::string_view path);
    bool notify(std::string_view path, Nanoseconds acknowledged);
private:
    bool write(std::string_view text);
    std::uintptr_t handle_;
    bool valid_;
};
} // namespace simnodus::renode
