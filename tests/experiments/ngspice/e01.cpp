// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// A measured experiment, not a production backend adapter.
#include "ngspice_host.hpp"
#include <ngspice/sharedspice.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace {
struct Sample { int index; double time, input, output; };
struct Trial { double time, value; };
struct Sync { double time, delta, old_delta; int redo, location, request_retry; };
struct State {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Sample> samples;
    std::vector<Trial> trials;
    std::vector<Sync> syncs;
    std::vector<int> background_flags;
    std::map<std::string, double> metrics;
    std::atomic<bool> callback_fault = false;
    bool version_seen = false, init_disabled = false, retry_enabled = false, retry_done = false;
    bool exited = false, quit = false, background_done = true;
    int exit_status = -1, diagnostics = 0, plot_initializations = 0;
    unsigned long data_thread = 0, source_thread = 0;
    HANDLE worker = nullptr;
};

template <typename Action>
int callback(State& state, Action action) noexcept
{
    try {
        std::lock_guard lock(state.mutex);
        action();
        state.changed.notify_all();
    } catch (...) {
        state.callback_fault = true;
    }
    return 0;
}

int output(char* message, int, void* user) noexcept
{
    auto& s = *static_cast<State*>(user);
    return callback(s, [&] {
        std::puts(message);
        s.version_seen |= std::strstr(message, "ngspice-47") != nullptr;
        s.init_disabled |= std::strstr(message, "Note: .spiceinit is ignored") != nullptr;
        if (std::strstr(message, "Error") || std::strstr(message, "error")) { ++s.diagnostics; }
    });
}
int exit_callback(int status, NG_BOOL, NG_BOOL quit, int, void* user) noexcept
{
    auto& s = *static_cast<State*>(user);
    return callback(s, [&] { s.exited = true; s.exit_status = status; s.quit = quit; });
}
int data(pvecvaluesall values, int, int, void* user) noexcept
{
    auto& s = *static_cast<State*>(user);
    return callback(s, [&] {
        if (s.samples.size() >= 200000) { throw std::runtime_error("Sample limit"); }
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        Sample point{values->vecindex, nan, nan, nan};
        for (int i = 0; i < values->veccount; ++i) {
            const auto* v = values->vecsa[i];
            if (std::strcmp(v->name, "time") == 0) { point.time = v->creal; }
            if (std::strcmp(v->name, "in") == 0) { point.input = v->creal; }
            if (std::strcmp(v->name, "out") == 0) { point.output = v->creal; }
        }
        s.samples.push_back(point);
        s.data_thread = GetCurrentThreadId();
    });
}
int init_data(pvecinfoall, int, void* user) noexcept
{
    auto& s = *static_cast<State*>(user);
    return callback(s, [&] { ++s.plot_initializations; });
}
int source(double* value, double time, char*, int, void* user) noexcept
{
    // Pure function of trial time; repeated or decreasing queries do not change state.
    *value = time >= 0.001 && time < 0.003 ? 3.3 : 0.0;
    auto& s = *static_cast<State*>(user);
    return callback(s, [&] {
        if (s.trials.size() >= 1000000) { throw std::runtime_error("Trial limit"); }
        s.trials.push_back({time, *value});
        s.source_thread = GetCurrentThreadId();
    });
}
int sync_callback(double time, double* delta, double old_delta, int redo, int, int loc, void* user) noexcept
{
    auto& s = *static_cast<State*>(user);
    int retry = 0;
    callback(s, [&] {
        if (s.syncs.size() >= 500000) { throw std::runtime_error("Sync limit"); }
        if (s.retry_enabled && !s.retry_done && loc == 1 && redo == 0 && time >= 0.001234) {
            retry = 1;
            s.retry_done = true;
            *delta = old_delta * 0.5;
        }
        s.syncs.push_back({time, *delta, old_delta, redo, loc, retry});
    });
    return retry;
}
int background(NG_BOOL finished, int, void* user) noexcept
{
    auto& s = *static_cast<State*>(user);
    return callback(s, [&] {
        // In this binary the flag is fl_exited: false=start, true=finished.
        s.background_done = finished;
        s.background_flags.push_back(finished ? 1 : 0);
        if (!finished) {
            if (s.worker) { throw std::runtime_error("Previous worker was not joined"); }
            s.worker = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
            if (!s.worker) { throw std::runtime_error("Cannot track worker termination"); }
        }
    });
}

void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

struct Api {
    decltype(&ngSpice_Init) init;
    decltype(&ngSpice_Init_Sync) init_sync;
    decltype(&ngSpice_Command) command;
    decltype(&ngSpice_Circ) circuit;
    decltype(&ngGet_Vec_Info) vector;
    decltype(&ngSpice_SetBkpt) breakpoint;
    decltype(&ngSpice_Reset) reset;
    decltype(&ngSpice_running) running;
};

