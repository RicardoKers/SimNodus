// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A backend child process (Renode) with a stdin pipe and its output in a log
// file. The child cannot outlive the application: on Windows it runs in a Job
// Object that kills it when the last handle closes, on Linux it receives
// SIGKILL when the parent dies. Not thread-safe; one owner.
#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::mcu {

struct ProcessOptions {
    std::filesystem::path executable;
    std::vector<std::string> arguments; // UTF-8, without the executable
    std::filesystem::path working_directory;
    std::vector<std::pair<std::string, std::string>> environment; // overrides of the inherited environment
    std::filesystem::path output_file; // stdout and stderr
};

class ChildProcess {
public:
    static std::unique_ptr<ChildProcess> start(const ProcessOptions& options, std::string& error);
    ~ChildProcess(); // kills the child if it still runs

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    bool write_input(const std::string& text);
    void close_input();
    bool running();
    // Exit code once the child has exited; nullopt if it still runs after `timeout`.
    std::optional<int> wait(std::chrono::milliseconds timeout);
    void kill();
    long long id() const;

private:
    ChildProcess() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace simnodus::mcu
