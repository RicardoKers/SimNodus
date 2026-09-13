// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Internal explicit-command supervisor, not a project-file execution interface.
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if(!condition) throw std::runtime_error(std::string(message)+" (Win32="+std::to_string(GetLastError())+")");
}
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Tree {
    HANDLE job = nullptr;
    PROCESS_INFORMATION child{};
    ~Tree()
    {
        // Closing the non-inherited job also stops descendants after root exit.
        if(job) CloseHandle(job);
        if(child.hProcess) {
            if(WaitForSingleObject(child.hProcess, 0) != WAIT_OBJECT_0) {
                TerminateProcess(child.hProcess, 1);
                WaitForSingleObject(child.hProcess, 5000);
            }
            CloseHandle(child.hProcess);
        }
        if(child.hThread) CloseHandle(child.hThread);
    }
};
struct Attributes {
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
    ~Attributes() { if(list) DeleteProcThreadAttributeList(list); }
};
// Microsoft CRT argv quoting, including embedded quotes and trailing backslashes.
std::wstring quote(const std::wstring& text)
{
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for(wchar_t c : text) {
        if(c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes*2+1 : slashes, L'\\');
        slashes = 0;
        result += c;
    }
    result.append(slashes*2, L'\\');
    result += L'"';
    return result;
}
bool lifecycle_exists(const std::filesystem::path& path)
{
    const auto attributes = GetFileAttributesW(path.c_str());
    if(attributes != INVALID_FILE_ATTRIBUTES) return true;
    const auto error = GetLastError();
    require(error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND, "Cannot inspect lifecycle file");
    return false;
}
void publish(const std::filesystem::path& target, const std::string& text)
{
    auto temporary = target; temporary += L".tmp";
    {
        Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        require(file.value != INVALID_HANDLE_VALUE, "Cannot create lifecycle record");
        DWORD written = 0;
        require(WriteFile(file.value, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)
            && written == text.size(), "Cannot write lifecycle record");
    }
    require(MoveFileW(temporary.c_str(), target.c_str()), "Cannot publish lifecycle record");
}
int execute(int argc, wchar_t** argv)
{
    require(argc >= 5 && std::wstring(argv[1]) == L"--record" && std::wstring(argv[3]) == L"--", "Usage: process-supervisor --record <absolute-path> -- <absolute-executable> [args]");
    const std::filesystem::path record(argv[2]), executable(argv[4]);
    require(record.is_absolute() && executable.is_absolute(), "Explicit absolute paths required");
    auto stop = record; stop += L".stop";
    auto finished = record; finished += L".exit.json";
    auto temporary = record; temporary += L".tmp";
    auto finished_temporary = finished; finished_temporary += L".tmp";
    require(!lifecycle_exists(finished_temporary) && !lifecycle_exists(record) && !lifecycle_exists(stop) && !lifecycle_exists(finished) && !lifecycle_exists(temporary), "Lifecycle record is not fresh");
    std::wstring command;
    for(int index=4;index<argc;++index) {
        if(!command.empty()) command += L' ';
        command += quote(argv[index]);
    }
    Tree tree;
    tree.job = CreateJobObjectW(nullptr, nullptr);
    require(tree.job != nullptr, "Cannot create fixture job");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(SetInformationJobObject(tree.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)), "Cannot configure fixture job");
    Handle input, output, error;
    const HANDLE current = GetCurrentProcess();
    require(DuplicateHandle(current, GetStdHandle(STD_INPUT_HANDLE), current, &input.value, 0, TRUE, DUPLICATE_SAME_ACCESS), "Cannot duplicate stdin");
    require(DuplicateHandle(current, GetStdHandle(STD_OUTPUT_HANDLE), current, &output.value, 0, TRUE, DUPLICATE_SAME_ACCESS), "Cannot duplicate stdout");
    require(DuplicateHandle(current, GetStdHandle(STD_ERROR_HANDLE), current, &error.value, 0, TRUE, DUPLICATE_SAME_ACCESS), "Cannot duplicate stderr");
    HANDLE inherited[] = {input.value, output.value, error.value};
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    Attributes attributes;
    attributes.storage.resize(size);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
    require(InitializeProcThreadAttributeList(list, 1, 0, &size), "Cannot initialize handle list");
    attributes.list = list;
    require(UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr), "Cannot set handle list");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value;
    startup.StartupInfo.hStdOutput = output.value;
    startup.StartupInfo.hStdError = error.value;
    startup.lpAttributeList = list;
    require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
        &startup.StartupInfo, &tree.child), "Cannot create fixture child");
    require(AssignProcessToJobObject(tree.job, tree.child.hProcess), "Cannot contain fixture child");
    publish(record, std::to_string(tree.child.dwProcessId));
    require(ResumeThread(tree.child.hThread) != static_cast<DWORD>(-1), "Cannot resume fixture child");
    bool forced = false;
    for(;;) {
        const auto waited = WaitForSingleObject(tree.child.hProcess, 10);
        if(waited == WAIT_OBJECT_0) break;
        require(waited == WAIT_TIMEOUT, "Cannot wait for fixture child");
        if(lifecycle_exists(stop)) {
            forced = true;
            require(TerminateJobObject(tree.job, 1), "Cannot stop fixture job");
            require(WaitForSingleObject(tree.child.hProcess, 5000) == WAIT_OBJECT_0, "Fixture termination deadline expired");
            break;
        }
    }
    DWORD exit = 0;
    require(GetExitCodeProcess(tree.child.hProcess, &exit), "Cannot obtain fixture exit");
    // A normally exited root can leave descendants. Confirm the whole owned
    // tree is gone before publishing completion, without changing root exit.
    require(TerminateJobObject(tree.job, 1), "Cannot terminate remaining descendants");
    const auto cleanup_deadline = GetTickCount64() + 5000;
    for(;;) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        require(QueryInformationJobObject(tree.job, JobObjectBasicAccountingInformation,
            &accounting, sizeof(accounting), nullptr), "Cannot inspect fixture job");
        if(accounting.ActiveProcesses == 0) break;
        require(GetTickCount64() < cleanup_deadline, "Descendant termination deadline expired");
        Sleep(10);
    }
    publish(finished, "{\"pid\":"+std::to_string(tree.child.dwProcessId)+",\"exit\":"+std::to_string(exit)
        +",\"forced\":"+(forced ? "true" : "false")+"}\n");
    return static_cast<int>(exit);
}
} // namespace
int wmain(int argc, wchar_t** argv)
{
    try { return execute(argc, argv); }
    catch(const std::exception& error) {
        std::cerr << "Fixture process supervisor failed: " << error.what() << '\n';
        return 2;
    }
}
