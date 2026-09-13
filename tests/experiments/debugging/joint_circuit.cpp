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
    auto issue = [&](std::string text) { require(command(text.data()) == 0, "Persistent command failed"); };
    Sample final{};
    uint64_t currentNs = 0;
    auto snapshot = [&]() {
        std::cout << std::setprecision(17) << "{\"time_ns\":" << currentNs
                  << ",\"actual_s\":" << final.time << ",\"output_v\":" << final.output
                  << ",\"samples\":" << state.samples.size() << "}" << std::endl;
    };
    snapshot();
    std::string request;
    bool stopped = false;
    while(std::getline(std::cin, request))
    {
        if(request == "quit") { stopped = true; break; }
        if(request == "inspect") { snapshot(); continue; }
        if(request == "stall")
        {
            // Fault injection: keep the real worker alive without acknowledging.
            std::string release;
            std::getline(std::cin, release);
            throw std::runtime_error("Injected stalled worker was released");
        }
        if(request == "high" || request == "low")
        {
            require(currentNs > 0, "Drive change requires an accepted boundary");
            if(request == "high")
            {
                require(!std::isfinite(state.rise), "Only one rising edge is supported");
                state.rise = static_cast<double>(currentNs) * 1e-9;
            }
            else
            {
                require(std::isfinite(state.rise) && !std::isfinite(state.fall), "Invalid falling edge");
                state.fall = static_cast<double>(currentNs) * 1e-9;
            }
            snapshot();
            continue;
        }
        std::istringstream input(request);
        std::string verb, trailing;
        uint64_t ns = 0;
        require(static_cast<bool>(input >> verb >> ns) && !(input >> trailing) && verb == "advance",
                "Invalid persistent circuit request");
        require(ns > currentNs && ns < targetNanoseconds, "Invalid persistent advance boundary");
        const auto time = static_cast<double>(ns) * 1e-9;
        const auto previousCount = state.samples.size();
        require(breakpoint(time), "Integration breakpoint rejected");
        issue("delete all");
        // ngspice 47 equality compares within three ULPs; >= can miss an
        // integration sample when decimal parsing rounds the threshold upward.
        // The independently checked endpoint still must agree within 1 ps.
        issue("stop when time = " + real(time));
        issue(previousCount == 0 ? "run" : "resume");
        require(!state.callbackFault && state.samples.size() > previousCount, "Persistent sample callback failed");
        final = state.samples.back();
        if(std::abs(final.time - time) > 1e-12)
        {
            const auto nearest = std::min_element(state.samples.begin() + previousCount, state.samples.end(),
                [time](const Sample& a, const Sample& b) { return std::abs(a.time - time) < std::abs(b.time - time); });
            std::cerr << std::setprecision(17) << "Boundary diagnostic: requested_s=" << time
                      << " final_s=" << final.time << " nearest_s=" << nearest->time << std::endl;
            throw std::runtime_error("Persistent analog boundary missed");
        }
        currentNs = ns;
        snapshot();
    }
    require(stopped, "Host disconnected without coordinated shutdown");
    char quit[] = "quit";
    static_cast<void>(command(quit));
    require(state.exited && state.quit && state.exitStatus == 0, "ngspice shutdown failed");


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
