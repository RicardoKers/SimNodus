// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Dependency smoke probe only; this does not exercise the numerical solver.

#include <windows.h>
#include <ngspice/sharedspice.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
struct Observations {
    bool version_seen = false;
    bool user_init_disabled = false;
    bool exited = false;
    bool quit_requested = false;
    int exit_status = -1;
};

int output(char* message, int, void* data) noexcept
{
    std::puts(message);
    auto& state = *static_cast<Observations*>(data);
    if (std::strstr(message, "ngspice-47") != nullptr) {
        state.version_seen = true;
    }
    if (std::strstr(message, "Note: .spiceinit is ignored") != nullptr) {
        state.user_init_disabled = true;
    }
    return 0;
}

int exited(int status, NG_BOOL, NG_BOOL quit, int, void* data) noexcept
{
    auto& state = *static_cast<Observations*>(data);
    state.exited = true;
    state.exit_status = status;
    state.quit_requested = quit;
    return 0;
}

class DllDirectory {
public:
    explicit DllDirectory(const std::filesystem::path& directory)
        : cookie_(AddDllDirectory(directory.c_str()))
    {
        if (!cookie_) {
            throw std::runtime_error("Cannot register the audio dependency directory.");
        }
    }
    ~DllDirectory() { RemoveDllDirectory(cookie_); }
    DllDirectory(const DllDirectory&) = delete;
    DllDirectory& operator=(const DllDirectory&) = delete;

private:
    DLL_DIRECTORY_COOKIE cookie_;
};

class Library {
public:
    explicit Library(const std::filesystem::path& file)
        : handle_(LoadLibraryExW(file.c_str(), nullptr,
              LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32
                  | LOAD_LIBRARY_SEARCH_USER_DIRS))
    {
        if (!handle_) {
            std::fprintf(stderr, "LoadLibraryExW failed with Windows error %lu.\n", GetLastError());
            throw std::runtime_error("Cannot load ngspice or one of its native dependencies.");
        }
    }
    ~Library() { FreeLibrary(handle_); }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    template <typename Function>
    Function get(const char* name) const
    {
        auto address = GetProcAddress(handle_, name);
        if (!address) {
            std::fprintf(stderr, "Missing export: %s\n", name);
            throw std::runtime_error("The library does not expose the expected API.");
        }
        // Windows defines GetProcAddress for converting an export to its ABI type.
        return reinterpret_cast<Function>(address);
    }

private:
    HMODULE handle_;
};
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) {
        std::fputs("Usage: ngspice-probe <verified-ngspice.dll> <verified-audio-bin-directory> <owned-init-directory>\n", stderr);
        return 2;
    }
    try {
        const auto dll = std::filesystem::absolute(argv[1]);
        const auto audio = std::filesystem::absolute(argv[2]);
        const auto init = std::filesystem::absolute(argv[3]);
        if (!std::filesystem::is_regular_file(dll) || !std::filesystem::is_directory(audio)) {
            throw std::runtime_error("The library file or audio directory does not exist.");
        }
        // A call to ngSpice_nospinit before initialization crashed the pinned DLL.
        // Use a checked, owned spinit instead; it disables the user init search.
        std::ifstream script(init / "spinit");
        std::string line;
        if (!std::getline(script, line) || line != "set no_spiceinit"
            || std::getline(script, line)) {
            throw std::runtime_error("The owned spinit must contain only: set no_spiceinit");
        }
        // Set before loading the DLL so its C runtime inherits the process setting.
        if (!SetEnvironmentVariableW(L"SPICE_SCRIPTS", init.c_str())) {
            throw std::runtime_error("Cannot select the owned initialization directory.");
        }
        // Keep callback state alive until after the DLL has been unloaded.
        Observations state;
        DllDirectory directory(audio);
        Library library(dll);
        std::fputs("Loaded ngspice DLL.\n", stderr);
        const auto initialize = library.get<decltype(&ngSpice_Init)>("ngSpice_Init");
        const auto command = library.get<decltype(&ngSpice_Command)>("ngSpice_Command");
        // Existence checks do not establish synchronization or breakpoint behavior.
        library.get<decltype(&ngSpice_Init_Sync)>("ngSpice_Init_Sync");
        library.get<decltype(&ngSpice_SetBkpt)>("ngSpice_SetBkpt");
        library.get<decltype(&ngSpice_Circ)>("ngSpice_Circ");
        library.get<decltype(&ngGet_Vec_Info)>("ngGet_Vec_Info");
        const auto running = library.get<decltype(&ngSpice_running)>("ngSpice_running");

        std::fputs("Initializing ngspice.\n", stderr);
        const int init_status = initialize(output, nullptr, exited, nullptr, nullptr, nullptr, &state);
        if (init_status != 0 || state.exited) {
            throw std::runtime_error("ngSpice_Init did not complete normally.");
        }
        char version[] = "version -f";
        const int version_status = command(version);
        const bool was_running = running();
        char quit[] = "quit";
        const int quit_status = command(quit);
        std::printf("init=%d version=%d quit=%d running=%d exit_callback=%d exit_status=%d quit_requested=%d\n",
            init_status, version_status, quit_status, was_running, state.exited, state.exit_status, state.quit_requested);
        // ngspice-47 returns 1 after its quit/longjmp path, with exit callback status 0.
        if (version_status != 0 || quit_status != 1 || !state.version_seen
            || !state.user_init_disabled || was_running
            || !state.exited || !state.quit_requested || state.exit_status != 0) {
            throw std::runtime_error("Version, idle state, or controlled shutdown check failed.");
        }
        std::puts("PASS: ngspice-47 loaded, expected exports found, initialized, reported version, and quit.");
        std::puts("No circuit, code model, firmware, or co-simulation was executed.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
