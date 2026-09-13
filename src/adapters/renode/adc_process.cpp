// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adc_process.hpp"
#include <array>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace simnodus::renode {
struct AdcProcess::Impl {
    const char* state = "idle";
    std::string output, errors;
    std::chrono::steady_clock::time_point deadline;
    std::uint32_t child_pid = 0, code = 0;
    bool ended = false, cleanup = false, out_eof = false, err_eof = false;
#ifdef _WIN32
    HANDLE process = nullptr, job = nullptr, out = nullptr, err = nullptr;
    void stop() {
        if(job) TerminateJobObject(job, 1);
        const auto until = GetTickCount64() + 5000;
        if(process) {
            if(WaitForSingleObject(process, 5000) == WAIT_OBJECT_0) {
                DWORD value = 0;
                if(GetExitCodeProcess(process, &value)) { code = value; ended = true; }
            }
        }
        if(job) {
            do {
                JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
                if(!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &accounting, sizeof(accounting), nullptr)) break;
                if(accounting.ActiveProcesses == 0) { cleanup = true; break; }
                Sleep(1);
            } while(GetTickCount64() < until);
        } else cleanup = process == nullptr;
    }
    bool drain(HANDLE pipe, std::string& text, bool& eof) {
        if(eof) return true;
        DWORD available = 0;
        if(!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
            if(GetLastError() == ERROR_BROKEN_PIPE) { eof = true; return true; }
            return false;
        }
        if(!available) return true;
        std::array<char,4097> buffer{};
        if(text.size() > 4096) return false;
        const auto remaining = static_cast<DWORD>(buffer.size()-text.size());
        DWORD count = 0;
        const DWORD size = available < remaining ? available : remaining;
        if(!ReadFile(pipe, buffer.data(), size, &count, nullptr)) return false;
        text.append(buffer.data(),count);
        return text.size() <= 4096;
    }
    ~Impl() {
        stop();
        for(HANDLE h : {process, job, out, err}) if(h) CloseHandle(h);
    }
#else
    void stop() { cleanup = true; }
#endif
};
AdcProcess::AdcProcess() : impl_(std::make_unique<Impl>()) {}
AdcProcess::~AdcProcess() = default;
#ifdef _WIN32
namespace {
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::wstring quote(const std::wstring& text) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for(wchar_t c : text) {
        if(c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes*2+1 : slashes, L'\\');
        slashes = 0; result += c;
    }
    result.append(slashes*2, L'\\'); return result+L"\"";
}
struct Attributes {
    std::vector<unsigned char> bytes;
    LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
    ~Attributes() { if(list) DeleteProcThreadAttributeList(list); }
};
}
#endif
bool AdcProcess::start(const std::filesystem::path& executable, unsigned port, unsigned microvolts,
                       std::chrono::steady_clock::time_point deadline)
{
    auto& s = *impl_;
    s.deadline = deadline;
    if(std::string_view(s.state) != "idle") return false;
    s.state = "failed";
    if(!executable.is_absolute() || port == 0 || port > 65535 || microvolts > 3300000) return false;
    if(std::chrono::steady_clock::now() >= deadline) { s.state = "timeout"; return false; }
#ifdef _WIN32
    s.job = CreateJobObjectW(nullptr, nullptr);
    if(!s.job) return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(s.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return false;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle out_write, err_write, input;
    if(!CreatePipe(&s.out, &out_write.value, &security, 0) || !SetHandleInformation(s.out, HANDLE_FLAG_INHERIT, 0)
        || !CreatePipe(&s.err, &err_write.value, &security, 0) || !SetHandleInformation(s.err, HANDLE_FLAG_INHERIT, 0)) return false;
    input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(input.value == INVALID_HANDLE_VALUE) return false;
    HANDLE inherited[] = {input.value, out_write.value, err_write.value};
    Attributes attributes;
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    attributes.bytes.resize(size);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.bytes.data());
    if(!InitializeProcThreadAttributeList(list, 1, 0, &size)) return false;
    attributes.list = list;
    if(!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr)) return false;
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value; startup.StartupInfo.hStdOutput = out_write.value; startup.StartupInfo.hStdError = err_write.value;
    startup.lpAttributeList = list;
    auto command = quote(executable.wstring()) + L" " + std::to_wstring(port) + L" adc 0 " + std::to_wstring(microvolts);
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &child)) return false;
    s.process = child.hProcess; s.child_pid = child.dwProcessId;
    Handle thread{child.hThread};
    if(!AssignProcessToJobObject(s.job, s.process)) {
        TerminateProcess(s.process, 1); s.stop(); return false;
    }
    if(std::chrono::steady_clock::now() >= deadline) { s.state = "timeout"; s.stop(); return false; }
    if(ResumeThread(thread.value) == static_cast<DWORD>(-1)) { s.stop(); return false; }
    s.state = "pending";
    return true;
#else
    return false;
#endif
}
const char* AdcProcess::poll()
{
    auto& s = *impl_;
    if(std::string_view(s.state) != "pending") return s.state;
    if(std::chrono::steady_clock::now() >= s.deadline) { s.state = "timeout"; s.stop(); return s.state; }
#ifdef _WIN32
    if(!s.drain(s.out, s.output, s.out_eof) || !s.drain(s.err, s.errors, s.err_eof)) {
        s.state = "failed"; s.stop(); return s.state;
    }
    if(WaitForSingleObject(s.process, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        if(!GetExitCodeProcess(s.process, &code)) { s.state = "failed"; s.stop(); return s.state; }
        s.code = code; s.ended = true;
        // A helper is not allowed to leave descendants alive after its root exits.
        s.stop();
        if(!s.drain(s.out, s.output, s.out_eof) || !s.drain(s.err, s.errors, s.err_eof)) s.state = "failed";
        else if(s.out_eof && s.err_eof) {
            const std::string expected = "{\"command\":\"adc\",\"before_us\":4010,\"after_us\":4010}";
            s.state = s.code == 0 && s.cleanup && s.errors.empty()
                && (s.output == expected+"\n" || s.output == expected+"\r\n") ? "ready" : "failed";
        }
    }
#endif
    if(std::chrono::steady_clock::now() >= s.deadline) { s.state = "timeout"; s.stop(); }
    return s.state;
}
void AdcProcess::cancel() { impl_->stop(); if(std::string_view(impl_->state) == "pending") impl_->state = "failed"; }
const char* AdcProcess::status() const noexcept { return impl_->state; }
std::uint32_t AdcProcess::pid() const noexcept { return impl_->child_pid; }
bool AdcProcess::exited() const noexcept { return impl_->ended; }
std::uint32_t AdcProcess::exit_code() const noexcept { return impl_->code; }
bool AdcProcess::cleaned() const noexcept { return impl_->cleanup; }
namespace {
std::string hex(const std::string& text) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for(unsigned char c : text) { result += digits[c >> 4]; result += digits[c & 15]; }
    return result;
}
}
std::string AdcProcess::stdout_hex() const { return hex(impl_->output); }
std::string AdcProcess::stderr_hex() const { return hex(impl_->errors); }
} // namespace simnodus::renode
