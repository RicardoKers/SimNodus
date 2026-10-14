// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "child_process.hpp"

#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace simnodus::mcu {

#ifdef _WIN32

namespace {

std::wstring widen(const std::string& text)
{
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// Quoting rules of CommandLineToArgvW / the MSVC runtime.
void append_quoted(std::wstring& line, const std::wstring& argument)
{
    if (!line.empty()) {
        line += L' ';
    }
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        line += argument;
        return;
    }
    line += L'"';
    for (auto it = argument.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != argument.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == argument.end()) {
            line.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"') {
            line.append(backslashes * 2 + 1, L'\\');
        } else {
            line.append(backslashes, L'\\');
        }
        line += *it;
    }
    line += L'"';
}

std::wstring environment_block(const std::vector<std::pair<std::string, std::string>>& overrides)
{
    std::vector<std::wstring> entries;
    if (wchar_t* current = GetEnvironmentStringsW()) {
        for (const wchar_t* p = current; *p; p += wcslen(p) + 1) {
            entries.emplace_back(p);
        }
        FreeEnvironmentStringsW(current);
    }
    for (const auto& [name, value] : overrides) {
        const std::wstring key = widen(name) + L'=';
        std::erase_if(entries, [&key](const std::wstring& entry) {
            return entry.size() >= key.size() && CompareStringOrdinal(entry.data(), static_cast<int>(key.size()),
                                                     key.data(), static_cast<int>(key.size()), TRUE) == CSTR_EQUAL;
        });
        entries.push_back(key + widen(value));
    }
    std::wstring block;
    for (const auto& entry : entries) {
        block += entry;
        block += L'\0';
    }
    block += L'\0';
    return block;
}

std::string last_error_text(const char* what)
{
    return std::string(what) + " (Windows error " + std::to_string(GetLastError()) + ")";
}

} // namespace

struct ChildProcess::Impl {
    HANDLE process = nullptr;
    HANDLE job = nullptr;
    HANDLE input = nullptr;
    DWORD pid = 0;
    std::optional<int> exit_code;

    ~Impl()
    {
        if (input) {
            CloseHandle(input);
        }
        if (process) {
            CloseHandle(process);
        }
        if (job) {
            CloseHandle(job); // kills the child if it still runs
        }
    }
};

std::unique_ptr<ChildProcess> ChildProcess::start(const ProcessOptions& options, std::string& error)
{
    auto impl = std::make_unique<Impl>();
    impl->job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!impl->job || !SetInformationJobObject(impl->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        error = last_error_text("Cannot create a job object for the backend");
        return nullptr;
    }

    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input_read = nullptr;
    if (!CreatePipe(&input_read, &impl->input, &inheritable, 0)) {
        error = last_error_text("Cannot create the backend input pipe");
        return nullptr;
    }
    SetHandleInformation(impl->input, HANDLE_FLAG_INHERIT, 0);
    HANDLE output = CreateFileW(options.output_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        CloseHandle(input_read);
        error = last_error_text("Cannot create the backend log file");
        return nullptr;
    }

    // Only these two handles are inherited, even if other threads create inheritable ones.
    HANDLE inherited[2] = {input_read, output};
    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    std::vector<unsigned char> attribute_storage(attribute_size);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_storage.data());
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input_read;
    startup.StartupInfo.hStdOutput = output;
    startup.StartupInfo.hStdError = output;
    bool ok = InitializeProcThreadAttributeList(attributes, 1, 0, &attribute_size)
        && UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited),
            nullptr, nullptr);
    startup.lpAttributeList = attributes;

    std::wstring command_line;
    append_quoted(command_line, options.executable.wstring());
    for (const auto& argument : options.arguments) {
        append_quoted(command_line, widen(argument));
    }
    std::wstring environment = environment_block(options.environment);
    PROCESS_INFORMATION info{};
    const std::wstring directory = options.working_directory.wstring();
    ok = ok
        && CreateProcessW(options.executable.c_str(), command_line.data(), nullptr, nullptr, TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
            environment.data(), directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &info);
    if (!ok) {
        error = last_error_text(("Cannot start " + options.executable.filename().string()).c_str());
    }
    DeleteProcThreadAttributeList(attributes);
    CloseHandle(input_read);
    CloseHandle(output);
    if (!ok) {
        return nullptr;
    }
    impl->process = info.hProcess;
    impl->pid = info.dwProcessId;
    if (!AssignProcessToJobObject(impl->job, info.hProcess)) {
        error = last_error_text("Cannot assign the backend to its job object");
        TerminateProcess(info.hProcess, 1);
        CloseHandle(info.hThread);
        return nullptr;
    }
    ResumeThread(info.hThread);
    CloseHandle(info.hThread);
    std::unique_ptr<ChildProcess> child(new ChildProcess);
    child->impl_ = std::move(impl);
    return child;
}

bool ChildProcess::write_input(const std::string& text)
{
    if (!impl_->input) {
        return false;
    }
    DWORD written = 0;
    return WriteFile(impl_->input, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)
        && written == text.size();
}

