// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "control_channel.hpp"
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif
namespace simnodus::renode {
namespace {
bool safe_path(std::string_view path)
{
    if(path.empty() || path.size() > 3000) return false;
    for(const unsigned char c : path)
        if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
             || c == ':' || c == '/' || c == '.' || c == '_' || c == '-')) return false;
    if(!std::filesystem::path(path).is_absolute()) return false;
    return true;
}
bool fresh_path(std::string_view path)
{
    if(!safe_path(path)) return false;
    std::error_code error;
    const bool exists = std::filesystem::exists(std::filesystem::path(path), error);
    if(error || exists) return false;
    const bool failed = std::filesystem::exists(std::filesystem::path(std::string(path)+".error"), error);
    return !error && !failed;
}
} // namespace
bool start_command(std::string_view path, Nanoseconds duration, std::string& text)
{
    if(!safe_path(path) || (duration != 5'000'000 && duration != 100'000'000)) return false;
    text = "emulation StartCancellationProbe " + std::to_string(duration / 1000) + " @" + std::string(path);
    if(duration == 100'000'000) text += " 0 1";
    text += '\n';
    return true;
}
ControlChannel::ControlChannel(std::uintptr_t handle) noexcept : handle_(handle), valid_(false)
{
#ifdef _WIN32
    valid_ = GetFileType(reinterpret_cast<HANDLE>(handle_)) == FILE_TYPE_PIPE;
#endif
}
ControlChannel::~ControlChannel()
{
#ifdef _WIN32
    if(handle_ != 0) CloseHandle(reinterpret_cast<HANDLE>(handle_));
#endif
}
bool ControlChannel::write(std::string_view text)
{
#ifdef _WIN32
    if(!valid_) return false;
    DWORD sent = 0;
    return WriteFile(reinterpret_cast<HANDLE>(handle_), text.data(), static_cast<DWORD>(text.size()), &sent, nullptr)
        && sent == text.size();
#else
    static_cast<void>(text);
    return false;
#endif
}
bool ControlChannel::start(std::string_view path, Nanoseconds duration)
{
    std::string command;
    return start_command(path, duration, command) && write(command);
}
bool ControlChannel::cancel() { return write("emulation CancelCancellationProbe\n"); }
bool ControlChannel::sample(std::string_view path)
{
    return fresh_path(path) && write("emulation SampleCancellationProbe @" + std::string(path) + "\n");
}
bool ControlChannel::notify(std::string_view path, Nanoseconds acknowledged)
{
    return fresh_path(path) && write("emulation NotifyCancellationProbe " + std::to_string(acknowledged)
        + " @" + std::string(path) + "\n");
}
} // namespace simnodus::renode
