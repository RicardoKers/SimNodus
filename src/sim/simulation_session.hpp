// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Simulation session (SN-117, SN-118, SN-121). The GUI talks only to this
// class; it never calls ngspice. A run works on a copy of the schematic, so
// editing the document cannot affect it (INT-05).
//
// Interactive mode (ADR 0080): consecutive transient segments (default 1 s
// of simulated time) restarted with every capacitor voltage and inductor
// current as initial condition, paced against wall clock x speed inside the
// data callback, with controls read through external sources. Memory stays
// bounded: each segment's engine data is discarded, acquisition goes to
// fixed-size ring buffers.
//
// Transient mode: one deterministic .tran; all points of the requested
// vectors are kept for the analyzer.
#pragma once

#include "sample_ring.hpp"

#include "library/component_library.hpp"
#include "mcu/mcu_runner.hpp"
#include "model/schematic.hpp"
#include "netlist/netlist.hpp"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace simnodus::sim {

class NgspiceSession;

enum class RunState { Idle, Running, Paused, Finished, Stopped, Failed };

// A microcontroller of the circuit (SN-131): its firmware runs in Renode,
// started before the analog run and kept within `mcu_lead_us` of its time.
struct McuRequest {
    model::Id component = model::no_id;
    mcu::McuRunOptions options;
};

struct RunRequest {
    model::Schematic schematic;
    const library::ComponentLibrary* library = nullptr;
    // Vectors to acquire, in ngspice naming: node names ("out"), source
    // currents ("v_am1#branch"). Matching is case-insensitive.
    std::vector<std::string> vectors;
    // Pins whose current is acquired; their vectors are added automatically
    // and listed in netlist().pin_current.
    std::vector<netlist::PinRef> current_probes;
    std::size_t ring_capacity = 200000; // samples per vector (AN-09)
    std::size_t transient_limit = 2000000; // points kept in transient mode
    std::vector<McuRequest> mcus;
    std::uint64_t mcu_lead_us = 20000; // firmware and circuit stay this close in virtual time
};

struct RunStatus {
    RunState state = RunState::Idle;
    model::SimulationMode mode = model::SimulationMode::Interactive;
    double sim_time = 0.0;       // seconds of simulated time
    double achieved_factor = 0.0; // simulated seconds per wall second, recent
    double speed = 1.0;          // requested; 0 = maximum
    int segments = 0;
    double progress = 0.0;       // transient: 0..1
    bool starting_mcu = false;   // waiting for the microcontroller engine
    std::string message;         // failure reason in plain language
    std::vector<std::string> engine_output; // ngspice error lines of a failure
};

struct TransientResult {
    std::vector<double> time;
    std::map<std::string, std::vector<double>> vectors;
    bool truncated = false;
};

class SimulationSession {
public:
    SimulationSession();
    ~SimulationSession();
    SimulationSession(const SimulationSession&) = delete;
    SimulationSession& operator=(const SimulationSession&) = delete;

    // Checks and starts a run in a worker thread. Returns false if the
    // netlist has errors (see diagnostics()) or the engine is unavailable.
    bool start(RunRequest request, std::string& error);
    void pause();
    void resume();
    // Ends the run and waits for the worker (bounded by the engine halt).
    void stop();
    bool active() const; // running or paused

    void set_speed(double speed);
    void set_control(model::Id component, double value);
    std::optional<double> control(model::Id component) const;

    RunStatus status() const;
    const std::vector<netlist::Diagnostic>& diagnostics() const { return diagnostics_; }
    // Netlist of the current run (node names, controls); valid after start().
    const netlist::Netlist& netlist() const { return netlist_; }

    std::vector<Sample> samples(const std::string& vector) const;
    // Only the samples at or after `from` (cheap for a scrolling display).
    std::vector<Sample> samples_since(const std::string& vector, double from) const;
    std::optional<double> latest(const std::string& vector) const;
    std::map<std::string, double> latest_all() const;
    TransientResult transient_result() const;

    // Microcontrollers of the current or last run.
    std::optional<mcu::McuStatus> mcu_status(model::Id component) const;
    std::map<std::string, mcu::PinState> mcu_pins(model::Id component) const;
    void mcu_reset(model::Id component);

private:
    bool start_mcus();
    void stop_mcus();
    std::optional<std::string> mcu_failure() const;
    void run_interactive();
    void run_transient();
    bool run_segment(NgspiceSession& engine, const std::vector<std::string>& deck, double t0, double length);
    std::vector<std::string> segment_deck(double t0, double length, bool interactive);
    void fail(const std::string& message, NgspiceSession* engine);
    void set_state(RunState state);

    RunRequest request_;
    model::SimulationSettings settings_;
    netlist::Netlist netlist_;
    std::vector<netlist::Diagnostic> diagnostics_;
    std::vector<std::string> save_vectors_;       // lower case
    std::map<std::string, double> carry_;         // element name -> IC value

    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> pause_requested_{false};
    std::atomic<double> speed_{1.0};

    mutable std::mutex mutex_;
    std::condition_variable pause_changed_;
    RunStatus status_;
    std::map<std::string, double> latest_;
    std::map<std::string, std::unique_ptr<SampleRing>> rings_;
    TransientResult transient_;
    std::map<std::string, double> controls_; // source name (lower case) -> value
    std::map<model::Id, std::string> control_source_;
    // Created by start() before the worker exists and kept until the next start;
    // the runners themselves are thread-safe.
    std::map<model::Id, std::unique_ptr<mcu::McuRunner>> mcus_;

    // Pacing and speed measurement (engine thread only).
    std::chrono::steady_clock::time_point wall_origin_;
    double sim_origin_ = 0.0;
    std::vector<std::pair<double, double>> history_; // (wall, sim)
    std::vector<int> index_;                          // per callback vector: index in save_vectors_ or -1
    bool index_valid_ = false;
    double segment_t0_ = 0.0;
    double last_time_ = 0.0;
};

const char* to_string(RunState state);

// Microcontroller requests for the boards of a schematic: the firmware path is
// resolved against `project_dir` when relative. Returns false with a message
// for the first board without a usable firmware or engine.
struct McuEnvironment {
    std::filesystem::path project_dir;
    std::optional<std::filesystem::path> renode;    // mcu::find_renode()
    std::optional<std::filesystem::path> resources; // mcu::find_mcu_resources()
};
bool build_mcu_requests(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const McuEnvironment& environment, std::vector<McuRequest>& requests, std::string& error);

} // namespace simnodus::sim
