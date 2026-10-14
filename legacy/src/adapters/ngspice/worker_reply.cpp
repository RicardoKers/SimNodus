// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "worker_reply.hpp"
#include <array>
#include <charconv>
#ifdef _WIN32
#include <windows.h>
#endif
namespace simnodus::ngspice {
const char* worker_status_name(WorkerStatus s) noexcept
{
    switch(s) {
    case WorkerStatus::idle: return "idle";
    case WorkerStatus::pending: return "pending";
    case WorkerStatus::ready: return "ready";
    case WorkerStatus::timeout: return "timeout";
    case WorkerStatus::malformed: return "malformed";
    case WorkerStatus::lost: return "lost";
    case WorkerStatus::stale: return "stale";
    }
    return "lost";
}
namespace {
bool number(std::string_view value, bool integer)
{
    if(value.empty()) return false;
    std::size_t i = 0;
    if(!integer && value[i] == '-') ++i;
    auto digit = [&](std::size_t n) { return n < value.size() && value[n] >= '0' && value[n] <= '9'; };
    if(!digit(i)) return false;
    if(value[i] == '0') ++i;
    else while(digit(i)) ++i;
    if(!integer && i < value.size() && value[i] == '.') {
        ++i; if(!digit(i)) return false;
        while(digit(i)) ++i;
    }
    if(!integer && i < value.size() && (value[i] == 'e' || value[i] == 'E')) {
        ++i; if(i < value.size() && (value[i] == '+' || value[i] == '-')) ++i;
        if(!digit(i)) return false;
        while(digit(i)) ++i;
    }
    return i == value.size();
}
}
bool parse_worker_reply(std::string_view text, WorkerReply& result) noexcept
{
    constexpr std::array<std::string_view,4> prefixes{"{\"time_ns\":", ",\"actual_s\":", ",\"output_v\":", ",\"samples\":"};
    std::array<std::string_view,4> values;
    for(std::size_t i=0;i<4;++i) {
        if(!text.starts_with(prefixes[i])) return false;
        text.remove_prefix(prefixes[i].size());
        const auto end=text.find(i==3 ? '}' : ',');
        if(end==std::string_view::npos) return false;
        values[i]=text.substr(0,end);text.remove_prefix(end);
        if(!number(values[i],i==0 || i==3)) return false;
    }
    if(text!="}") return false;
    WorkerReply parsed;
    auto convert=[](std::string_view value, auto& output) {
        const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),output);
        return error==std::errc{} && end==value.data()+value.size();
    };
    if(!convert(values[0],parsed.analog.requested_ns) || !convert(values[1],parsed.analog.observed_seconds)
        || !convert(values[2],parsed.analog.output_volts) || !convert(values[3],parsed.samples)
        || RcFixtureContract::observation(parsed.analog)!=BoundaryError::none) return false;
    result=parsed;return true;
}
WorkerReader::WorkerReader(std::uintptr_t handle) noexcept : handle_(handle),valid_(false)
{
#ifdef _WIN32
    valid_=GetFileType(reinterpret_cast<HANDLE>(handle_))==FILE_TYPE_PIPE;
#endif
}
WorkerReader::~WorkerReader()
{
#ifdef _WIN32
    if(handle_) CloseHandle(reinterpret_cast<HANDLE>(handle_));
#endif
}
bool WorkerReader::arm(bool allow_buffered)
{
    if(!valid_ || (status_!=WorkerStatus::idle && status_!=WorkerStatus::ready)) return false;
#ifdef _WIN32
    DWORD available=0;
    if(!PeekNamedPipe(reinterpret_cast<HANDLE>(handle_),nullptr,0,nullptr,&available,nullptr)) {status_=WorkerStatus::lost;return false;}
    if(!allow_buffered && available) {status_=WorkerStatus::stale;return false;}
#else
    static_cast<void>(allow_buffered);return false;
#endif
    text_.clear();status_=WorkerStatus::pending;
    deadline_=std::chrono::steady_clock::now()+std::chrono::milliseconds(1900);
    return true;
}
WorkerStatus WorkerReader::poll()
{
    if(status_!=WorkerStatus::pending) return status_;
    if(std::chrono::steady_clock::now()>=deadline_) return status_=WorkerStatus::timeout;
#ifdef _WIN32
    DWORD available=0;
    if(!PeekNamedPipe(reinterpret_cast<HANDLE>(handle_),nullptr,0,nullptr,&available,nullptr)) return status_=WorkerStatus::lost;
    if(!available) return status_;
    std::array<char,4097> bytes{};
    const DWORD requested=available<bytes.size() ? available : static_cast<DWORD>(bytes.size());
    DWORD count=0;
    if(!ReadFile(reinterpret_cast<HANDLE>(handle_),bytes.data(),requested,&count,nullptr) || !count) return status_=WorkerStatus::lost;
    text_.append(bytes.data(),count);
    if(std::chrono::steady_clock::now()>=deadline_) return status_=WorkerStatus::timeout;
    if(text_.size()>4096) return status_=WorkerStatus::malformed;
    const auto newline=text_.find('\n');
    if(newline==std::string::npos) return status_;
    if(newline+1!=text_.size()) return status_=WorkerStatus::malformed;
    text_.pop_back();if(text_.ends_with('\r')) text_.pop_back();
    if(!parse_worker_reply(text_,result_)) return status_=WorkerStatus::malformed;
    if(std::chrono::steady_clock::now()>=deadline_) return status_=WorkerStatus::timeout;
    return status_=WorkerStatus::ready;
#else
    return status_=WorkerStatus::lost;
#endif
}
} // namespace simnodus::ngspice