class WorkerGuard {
public:
    WorkerGuard(Api& api, State& state) : api_(api), state_(state) {}
    bool active = false;
    void join() noexcept
    {
        HANDLE worker;
        {
            std::lock_guard lock(state_.mutex);
            worker = state_.worker;
        }
        // Never unload a DLL or destroy callback state while its thread may run.
        if (!worker || WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) {
            std::fputs("Cannot confirm worker termination; ending isolated process.\n", stderr);
            std::fflush(nullptr);
            std::_Exit(3);
        }
        CloseHandle(worker);
        state_.worker = nullptr;
        active = false;
    }
    ~WorkerGuard()
    {
        if (active) {
            char halt[] = "bg_halt";
            api_.command(halt);
            join();
        }
    }
private:
    Api& api_;
    State& state_;
};

int command(Api& api, State& s, const std::string& name, const std::string& text)
{
    std::string mutable_text = text;
    const int result = api.command(mutable_text.data());
    s.metrics[name] = result;
    return result;
}
void initialize(Api& api, State& s)
{
    s.exited = s.quit = s.version_seen = s.init_disabled = false;
    s.exit_status = -1;
    // In ngspice-47, a null SendInitData also disables SendData, even if supplied.
    require(api.init(output, nullptr, exit_callback, data, init_data, background, &s) == 0,
        "Initialization failed");
    require(s.version_seen && s.init_disabled && !s.exited, "Unexpected version/init state");
    require(api.init_sync(source, nullptr, sync_callback, nullptr, &s) == 0, "Sync registration failed");
}
int load(Api& api, const std::filesystem::path& file, bool long_run)
{
    std::ifstream input(file);
    require(input.good(), "Missing owned netlist");
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) {
        if (long_run && line.starts_with(".tran")) { line = ".tran 1u 50m 0 1u uic"; }
        lines.push_back(line);
    }
    std::vector<char*> pointers;
    for (auto& line : lines) { pointers.push_back(line.data()); }
    pointers.push_back(nullptr);
    return api.circuit(pointers.data());
}
void snapshot(Api& api, const std::filesystem::path& path)
{
    char time_name[] = "time", in_name[] = "v(in)", out_name[] = "v(out)";
    // Copy before any subsequent engine call that may invalidate vector storage.
    const auto copy = [&](char* name) {
        const auto* v = api.vector(name);
        require(v && v->v_realdata && v->v_length > 0, "Missing real vector");
        return std::vector<double>(v->v_realdata, v->v_realdata + v->v_length);
    };
    const auto time = copy(time_name), in = copy(in_name), out = copy(out_name);
    require(time.size() == in.size() && time.size() == out.size(), "Vector length mismatch");
    std::ofstream file(path);
    file << std::setprecision(17) << "time_s,input_v,output_v\n";
    for (size_t i = 0; i < time.size(); ++i) { file << time[i] << ',' << in[i] << ',' << out[i] << '\n'; }
    require(file.good(), "Cannot write vector evidence");
}
void traces(State& s, const std::filesystem::path& directory)
{
    std::ofstream samples(directory / "callbacks.csv"), trials(directory / "source.csv"), syncs(directory / "sync.csv");
    samples << std::setprecision(17) << "index,time_s,input_v,output_v\n";
    for (const auto& p : s.samples) { samples << p.index << ',' << p.time << ',' << p.input << ',' << p.output << '\n'; }
    trials << std::setprecision(17) << "time_s,value_v\n";
    for (const auto& p : s.trials) { trials << p.time << ',' << p.value << '\n'; }
    syncs << std::setprecision(17) << "time_s,delta_s,old_delta_s,redo,location,request_retry\n";
    for (const auto& p : s.syncs) {
        syncs << p.time << ',' << p.delta << ',' << p.old_delta << ',' << p.redo << ',' << p.location << ',' << p.request_retry << '\n';
    }
    require(samples.good() && trials.good() && syncs.good(), "Cannot write callback evidence");
    s.metrics["callback_fault"] = s.callback_fault ? 1 : 0;
    s.metrics["diagnostics"] = s.diagnostics;
    s.metrics["exit_status"] = s.exit_status;
    s.metrics["quit_requested"] = s.quit ? 1 : 0;
    s.metrics["data_on_main_thread"] = s.data_thread == GetCurrentThreadId() ? 1 : 0;
    s.metrics["source_on_main_thread"] = s.source_thread == GetCurrentThreadId() ? 1 : 0;
    s.metrics["plot_initializations"] = s.plot_initializations;
    std::ofstream metrics(directory / "metrics.json");
    metrics << std::setprecision(17) << "{\n";
    bool first = true;
    for (const auto& [key, value] : s.metrics) {
        if (!first) { metrics << ",\n"; }
        first = false;
        metrics << "  \"" << key << "\": " << value;
    }
    metrics << ",\n  \"background_flags\": [";
    for (size_t i = 0; i < s.background_flags.size(); ++i) {
        if (i) { metrics << ','; }
        metrics << s.background_flags[i];
    }
    metrics << "]\n}\n";
    require(metrics.good(), "Cannot write metrics");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 7) {
        std::fputs("Usage: e01 <dll> <audio-directory> <init-directory> <fixtures> <case> <output>\n", stderr);
        return 2;
    }
    try {
        const auto dll = std::filesystem::absolute(argv[1]), audio = std::filesystem::absolute(argv[2]);
        const auto init = std::filesystem::absolute(argv[3]), fixtures = std::filesystem::absolute(argv[4]);
        const std::wstring mode = argv[5];
        require(mode == L"rc" || mode == L"external" || mode == L"breakpoint" || mode == L"pause"
            || mode == L"reset" || mode == L"invalid" || mode == L"retry" || mode == L"background", "Unknown case");
        const auto directory = std::filesystem::absolute(argv[6]);
        std::filesystem::create_directories(directory);
        simnodus::experiment::configure_initialization(init);
        State s;
        simnodus::experiment::DllDirectory search(audio);
        simnodus::experiment::Library library(dll);
        Api api{
            library.get<decltype(&ngSpice_Init)>("ngSpice_Init"),
            library.get<decltype(&ngSpice_Init_Sync)>("ngSpice_Init_Sync"),
            library.get<decltype(&ngSpice_Command)>("ngSpice_Command"),
            library.get<decltype(&ngSpice_Circ)>("ngSpice_Circ"),
            library.get<decltype(&ngGet_Vec_Info)>("ngGet_Vec_Info"),
            library.get<decltype(&ngSpice_SetBkpt)>("ngSpice_SetBkpt"),
            library.get<decltype(&ngSpice_Reset)>("ngSpice_Reset"),
            library.get<decltype(&ngSpice_running)>("ngSpice_running")};
        WorkerGuard worker(api, s);
        initialize(api, s);
        const bool external = mode == L"external" || mode == L"retry";
        const auto netlist = fixtures / (external ? "external.cir" : "rc.cir");
        if (mode == L"invalid") {
            s.metrics["invalid_load"] = load(api, fixtures / "invalid.cir", false);
            s.metrics["invalid_exit_callback"] = s.exited ? 1 : 0;
            s.metrics["invalid_exit_status"] = s.exit_status;
            s.metrics["invalid_diagnostics"] = s.diagnostics;
            command(api, s, "invalid_run", "run");
            char time_name[] = "time";
            const auto* invalid_time = api.vector(time_name);
            s.metrics["invalid_vector_present"] = invalid_time && invalid_time->v_length > 0 ? 1 : 0;
            s.metrics["invalid_callback_samples"] = static_cast<double>(s.samples.size());
            s.metrics["full_reset_after_invalid"] = api.reset();
            initialize(api, s);
        }
        require(load(api, netlist, mode == L"background") == 0, "Netlist load failed");
        if (external) {
            require(api.breakpoint(0.001) && api.breakpoint(0.003), "External breakpoint setup failed");
        }
        s.retry_enabled = mode == L"retry";
        if (mode == L"breakpoint") { s.metrics["set_breakpoint"] = api.breakpoint(0.002) ? 1 : 0; }
        if (mode == L"pause") { command(api, s, "stop_condition", "stop when time > 0.002"); }
        if (mode == L"background") {
            worker.active = true;
            command(api, s, "background_run", "bg_run");
            {
                std::unique_lock lock(s.mutex);
                require(s.changed.wait_for(lock, std::chrono::seconds(5), [&] { return s.samples.size() >= 1000; }),
                    "No background progress");
                require(!s.background_done, "Background run finished before pause request");
                s.metrics["pause_request_observed_time_s"] = s.samples.back().time;
            }
            const auto started = std::chrono::steady_clock::now();
            command(api, s, "background_halt", "bg_halt");
            worker.join();
            s.metrics["halt_wall_seconds"] = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            require(!api.running() && s.background_done, "Background pause not confirmed");
            s.metrics["actual_pause_time_s"] = s.samples.back().time;
            snapshot(api, directory / "first.csv");
            worker.active = true;
            command(api, s, "background_resume", "bg_resume");
            {
                std::unique_lock lock(s.mutex);
                require(s.changed.wait_for(lock, std::chrono::seconds(5), [&] {
                    return s.background_flags.size() >= 4 && s.background_done;
                }), "Background resume did not finish");
            }
            worker.join();
            command(api, s, "background_retire", "bg_halt");
            snapshot(api, directory / "resumed.csv");
        } else {
            command(api, s, "run", "run");
            snapshot(api, directory / "first.csv");
        }
        if (mode == L"pause") {
            command(api, s, "delete_conditions", "delete all");
            command(api, s, "resume", "resume");
            snapshot(api, directory / "resumed.csv");
        }
        if (mode == L"reset") {
            command(api, s, "circuit_reset", "reset");
            command(api, s, "rerun", "run");
            snapshot(api, directory / "circuit-reset.csv");
            s.metrics["full_reset"] = api.reset();
            initialize(api, s);
            require(load(api, netlist, false) == 0, "Reload failed");
            command(api, s, "reinitialized_run", "run");
            snapshot(api, directory / "full-reset.csv");
        }
        s.metrics["idle_before_quit"] = api.running() ? 0 : 1;
        command(api, s, "quit", "quit");
        require(s.exited && s.quit && s.exit_status == 0 && !s.callback_fault, "Shutdown or callback failure");
        traces(s, directory);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "E-01 host failure: %s\n", error.what());
        return 1;
    }
}