void ChildProcess::close_input()
{
    if (impl_->input) {
        CloseHandle(impl_->input);
        impl_->input = nullptr;
    }
}

std::optional<int> ChildProcess::wait(std::chrono::milliseconds timeout)
{
    if (impl_->exit_code) {
        return impl_->exit_code;
    }
    if (WaitForSingleObject(impl_->process, static_cast<DWORD>(timeout.count())) != WAIT_OBJECT_0) {
        return std::nullopt;
    }
    DWORD code = 0;
    GetExitCodeProcess(impl_->process, &code);
    impl_->exit_code = static_cast<int>(code);
    return impl_->exit_code;
}

void ChildProcess::kill()
{
    if (!impl_->exit_code) {
        TerminateJobObject(impl_->job, 1);
        wait(std::chrono::milliseconds(5000));
    }
}

long long ChildProcess::id() const
{
    return impl_->pid;
}

#else

struct ChildProcess::Impl {
    pid_t pid = -1;
    int input = -1;
    std::optional<int> exit_code;

    ~Impl()
    {
        if (input >= 0) {
            ::close(input);
        }
    }
};

std::unique_ptr<ChildProcess> ChildProcess::start(const ProcessOptions& options, std::string& error)
{
    // Everything the child needs is prepared before fork(): only async-signal-safe calls follow.
    std::vector<std::string> argument_storage{options.executable.string()};
    argument_storage.insert(argument_storage.end(), options.arguments.begin(), options.arguments.end());
    std::vector<char*> argv;
    for (auto& argument : argument_storage) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);
    std::vector<std::string> environment_storage;
    for (char** entry = environ; *entry; ++entry) {
        const std::string text = *entry;
        bool replaced = false;
        for (const auto& [name, value] : options.environment) {
            replaced = replaced || text.rfind(name + '=', 0) == 0;
        }
        if (!replaced) {
            environment_storage.push_back(text);
        }
    }
    for (const auto& [name, value] : options.environment) {
        environment_storage.push_back(name + '=' + value);
    }
    std::vector<char*> envp;
    for (auto& entry : environment_storage) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);
    const std::string directory = options.working_directory.string();
    const std::string executable = options.executable.string();

    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
        error = std::string("Cannot create the backend input pipe: ") + std::strerror(errno);
        return nullptr;
    }
    const int output = ::open(options.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (output < 0) {
        error = std::string("Cannot create the backend log file: ") + std::strerror(errno);
        ::close(pipe_fds[0]);
        ::close(pipe_fds[1]);
        return nullptr;
    }
    const pid_t parent = getpid();
    const pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parent) {
            _exit(127);
        }
        setpgid(0, 0);
        dup2(pipe_fds[0], STDIN_FILENO);
        dup2(output, STDOUT_FILENO);
        dup2(output, STDERR_FILENO);
        if (!directory.empty() && chdir(directory.c_str()) != 0) {
            _exit(126);
        }
        execve(executable.c_str(), argv.data(), envp.data());
        _exit(127);
    }
    ::close(pipe_fds[0]);
    ::close(output);
    if (pid < 0) {
        error = std::string("Cannot start the backend: ") + std::strerror(errno);
        ::close(pipe_fds[1]);
        return nullptr;
    }
    signal(SIGPIPE, SIG_IGN); // a dead child must not kill the application on write
    std::unique_ptr<ChildProcess> child(new ChildProcess);
    child->impl_ = std::make_unique<Impl>();
    child->impl_->pid = pid;
    child->impl_->input = pipe_fds[1];
    return child;
}

bool ChildProcess::write_input(const std::string& text)
{
    if (impl_->input < 0) {
        return false;
    }
    std::size_t done = 0;
    while (done < text.size()) {
        const ssize_t n = ::write(impl_->input, text.data() + done, text.size() - done);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(n);
    }
    return true;
}

void ChildProcess::close_input()
{
    if (impl_->input >= 0) {
        ::close(impl_->input);
        impl_->input = -1;
    }
}

std::optional<int> ChildProcess::wait(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!impl_->exit_code) {
        int status = 0;
        const pid_t done = waitpid(impl_->pid, &status, WNOHANG);
        if (done == impl_->pid) {
            impl_->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            break;
        }
        if (done < 0 && errno != EINTR) {
            impl_->exit_code = -1;
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return std::nullopt;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return impl_->exit_code;
}

void ChildProcess::kill()
{
    if (!impl_->exit_code) {
        ::kill(-impl_->pid, SIGKILL); // its process group
        ::kill(impl_->pid, SIGKILL);
        wait(std::chrono::milliseconds(5000));
    }
}

long long ChildProcess::id() const
{
    return impl_->pid;
}

#endif

ChildProcess::~ChildProcess()
{
    if (impl_) {
        close_input();
        kill();
    }
}

bool ChildProcess::running()
{
    return !wait(std::chrono::milliseconds(0)).has_value();
}

} // namespace simnodus::mcu
