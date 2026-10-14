// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
namespace simnodus::renode {
class AdcProcess {
public:
    AdcProcess();
    ~AdcProcess();
    AdcProcess(const AdcProcess&) = delete;
    AdcProcess& operator=(const AdcProcess&) = delete;
    bool start(const std::filesystem::path& executable, unsigned port, unsigned microvolts,
               std::chrono::steady_clock::time_point deadline);
    const char* poll();
    void cancel();
    const char* status() const noexcept;
    std::uint32_t pid() const noexcept;
    bool exited() const noexcept;
    std::uint32_t exit_code() const noexcept;
    bool cleaned() const noexcept;
    std::string stdout_hex() const;
    std::string stderr_hex() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace simnodus::renode
