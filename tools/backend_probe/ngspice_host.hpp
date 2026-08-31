// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Windows-only helpers for explicit, trusted backend experiments.
#pragma once
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace simnodus::experiment {
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

inline void configure_initialization(const std::filesystem::path& init)
{
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
}
} // namespace simnodus::experiment
