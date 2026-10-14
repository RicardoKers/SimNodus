// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "cancellation_result.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace simnodus::renode {
const char* status_name(ResultStatus value) noexcept
{
    switch(value) {
    case ResultStatus::pending: return "pending";
    case ResultStatus::ready: return "ready";
    case ResultStatus::timeout: return "timeout";
    case ResultStatus::malformed: return "malformed";
    case ResultStatus::backend_error: return "backend_error";
    case ResultStatus::io_error: return "io_error";
    case ResultStatus::stale: return "stale";
    case ResultStatus::consumed: return "consumed";
    }
    return "io_error";
}

ResultStatus parse_cancellation(std::string_view text, CancellationResult& result) noexcept
{
    if(text.empty() || text.size() > 4096) return ResultStatus::malformed;
    constexpr std::array<std::string_view, 7> keys{"start", "end", "requested", "unused", "sinks", "reason", "cancelled"};
    std::array<std::string_view, 7> fields{};
    unsigned seen = 0;
    while(!text.empty()) {
        auto length = text.find('\n');
        auto line = text.substr(0, length);
        text = length == std::string_view::npos ? std::string_view{} : text.substr(length+1);
        if(line.ends_with('\r')) line.remove_suffix(1);
        const auto equals = line.find('=');
        if(equals == std::string_view::npos) return ResultStatus::malformed;
        const auto key = line.substr(0, equals);
        std::size_t index = 0;
        while(index < keys.size() && keys[index] != key) ++index;
        if(index == keys.size() || (seen & (1U << index))) return ResultStatus::malformed;
        seen |= 1U << index;
        fields[index] = line.substr(equals+1);
    }
    if(seen != 127 || fields[5] != "cancelled" || fields[6] != "True") return ResultStatus::malformed;
    std::array<Nanoseconds, 5> values{};
    for(std::size_t index = 0; index < values.size(); ++index) {
        const auto value = fields[index];
        const auto [end, error] = std::from_chars(value.data(), value.data()+value.size(), values[index]);
        if(value.empty() || error != std::errc{} || end != value.data()+value.size()) return ResultStatus::malformed;
    }
    result = {values[0], values[1], values[2], values[3], values[4]};
    return ResultStatus::ready;
}

