// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "ngspice/sharedspice.h"
#include "ngspice_host.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if(!condition)
    {
        throw std::runtime_error(message);
    }
}

uint64_t number(const wchar_t* text)
{
    wchar_t* end = nullptr;
    const auto result = std::wcstoull(text, &end, 0);
    require(end && *end == L'\0', "Invalid nanosecond argument");
    return result;
}

std::string real(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

struct Sample
{
    double time = 0.0;
    double input = 0.0;
    double output = 0.0;
};

struct State
{
    double rise = std::numeric_limits<double>::infinity();
    double fall = std::numeric_limits<double>::infinity();
    std::vector<Sample> samples;
    bool versionSeen = false;
    bool initializationDisabled = false;
    bool callbackFault = false;
    bool exited = false;
    bool quit = false;
    int exitStatus = -1;
};

int output(char* message, int, void* user) noexcept
{
    auto& state = *static_cast<State*>(user);
    state.versionSeen |= std::strstr(message, "ngspice-47") != nullptr;
    state.initializationDisabled |= std::strstr(message, "Note: .spiceinit is ignored") != nullptr;
    return 0;
}

int exitCallback(int status, NG_BOOL, NG_BOOL quit, int, void* user) noexcept
{
    auto& state = *static_cast<State*>(user);
    state.exited = true;
    state.quit = quit;
    state.exitStatus = status;
    return 0;
}

int data(pvecvaluesall values, int, int, void* user) noexcept
{
    auto& state = *static_cast<State*>(user);
    try
    {
        require(state.samples.size() < 100000, "Circuit sample limit exceeded");
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        Sample sample{nan, nan, nan};
        for(int index = 0; index < values->veccount; ++index)
        {
            const auto* value = values->vecsa[index];
            if(std::strcmp(value->name, "time") == 0) { sample.time = value->creal; }
            if(std::strcmp(value->name, "in") == 0) { sample.input = value->creal; }
            if(std::strcmp(value->name, "out") == 0) { sample.output = value->creal; }
        }
        require(std::isfinite(sample.time) && std::isfinite(sample.input)
                && std::isfinite(sample.output), "Nonfinite circuit callback data");
        if(!state.samples.empty())
        {
            require(sample.time > state.samples.back().time, "Circuit time did not increase");
        }
        state.samples.push_back(sample);
    }
    catch(...)
    {
        state.callbackFault = true;
    }
    return 0;
}

int initData(pvecinfoall, int, void*) noexcept { return 0; }

int source(double* value, double time, char*, int, void* user) noexcept
{
    const auto& state = *static_cast<State*>(user);
    *value = time + 1e-15 >= state.rise && time + 1e-15 < state.fall ? 3.3 : 0.0;
    return 0;
}

int syncCallback(double, double*, double, int, int, int, void*) noexcept { return 0; }
int background(NG_BOOL, int, void*) noexcept { return 0; }

void execute(int argc, wchar_t** argv)
{
    require(argc == 7, "Usage: e05_circuit <dll> <audio-directory> <initialization> <target-ns> <rise-ns-or-none> <fall-ns-or-none>");
    const auto dll = std::filesystem::absolute(argv[1]);
    const auto audio = std::filesystem::absolute(argv[2]);
    const auto initialization = std::filesystem::absolute(argv[3]);
    const uint64_t targetNanoseconds = number(argv[4]);
    require(targetNanoseconds <= 10000000, "Circuit target is outside E-05 bounds");
    State state;
    if(std::wstring(argv[5]) != L"none")
    {
        const auto riseNanoseconds = number(argv[5]);
        require(riseNanoseconds <= targetNanoseconds, "Circuit source rise is after the target");
        state.rise = static_cast<double>(riseNanoseconds) * 1e-9;
    }
    if(std::wstring(argv[6]) != L"none")
    {
        const auto fallNanoseconds = number(argv[6]);
        require(fallNanoseconds <= targetNanoseconds, "Circuit source fall is after the target");
        state.fall = static_cast<double>(fallNanoseconds) * 1e-9;
    }
    require(state.fall >= state.rise, "Circuit fall precedes its rise");
    const double target = static_cast<double>(targetNanoseconds) * 1e-9;

    simnodus::experiment::configure_initialization(initialization);
    simnodus::experiment::DllDirectory search(audio);
    simnodus::experiment::Library library(dll);
    const auto init = library.get<decltype(&ngSpice_Init)>("ngSpice_Init");
    const auto initSync = library.get<decltype(&ngSpice_Init_Sync)>("ngSpice_Init_Sync");
    const auto command = library.get<decltype(&ngSpice_Command)>("ngSpice_Command");
    const auto circuit = library.get<decltype(&ngSpice_Circ)>("ngSpice_Circ");
    const auto breakpoint = library.get<decltype(&ngSpice_SetBkpt)>("ngSpice_SetBkpt");
    require(init(output, nullptr, exitCallback, data, initData, background, &state) == 0,
            "ngspice initialization failed");
    require(state.versionSeen && state.initializationDisabled, "Unexpected ngspice initialization state");
    require(initSync(source, nullptr, syncCallback, nullptr, &state) == 0,
            "ngspice synchronization callback registration failed");

    std::vector<std::string> lines{
        "SimNodus E-05 debugger checkpoint fixture",
        "Vdrive in 0 external",
        "R1 in out 1k",
        "C1 out 0 1u IC=0",
        ".options reltol=1e-6 abstol=1e-12 vntol=1e-9 method=trap maxord=2",
        ".save v(in) v(out)",
        ".tran 0.125u " + real(targetNanoseconds > 0 ? target : 0.000000125) + " 0 0.125u uic",
        ".end",
    };
    std::vector<char*> pointers;
    for(auto& line : lines) { pointers.push_back(line.data()); }
    pointers.push_back(nullptr);
    require(circuit(pointers.data()) == 0, "E-05 circuit load failed");
    const std::vector<uint64_t> boundaries{2010875, 2011375, 4010750, 4027000};
    require(targetNanoseconds > boundaries.back(), "Final target must follow pause boundaries");
    auto issue = [&](std::string text) { require(command(text.data()) == 0, "Persistent command failed"); };
    Sample final{};
    std::size_t previousCount = 0;
    bool exact = true;
    for(const auto ns : boundaries)
    {
        const auto time = static_cast<double>(ns) * 1e-9;
        require(breakpoint(time), "Integration breakpoint rejected");
        issue("delete all");
        issue("stop when time >= " + real(time));
        issue(previousCount == 0 ? "run" : "resume");
        require(!state.callbackFault && state.samples.size() > previousCount, "Persistent sample callback failed");
        final = state.samples.back();
        exact &= std::abs(final.time - time) <= 1e-12;
        std::cout << std::setprecision(17) << "{\"boundary_ns\":" << ns
                  << ",\"actual_s\":" << final.time << ",\"output_v\":" << final.output
                  << ",\"samples\":" << state.samples.size() << "}\n";
        previousCount = state.samples.size();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        require(state.samples.size() == previousCount && state.samples.back().time == final.time
                && state.samples.back().output == final.output, "Paused circuit state changed");
    }
    issue("delete all");
    issue("resume");
    require(!state.callbackFault && state.samples.size() > previousCount, "Final resume failed");
    final = state.samples.back();
    exact &= std::abs(final.time - target) <= 1e-12;
    char quit[] = "quit";
    static_cast<void>(command(quit));
    require(state.exited && state.quit && state.exitStatus == 0, "ngspice shutdown failed");

    std::cout << std::setprecision(17)
              << "{\"target_ns\":" << targetNanoseconds
              << ",\"actual_s\":" << final.time
              << ",\"input_v\":" << final.input
              << ",\"output_v\":" << final.output
              << ",\"samples\":" << state.samples.size() << ",\"all_boundaries_exact\":" << (exact ? "true" : "false") << "}\n";
    require(exact, "Persistent pause missed a predeclared boundary");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    try
    {
        execute(argc, argv);
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << "E-05 circuit failed: " << error.what() << '\n';
        return 1;
    }
}
