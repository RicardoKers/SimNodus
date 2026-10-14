// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Run-time loading of the ngspice shared library and a thin, thread-aware
// wrapper around its callback interface. Nothing here is loaded while a
// project is opened; callers decide when an engine is needed (SIM-04).
#pragma once

#include "ngspice_abi.hpp"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::sim {

struct NgspiceLocation {
    std::filesystem::path library;     // ngspice.dll or libngspice.so[.0]
    std::filesystem::path scripts_dir; // directory with the owned spinit
};

// Candidate locations, in search order, for an application installed in
// `application_dir`: the bundled copy first, then (Linux) the system library.
std::vector<NgspiceLocation> ngspice_candidates(const std::filesystem::path& application_dir);

// Extracts "47" from engine output such as "** ngspice-47 : Circuit level...".
std::optional<int> parse_ngspice_version(const std::string& text);

// One accepted time point, copied out of the engine callback.
struct AcceptedPoint {
    int index = 0;
    double time = 0.0;
    std::vector<double> values; // in the order of NgspiceSession::vector_names()
};

// Owns the process-wide ngspice instance. ngspice keeps global state, so only
// one session may exist per process; a second one fails to open.
//
// The library stays mapped until process exit: E-01 found that the worker
// thread can still be returning after it reports completion, so unmapping is
// not a safe barrier. The destructor halts a background run and waits for it.
class NgspiceSession {
public:
    using DataHandler = std::function<void(const AcceptedPoint&)>;
    // Value of an external voltage source at trial time `time`. Trial times
    // can repeat or move backward (E-01 finding 2): return a function of time.
    using SourceHandler = std::function<double(const std::string& source, double time)>;

    static std::unique_ptr<NgspiceSession> open(const NgspiceLocation& location, std::string& error);
    ~NgspiceSession();
    NgspiceSession(const NgspiceSession&) = delete;
    NgspiceSession& operator=(const NgspiceSession&) = delete;

    std::optional<int> version() const { return version_; }

    // Handlers run on the engine thread without locking; set them only while
    // no run is active.
    void set_data_handler(DataHandler handler);
    void set_source_handler(SourceHandler handler);

    bool command(const std::string& text);
    bool load_circuit(const std::vector<std::string>& lines);
    // Vector names reported in the first data callback of the current run.
    std::vector<std::string> vector_names() const;
    std::vector<double> vector(const std::string& name) const;

    bool start_background(const std::string& command = "bg_run");
    bool background_running() const;
    bool wait_background(std::chrono::milliseconds timeout);
    void halt_background(std::chrono::milliseconds timeout);

    // Engine text output since the last call (stdout and stderr lines).
    std::vector<std::string> take_output();
    std::size_t error_line_count() const;

private:
    NgspiceSession() = default;

    static int on_char(char* text, int id, void* user);
    static int on_stat(char* text, int id, void* user);
    static int on_exit(int status, ngspice_abi::NgBool unload, ngspice_abi::NgBool quit, int id, void* user);
    static int on_data(ngspice_abi::VecValuesAll* values, int count, int id, void* user);
    static int on_init_data(ngspice_abi::VecInfoAll* info, int id, void* user);
    static int on_bg_thread(ngspice_abi::NgBool not_running, int id, void* user);
    static int on_vsrc(double* value, double time, char* name, int id, void* user);

    ngspice_abi::Api api_;
    std::optional<int> version_;

    mutable std::mutex mutex_;
    std::condition_variable bg_changed_;
    bool bg_running_ = false;
    std::vector<std::string> output_;
    std::size_t error_lines_ = 0;
    std::vector<std::string> names_;
    bool names_valid_ = false;

    DataHandler data_handler_;
    SourceHandler source_handler_;
    AcceptedPoint scratch_;
};

} // namespace simnodus::sim
