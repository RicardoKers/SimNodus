// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "worker_channel.hpp"
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
namespace simnodus::ngspice {
WorkerChannel::WorkerChannel(std::uintptr_t handle) noexcept : handle_(handle), valid_(false)
{
#ifdef _WIN32
    valid_ = GetFileType(reinterpret_cast<HANDLE>(handle_)) == FILE_TYPE_PIPE;
#endif
}
WorkerChannel::~WorkerChannel()
{
#ifdef _WIN32
    if(handle_ != 0) CloseHandle(reinterpret_cast<HANDLE>(handle_));
#endif
}
bool WorkerChannel::write(std::string_view command)
{
    if(!valid_) return false;
#ifdef _WIN32
    DWORD count = 0;
    return WriteFile(reinterpret_cast<HANDLE>(handle_), command.data(), static_cast<DWORD>(command.size()), &count, nullptr)
        && count == command.size();
#else
    static_cast<void>(command);
    return false;
#endif
}
bool WorkerChannel::advance(Nanoseconds target)
{
    return target > 0 && target < RcFixtureContract::limit_ns
        && write("advance " + std::to_string(target) + "\n");
}
bool WorkerChannel::high() { return write("high\n"); }
bool WorkerChannel::inspect() { return write("inspect\n"); }
} // namespace simnodus::ngspice