namespace {
ResultStatus presence(const std::filesystem::path& path, bool& exists)
{
    std::error_code error;
    exists = std::filesystem::exists(path, error);
    return error ? ResultStatus::io_error : ResultStatus::ready;
}
ResultStatus read_file(const std::filesystem::path& path, std::string& text, unsigned& retries, bool allow_empty = false, bool exclusive = false)
{
    std::array<char, 4097> buffer{};
#ifdef _WIN32
    struct File {
        HANDLE value;
        ~File() { if(value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    } file{CreateFileW(path.c_str(), GENERIC_READ, exclusive ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if(file.value == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return ResultStatus::pending;
        if(error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
            ++retries;
            return ResultStatus::pending;
        }
        return ResultStatus::io_error;
    }
    if(GetFileType(file.value) != FILE_TYPE_DISK) return ResultStatus::io_error;
    LARGE_INTEGER size;
    if(!GetFileSizeEx(file.value, &size)) return ResultStatus::io_error;
    if(size.QuadPart == 0 && allow_empty) return ResultStatus::pending;
    if(size.QuadPart <= 0 || size.QuadPart > 4096) return ResultStatus::malformed;
    DWORD count = 0;
    if(!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
        return ResultStatus::io_error;
    if(count != size.QuadPart) return ResultStatus::malformed;
    text.assign(buffer.data(), count);
#else
    static_cast<void>(allow_empty);
    if(exclusive) return ResultStatus::io_error; // Closure fence is Windows-only.
    std::ifstream file(path, std::ios::binary);
    if(!file) {
        ++retries;
        return ResultStatus::pending;
    }
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if(file.bad()) return ResultStatus::io_error;
    text.assign(buffer.data(), static_cast<std::size_t>(file.gcount()));
#endif
    return ResultStatus::ready;
}
} // namespace

CancellationReader::CancellationReader(std::filesystem::path path, std::chrono::milliseconds budget)
    : path_(std::move(path)), deadline_(ResultDeadline::Clock::now(), budget)
{
    bool exists = false;
    if(budget.count() <= 0 || budget.count() > 2000 || !path_.is_absolute()) state_ = ResultStatus::io_error;
    else if(presence(path_, exists) != ResultStatus::ready) state_ = ResultStatus::io_error;
    else if(exists) state_ = ResultStatus::stale;
}

ResultStatus CancellationReader::poll()
{
    if(state_ == ResultStatus::ready) return ResultStatus::consumed;
    if(state_ != ResultStatus::pending) return state_;
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    bool failed = false;
    auto error_path = path_;
    error_path += ".error";
    if(presence(error_path, failed) != ResultStatus::ready) return state_ = ResultStatus::io_error;
    if(failed) return state_ = ResultStatus::backend_error;
    std::string text;
    auto status = read_file(path_, text, retries_);
    // A read that completes after the original deadline is not an acknowledgement.
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    if(status == ResultStatus::ready) status = parse_cancellation(text, result_);
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    state_ = status;
    return state_;
}
ReadyReader::ReadyReader(std::filesystem::path path, std::chrono::milliseconds budget)
    : path_(std::move(path)), deadline_(ResultDeadline::Clock::now(), budget)
{
    if(budget.count() <= 0 || budget.count() > 2000 || !path_.is_absolute()) {
        state_ = ResultStatus::io_error;
        return;
    }
    for(const auto* suffix : {"", ".ready", ".error"}) {
        auto candidate = path_;
        candidate += suffix;
        bool exists = false;
        if(presence(candidate, exists) != ResultStatus::ready) state_ = ResultStatus::io_error;
        else if(exists) state_ = ResultStatus::stale;
        if(state_ != ResultStatus::pending) break;
    }
}
ResultStatus ReadyReader::poll()
{
    if(state_ != ResultStatus::pending) return state_;
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    auto error_path = path_; error_path += ".error";
    bool exists = false;
    if(presence(error_path, exists) != ResultStatus::ready) return state_ = ResultStatus::io_error;
    if(exists) return state_ = ResultStatus::backend_error;
    auto ready_path = path_; ready_path += ".ready";
    std::string text;
    auto status = read_file(ready_path, text, retries_, true);
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    if(status == ResultStatus::ready) {
        if(text == "active") state_ = ResultStatus::ready;
        else if(!std::string_view("active").starts_with(text)) state_ = ResultStatus::malformed;
    } else state_ = status;
    return state_;
}
DebugReplyReader::DebugReplyReader(std::filesystem::path path, bool notification, ResultDeadline deadline)
    : path_(std::move(path)), notification_(notification), deadline_(deadline)
{
    if(!path_.is_absolute()) { state_ = ResultStatus::io_error; return; }
    if(deadline_.expired(ResultDeadline::Clock::now())) { state_ = ResultStatus::timeout; return; }
    for(const auto* suffix : {"", ".error"}) {
        auto candidate = path_; candidate += suffix;
        bool exists = false;
        if(presence(candidate, exists) != ResultStatus::ready) state_ = ResultStatus::io_error;
        else if(exists) state_ = ResultStatus::stale;
        if(state_ != ResultStatus::pending) break;
    }
}
ResultStatus DebugReplyReader::poll()
{
    if(state_ == ResultStatus::ready) return ResultStatus::consumed;
    if(state_ != ResultStatus::pending) return state_;
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    auto error_path = path_; error_path += ".error";
    bool failed = false;
    if(presence(error_path, failed) != ResultStatus::ready) return state_ = ResultStatus::io_error;
    if(failed) return state_ = ResultStatus::backend_error;
    std::string text;
    auto status = read_file(path_, text, retries_, false, true);
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    if(status == ResultStatus::ready) {
        if(notification_) {
            if(text.starts_with("rejected: ")) status = ResultStatus::backend_error;
            else if(text != "notified") status = ResultStatus::malformed;
        } else {
            const auto [end, error] = std::from_chars(text.data(), text.data()+text.size(), observed_);
            if(text.empty() || error != std::errc{} || end != text.data()+text.size()) status = ResultStatus::malformed;
        }
    }
    if(deadline_.expired(ResultDeadline::Clock::now())) return state_ = ResultStatus::timeout;
    return state_ = status;
}
} // namespace simnodus::renode
