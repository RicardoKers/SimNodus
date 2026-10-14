// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "simulation_session.hpp"

#include "engine_host.hpp"
#include "ngspice_runtime.hpp"

#include "model/engineering.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace simnodus::sim {
namespace {

using Clock = std::chrono::steady_clock;

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

double seconds_since(Clock::time_point origin)
{
    return std::chrono::duration<double>(Clock::now() - origin).count();
}

// Plain-language reason for an engine failure (SIM-05).
std::string explain(const std::vector<std::string>& lines, double at)
{
    std::string joined;
    for (const auto& l : lines) {
        joined += lower(l) + "\n";
    }
    const std::string when = " at t = " + model::format_engineering(at) + "s";
    if (joined.find("timestep too small") != std::string::npos) {
        return "The simulation did not converge" + when
            + " (time step too small). Look for ideal switches or sources in a loop, or extreme component values.";
    }
    if (joined.find("singular matrix") != std::string::npos) {
        return "The circuit equations cannot be solved" + when
            + " (singular matrix): a node may be floating or two ideal sources conflict.";
    }
    if (joined.find("memory required") != std::string::npos) {
        return "The engine refused the run: not enough memory for the requested time span and step.";
    }
    return "The simulation stopped" + when + " because the engine reported an error.";
}

} // namespace

const char* to_string(RunState state)
{
    switch (state) {
    case RunState::Idle:
        return "Idle";
    case RunState::Running:
        return "Running";
    case RunState::Paused:
        return "Paused";
    case RunState::Finished:
        return "Finished";
    case RunState::Stopped:
        return "Stopped";
    case RunState::Failed:
        return "Failed";
    }
    return "?";
}

SimulationSession::SimulationSession() = default;

SimulationSession::~SimulationSession()
{
    stop();
}

bool SimulationSession::start(RunRequest request, std::string& error)
{
    stop();
    if (!request.library) {
        error = "No component library.";
        return false;
    }
    request_ = std::move(request);
    settings_ = request_.schematic.settings();
    const bool interactive = settings_.mode == model::SimulationMode::Interactive;
    netlist_ = netlist::generate_netlist(request_.schematic, *request_.library, {interactive, 0.0});
    diagnostics_ = netlist_.diagnostics;
    if (!netlist_.ok()) {
        error = "The circuit has errors; see the Messages panel.";
        return false;
    }
    if (netlist_.elements.empty()) {
        error = "The schematic is empty.";
        return false;
    }

    // Vectors: requested ones plus the state carried across segments.
    std::set<std::string> seen;
    save_vectors_.clear();
    auto add = [&](const std::string& name) {
        const std::string key = lower(name);
        if (key.empty() || key == "0" || !seen.insert(key).second) {
            return;
        }
        save_vectors_.push_back(key);
    };
    for (const auto& v : request_.vectors) {
        add(v);
    }
    for (const auto& e : netlist_.elements) {
        if (e.letter == 'C' && e.nodes.size() == 2) {
            add(e.nodes[0]);
            add(e.nodes[1]);
        } else if (e.letter == 'L') {
            add(e.name + "#branch");
        }
    }

    {
        std::lock_guard lock(mutex_);
        rings_.clear();
        for (const auto& v : request_.vectors) {
            rings_[lower(v)] = std::make_unique<SampleRing>(request_.ring_capacity);
        }
        latest_.clear();
        transient_ = {};
        controls_.clear();
        control_source_.clear();
        for (const auto& c : netlist_.controls) {
            controls_[lower(c.source)] = c.initial;
            control_source_[c.component] = lower(c.source);
        }
        status_ = {};
        status_.mode = settings_.mode;
        status_.state = RunState::Running;
        status_.speed = interactive ? settings_.speed : 0.0;
    }
    carry_.clear();
    speed_ = interactive ? settings_.speed : 0.0;
    stop_requested_ = false;
    pause_requested_ = false;
    worker_ = std::thread([this, interactive] { interactive ? run_interactive() : run_transient(); });
    return true;
}

void SimulationSession::pause()
{
    if (active()) {
        pause_requested_ = true;
    }
}

void SimulationSession::resume()
{
    pause_requested_ = false;
    pause_changed_.notify_all();
}

void SimulationSession::stop()
{
    stop_requested_ = true;
    pause_requested_ = false;
    pause_changed_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool SimulationSession::active() const
{
    std::lock_guard lock(mutex_);
    return status_.state == RunState::Running || status_.state == RunState::Paused;
}

void SimulationSession::set_speed(double speed)
{
    speed_ = std::max(0.0, speed);
    std::lock_guard lock(mutex_);
    status_.speed = speed_;
}

void SimulationSession::set_control(model::Id component, double value)
{
    std::lock_guard lock(mutex_);
    const auto it = control_source_.find(component);
    if (it != control_source_.end()) {
        controls_[it->second] = value;
    }
}

std::optional<double> SimulationSession::control(model::Id component) const
{
    std::lock_guard lock(mutex_);
    const auto it = control_source_.find(component);
    if (it == control_source_.end()) {
        return std::nullopt;
    }
    return controls_.at(it->second);
}

RunStatus SimulationSession::status() const
{
    std::lock_guard lock(mutex_);
    return status_;
}

std::vector<Sample> SimulationSession::samples(const std::string& vector) const
{
    std::lock_guard lock(mutex_);
    const auto it = rings_.find(lower(vector));
    return it == rings_.end() ? std::vector<Sample>{} : it->second->snapshot();
}

std::optional<double> SimulationSession::latest(const std::string& vector) const
{
    std::lock_guard lock(mutex_);
    const auto key = lower(vector);
    if (key == "0") {
        return 0.0;
    }
    const auto it = latest_.find(key);
    return it == latest_.end() ? std::nullopt : std::optional<double>(it->second);
}

std::map<std::string, double> SimulationSession::latest_all() const
{
    std::lock_guard lock(mutex_);
    return latest_;
}

TransientResult SimulationSession::transient_result() const
{
    std::lock_guard lock(mutex_);
    return transient_;
}

void SimulationSession::set_state(RunState state)
{
    std::lock_guard lock(mutex_);
    status_.state = state;
}

void SimulationSession::fail(const std::string& message, NgspiceSession* engine)
{
    std::lock_guard lock(mutex_);
    status_.state = RunState::Failed;
    status_.message = message;
    if (engine) {
        for (auto& line : engine->take_output()) {
            if (line.rfind("stderr", 0) == 0) {
                status_.engine_output.push_back(line.substr(7));
            }
        }
    }
}

std::vector<std::string> SimulationSession::segment_deck(double t0, double length, bool interactive)
{
    // Periodic sources depend on t0, so the netlist is generated per segment.
    const auto n = netlist::generate_netlist(request_.schematic, *request_.library, {interactive, t0});
    std::vector<std::string> lines;
    for (const auto& line : n.lines) {
        std::string out = line;
        const std::string name = lower(line.substr(0, line.find(' ')));
        if (const auto it = carry_.find(name); it != carry_.end()) {
            out += " IC=" + model::spice_number(it->second);
        }
        lines.push_back(out);
    }
    netlist::Netlist with_ic = n;
    with_ic.lines = lines;
    std::vector<std::string> extra;
    for (std::size_t i = 0; i < save_vectors_.size(); i += 16) {
        std::string save = ".save";
        for (std::size_t j = i; j < std::min(save_vectors_.size(), i + 16); ++j) {
            save += " " + save_vectors_[j];
        }
        extra.push_back(save);
    }
    const double step = interactive ? settings_.max_step : settings_.transient_max_step;
    // Output step = maximum step keeps ngspice's preallocation small (S1).
    extra.push_back(".tran " + model::spice_number(step) + " " + model::spice_number(length) + " 0 "
        + model::spice_number(step) + " uic");
    return netlist::build_deck(with_ic, "SimNodus", extra);
}

bool SimulationSession::run_segment(NgspiceSession& engine, const std::vector<std::string>& deck, double t0,
    double length)
{
    segment_t0_ = t0;
    last_time_ = 0.0;
    index_valid_ = false;
    engine.take_output();
    if (!engine.load_circuit(deck)) {
        fail("The engine rejected the circuit.", &engine);
        engine.command("remcirc");
        return false;
    }
    if (!engine.start_background()) {
        fail("The engine could not start the simulation.", &engine);
        engine.command("remcirc");
        return false;
    }
    while (!engine.wait_background(std::chrono::milliseconds(50))) {
        if (stop_requested_) {
            pause_changed_.notify_all();
            engine.halt_background(std::chrono::seconds(10));
            break;
        }
    }
    const bool complete = last_time_ >= length * (1.0 - 1e-9);
    if (!complete && !stop_requested_) {
        auto output = engine.take_output();
        fail(explain(output, t0 + last_time_), nullptr);
        std::lock_guard lock(mutex_);
        for (const auto& line : output) {
            if (line.rfind("stderr", 0) == 0) {
                status_.engine_output.push_back(line.substr(7));
            }
        }
    }
    // Carry capacitor voltages and inductor currents to the next segment.
    if (complete) {
        std::lock_guard lock(mutex_);
        auto value = [this](const std::string& node) {
            if (node == "0") {
                return 0.0;
            }
            const auto it = latest_.find(lower(node));
            return it == latest_.end() ? 0.0 : it->second;
        };
        for (const auto& e : netlist_.elements) {
            if (e.letter == 'C' && e.nodes.size() == 2) {
                carry_[lower(e.name)] = value(e.nodes[0]) - value(e.nodes[1]);
            } else if (e.letter == 'L') {
                carry_[lower(e.name)] = value(e.name + "#branch");
            }
        }
    }
    engine.command("destroy all");
    engine.command("remcirc");
    engine.take_output();
    return complete;
}

void SimulationSession::run_interactive()
{
    auto& host = EngineHost::instance();
    std::lock_guard engine_lock(host.lock());
    std::string error;
    NgspiceSession* engine = host.acquire(error);
    if (!engine) {
        fail("The analog engine is not available: " + error, nullptr);
        return;
    }
    wall_origin_ = Clock::now();
    sim_origin_ = 0.0;
    history_.clear();
    double last_speed = speed_;

    engine->set_source_handler([this](const std::string& source, double) {
        std::lock_guard lock(mutex_);
        const auto it = controls_.find(lower(source));
        return it == controls_.end() ? 0.0 : it->second;
    });
    engine->set_data_handler([this, engine, &last_speed](const AcceptedPoint& point) {
        if (!index_valid_) {
            const auto names = engine->vector_names();
            index_.assign(names.size(), -1);
            for (std::size_t i = 0; i < names.size(); ++i) {
                const auto key = lower(names[i]);
                const auto it = std::find(save_vectors_.begin(), save_vectors_.end(), key);
                if (it != save_vectors_.end()) {
                    index_[i] = static_cast<int>(it - save_vectors_.begin());
                }
            }
            index_valid_ = true;
        }
        last_time_ = point.time;
        const double global = segment_t0_ + point.time;
        {
            std::lock_guard lock(mutex_);
            for (std::size_t i = 0; i < index_.size() && i < point.values.size(); ++i) {
                if (index_[i] < 0) {
                    continue;
                }
                const auto& name = save_vectors_[static_cast<std::size_t>(index_[i])];
                latest_[name] = point.values[i];
                if (const auto ring = rings_.find(name); ring != rings_.end()) {
                    ring->second->push(global, point.values[i]);
                }
            }
            status_.sim_time = global;
        }
        // Pause inside the callback: the engine thread simply waits.
        if (pause_requested_ && !stop_requested_) {
            set_state(RunState::Paused);
            const auto paused_at = Clock::now();
            std::unique_lock lock(mutex_);
            pause_changed_.wait(lock, [this] { return !pause_requested_ || stop_requested_; });
            status_.state = RunState::Running;
            lock.unlock();
            wall_origin_ += Clock::now() - paused_at;
            history_.clear();
        }
        // Pace against wall clock x speed (re-anchored when the speed changes).
        const double speed = speed_;
        if (speed != last_speed) {
            wall_origin_ = Clock::now();
            sim_origin_ = global;
            last_speed = speed;
        }
        if (speed > 0.0 && !stop_requested_) {
            const auto target = wall_origin_
                + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>((global - sim_origin_) / speed));
            if (target > Clock::now() + std::chrono::milliseconds(1)) {
                std::this_thread::sleep_until(target);
            }
        }
        // Achieved factor over the last ~2 s.
        const double wall = seconds_since(wall_origin_);
        if (history_.empty() || wall - history_.back().first >= 0.05) {
            history_.emplace_back(wall, global);
            while (history_.size() > 2 && history_.back().first - history_.front().first > 2.0) {
                history_.erase(history_.begin());
            }
            if (history_.size() >= 2) {
                const double dw = history_.back().first - history_.front().first;
                std::lock_guard lock(mutex_);
                status_.achieved_factor = dw > 0 ? (history_.back().second - history_.front().second) / dw : 0.0;
            }
        }
    });

    const double segment = std::max(settings_.segment, settings_.max_step * 10);
    double t0 = 0.0;
    while (!stop_requested_) {
        const auto deck = segment_deck(t0, segment, true);
        if (!run_segment(*engine, deck, t0, segment)) {
            break;
        }
        t0 += segment;
        std::lock_guard lock(mutex_);
        status_.segments += 1;
    }
    engine->set_data_handler(nullptr);
    engine->set_source_handler(nullptr);
    std::lock_guard lock(mutex_);
    if (status_.state != RunState::Failed) {
        status_.state = RunState::Stopped;
    }
}

void SimulationSession::run_transient()
{
    auto& host = EngineHost::instance();
    std::lock_guard engine_lock(host.lock());
    std::string error;
    NgspiceSession* engine = host.acquire(error);
    if (!engine) {
        fail("The analog engine is not available: " + error, nullptr);
        return;
    }
    const double stop_time = settings_.transient_stop;
    engine->set_source_handler(nullptr);
    engine->set_data_handler([this, engine, stop_time](const AcceptedPoint& point) {
        if (!index_valid_) {
            const auto names = engine->vector_names();
            index_.assign(names.size(), -1);
            for (std::size_t i = 0; i < names.size(); ++i) {
                const auto it = std::find(save_vectors_.begin(), save_vectors_.end(), lower(names[i]));
                if (it != save_vectors_.end()) {
                    index_[i] = static_cast<int>(it - save_vectors_.begin());
                }
            }
            index_valid_ = true;
        }
        last_time_ = point.time;
        std::lock_guard lock(mutex_);
        const bool keep = transient_.time.size() < request_.transient_limit;
        if (keep) {
            transient_.time.push_back(point.time);
        } else {
            transient_.truncated = true;
        }
        for (std::size_t i = 0; i < index_.size() && i < point.values.size(); ++i) {
            if (index_[i] < 0) {
                continue;
            }
            const auto& name = save_vectors_[static_cast<std::size_t>(index_[i])];
            latest_[name] = point.values[i];
            if (const auto ring = rings_.find(name); ring != rings_.end()) {
                ring->second->push(point.time, point.values[i]);
                if (keep) {
                    transient_.vectors[name].push_back(point.values[i]);
                }
            }
        }
        status_.sim_time = point.time;
        status_.progress = stop_time > 0 ? std::min(1.0, point.time / stop_time) : 0.0;
    });
    const bool complete = run_segment(*engine, segment_deck(0.0, stop_time, false), 0.0, stop_time);
    engine->set_data_handler(nullptr);
    std::lock_guard lock(mutex_);
    if (status_.state != RunState::Failed) {
        status_.state = complete ? RunState::Finished : RunState::Stopped;
    }
}

} // namespace simnodus::sim
