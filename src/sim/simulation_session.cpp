// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "simulation_session.hpp"

#include "engine_host.hpp"
#include "ngspice_runtime.hpp"

#include "model/engineering.hpp"

#include <algorithm>
#include <limits>
#include <cctype>
#include <cmath>
#include <filesystem>
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
    netlist_ = netlist::generate_netlist(
        request_.schematic, *request_.library, {interactive, 0.0, request_.current_probes});
    diagnostics_ = netlist_.diagnostics;
    if (!netlist_.ok()) {
        error = "The circuit has errors; see the Messages panel.";
        return false;
    }
    if (netlist_.elements.empty()) {
        error = "The schematic is empty.";
        return false;
    }
    if (!interactive && !request_.mcus.empty()) {
        error = "Transient mode does not run microcontrollers yet; use interactive mode.";
        return false;
    }
    mcus_.clear();
    external_refs_.clear();
    mcu_caches_.clear();
    serial_links_.clear();
    debug_resumes_ = 0;
    analog_points_.clear();
    edge_feeds_.clear();
    mcu_stale_ = false;
    input_watches_.clear();
    request_.mcu_lead_us = std::max<std::uint64_t>(static_cast<std::uint64_t>(std::llround(settings_.mcu_quantum * 1e6)), 1);
    // With only the own emulator, the firmware leads by its (smaller) window, which
    // the deterministic coupling needs (NativeMcuRunner, SN-173).
    if (!request_.mcus.empty()
        && std::all_of(request_.mcus.begin(), request_.mcus.end(), [](const McuRequest& m) { return m.engine == "native"; })) {
        for (const auto& m : request_.mcus) {
            request_.mcu_lead_us = std::min(request_.mcu_lead_us, m.options.chunk_us);
        }
    }
    // Serial links between boards: a TX pin and an RX pin on one net.
    for (const auto& a : request_.mcus) {
        for (const auto& b : request_.mcus) {
            if (a.component == b.component) {
                continue;
            }
            for (const auto& [ua, pins_a] : a.uart_pins) {
                for (const auto& [ub, pins_b] : b.uart_pins) {
                    const auto tx = netlist_.pin_node.find({a.component, pins_a.first});
                    const auto rx = netlist_.pin_node.find({b.component, pins_b.second});
                    if (tx != netlist_.pin_node.end() && rx != netlist_.pin_node.end() && tx->second == rx->second) {
                        serial_links_.push_back({a.component, ua, b.component, ub, 0});
                    }
                }
            }
        }
    }
    request_.debugging = std::any_of(request_.mcus.begin(), request_.mcus.end(),
        [](const McuRequest& m) { return m.options.gdb_port >= 0; });
    for (const auto& m : request_.mcus) {
        std::unique_ptr<mcu::McuBackend> runner;
        if (m.engine == "native") {
            auto options = m.options;
            // ADC samples read the circuit at a time it has passed (SN-178).
            options.analog_lag_ns = request_.mcu_lead_us * 1000
                + static_cast<std::uint64_t>(std::llround(std::max(settings_.max_step, 0.0) * 1e9));
            runner = std::make_unique<mcu::NativeMcuRunner>(options);
        } else {
            runner = std::make_unique<mcu::RenodeMcuRunner>(m.options);
        }
        // Circuit-to-firmware direction (MCU-04): every GPIO pin's voltage to the board ground.
        const auto ground = netlist_.pin_node.find({m.component, m.ground_pin});
        const auto& names = runner->pin_names();
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto node = netlist_.pin_node.find({m.component, names[i]});
            if (node != netlist_.pin_node.end()) {
                input_watches_.push_back({runner.get(), i, lower(node->second),
                    ground == netlist_.pin_node.end() ? std::string("0") : lower(ground->second), m.options.pin_model});
            }
        }
        analog_points_[runner.get()].assign(names.size(), std::nan(""));
        mcus_[m.component] = std::move(runner);
    }
    // Pin driver sources of the boards: "pa0_g" / "pa0_i" -> pin PA0, conductance / current.
    for (const auto& e : netlist_.externals) {
        const auto runner = mcus_.find(e.component);
        const auto split = e.name.rfind('_');
        if (runner == mcus_.end() || split == std::string::npos) {
            continue;
        }
        std::string pin = e.name.substr(0, split);
        for (auto& ch : pin) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        const auto& names = runner->second->pin_names();
        const auto index = std::find(names.begin(), names.end(), pin);
        if (index == names.end()) {
            continue;
        }
        if (!mcu_caches_.count(e.component)) {
            mcu_caches_[e.component] = {};
        }
        external_refs_[lower(e.source)] = {e.component, static_cast<std::size_t>(index - names.begin()),
            e.name.substr(split + 1) == "g"};
    }
    for (const auto& m : request_.mcus) {
        auto* runner = mcus_.at(m.component).get();
        if (!settings_.exact_edges || !runner->reports_edges_ahead()) {
            continue;
        }
        EdgeFeed feed;
        feed.runner = runner;
        feed.window_ns = std::max<std::uint64_t>(m.options.chunk_us, 1) * 1000;
        for (const auto& [source, ref] : external_refs_) {
            if (ref.component == m.component
                && std::find(feed.pins.begin(), feed.pins.end(), ref.pin) == feed.pins.end()) {
                feed.pins.push_back(ref.pin);
            }
        }
        if (!feed.pins.empty()) {
            edge_feeds_.push_back(std::move(feed));
        }
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
    for (const auto& [pin, vector] : netlist_.pin_current) {
        request_.vectors.push_back(vector);
    }
    for (const auto& v : request_.vectors) {
        add(v);
    }
    for (const auto& w : input_watches_) {
        add(w.node);
        add(w.ground);
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
        bouncing_sources_.clear();
        bounces_.clear();
        for (const auto& c : netlist_.controls) {
            controls_[lower(c.source)] = c.initial;
            control_source_[c.component] = lower(c.source);
            if (c.bounce) {
                bouncing_sources_.insert(lower(c.source));
            }
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
        const double previous = controls_[it->second];
        if (bouncing_sources_.count(it->second) && previous != value) {
            bounces_[it->second] = {status_.sim_time, previous};
        }
        controls_[it->second] = value;
    }
}

static double bounce_value(double since_change, double value, double previous)
{
    // Intervals (ms after the change) in which the contact is back in its previous
    // state: a fixed pattern, so runs are reproducible.
    static constexpr double open[][2] = {{0.3, 0.6}, {1.0, 1.5}, {2.1, 2.4}, {3.0, 3.2}};
    const double ms = since_change * 1e3;
    for (const auto& interval : open) {
        if (ms >= interval[0] && ms < interval[1]) {
            return previous;
        }
    }
    return value;
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
    RunStatus status;
    {
        std::lock_guard lock(mutex_);
        status = status_;
    }
    if (status.state == RunState::Running || status.state == RunState::Paused) {
        for (const auto& [id, runner] : mcus_) {
            const auto mcu = runner->status();
            status.debugger_waiting = status.debugger_waiting || mcu.debug_waiting;
            status.debugger_halted = status.debugger_halted || mcu.debug_halted;
            if (status.gdb_port < 0) {
                status.gdb_port = mcu.gdb_port;
            }
        }
    }
    return status;
}

std::vector<Sample> SimulationSession::samples(const std::string& vector) const
{
    std::lock_guard lock(mutex_);
    const auto it = rings_.find(lower(vector));
    return it == rings_.end() ? std::vector<Sample>{} : it->second->snapshot();
}

std::vector<Sample> SimulationSession::samples_since(const std::string& vector, double from) const
{
    std::lock_guard lock(mutex_);
    const auto it = rings_.find(lower(vector));
    return it == rings_.end() ? std::vector<Sample>{} : it->second->since(from);
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
    const auto n =
        netlist::generate_netlist(request_.schematic, *request_.library, {interactive, t0, request_.current_probes});
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
    if (interactive) {
        // Gear integration: switches, buttons (with bounce) and GPIO edges change
        // abruptly; the trapezoidal default rings there (a bouncing button over
        // 1 uF swung its node to -2.4 V). Transient mode keeps the default.
        extra.push_back(".options method=gear");
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
    for (auto& feed : edge_feeds_) {
        feed.done_ns = static_cast<std::uint64_t>(std::llround(t0 * 1e9)); // a new transient has no breakpoints
    }
    step_limit_ = std::numeric_limits<double>::infinity();
    edge_breakpoints_.clear();
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

bool build_mcu_requests(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const McuEnvironment& environment, std::vector<McuRequest>& requests, std::string& error)
{
    requests.clear();
    for (const auto& c : schematic.components()) {
        const auto* d = library.find(c.type);
        if (!d || !d->mcu) {
            continue;
        }
        const auto found = c.parameters.find(d->mcu->firmware_parameter);
        const std::string firmware = found == c.parameters.end() ? std::string() : found->second;
        // With debugging, a board may start without firmware: the debugger's `load`
        // writes it, as with an ST-LINK.
        const bool loaded_by_debugger = firmware.empty() && environment.gdb_port >= 0;
        if (firmware.empty() && !loaded_by_debugger) {
            error = c.designator + " has no firmware: choose an ELF file in its properties.";
            return false;
        }
        std::filesystem::path path(std::u8string(firmware.begin(), firmware.end())); // UTF-8 text
        if (!loaded_by_debugger && path.is_relative()) {
            path = environment.project_dir / path;
        }
        std::error_code ec;
        if (!loaded_by_debugger && !std::filesystem::is_regular_file(path, ec)) {
            const auto text = path.u8string();
            error = c.designator + ": the firmware file " + std::string(text.begin(), text.end()) + " does not exist.";
            return false;
        }
        McuRequest request;
        request.component = c.id;
        request.engine = environment.engine.empty() ? d->mcu->engine : environment.engine;
        if (request.engine == "renode" && (!environment.renode || !environment.resources)) {
            error = "The microcontroller engine (Renode) was not found next to the application.";
            return false;
        }
        auto& machine = request.options.machine;
        machine.renode = environment.renode.value_or(std::filesystem::path());
        machine.resources = environment.resources.value_or(std::filesystem::path());
        machine.platform = d->mcu->platform;
        machine.extensions = d->mcu->extensions;
        for (const auto& uart : d->mcu->uarts) {
            request.options.uarts.push_back(uart.name);
            if (!uart.tx_pin.empty()) {
                request.options.uart_tx_pins[uart.name] = uart.tx_pin;
            }
            request.uart_pins[uart.name] = {uart.tx_pin, uart.rx_pin};
        }
        machine.firmware = path;
        machine.before_firmware = d->mcu->before_firmware;
        machine.after_firmware = d->mcu->after_firmware;
        request.options.pin_model = {d->mcu->pin_model.supply, d->mcu->pin_model.output_resistance,
            d->mcu->pin_model.pull_resistance, d->mcu->pin_model.input_high, d->mcu->pin_model.input_low};
        request.ground_pin = d->mcu->ground_pin;
        const auto quantum = static_cast<std::uint64_t>(std::llround(schematic.settings().mcu_quantum * 1e6));
        request.options.chunk_us = std::max<std::uint64_t>(quantum, 1);
        if (request.engine == "native") {
            // Measured (SN-173, EQ-3): windows of 1 ms, 100 us and 20 us run at 2.48x,
            // 2.40x and 2.32x real time; 100 us keeps inputs late by less than 0.1 ms.
            request.options.chunk_us = std::min<std::uint64_t>(request.options.chunk_us, native_window_us);
        }
        for (const auto& pin : d->pins) {
            if (mcu::gpio_pin(pin.name)) {
                request.options.pins.push_back(pin.name);
            }
        }
        if (environment.gdb_port >= 0) {
            if (request.engine != "native") {
                error = c.designator + ": debugging needs SimNodus's own emulator; turn off Simulate > Use Renode "
                                       "for Microcontrollers.";
                return false;
            }
            request.options.gdb_port = environment.gdb_port + static_cast<int>(requests.size());
        }
        requests.push_back(std::move(request));
    }
    return true;
}

double SimulationSession::external_value(const ExternalRef& ref, double time)
{
    // Engine thread only. One drive table per board and trial step; the firmware
    // must have reached the circuit's time before its outputs are known. The
    // drive is averaged over the step from the last accepted point (SN-176): an
    // edge inside the step contributes its exact share instead of acting at the
    // step's end, which made a 1 kHz PWM average 20-30 % instead of 25 %.
    auto& cache = mcu_caches_[ref.component];
    const auto time_ns = static_cast<std::uint64_t>(std::llround(std::max(0.0, time) * 1e9));
    const auto from_ns = static_cast<std::uint64_t>(std::llround((segment_t0_ + last_time_) * 1e9));
    if (cache.time_ns != time_ns || cache.from_ns != from_ns || cache.drives.empty()) {
        auto& runner = *mcus_.at(ref.component);
        // Never wait for a firmware that may not run. When the firmware reaches this
        // limit it collects held events and reports the complete time.
        if (!stop_requested_) { // after Stop the firmware stays where it is
            runner.set_limit_ns(time_ns + request_.mcu_lead_us * 1000);
        }
        const bool reached = runner.wait_time_ns(time_ns, stop_requested_);
        cache.drives = runner.average_drives(from_ns, time_ns);
        // Only after a stop (or a failed firmware): the levels are stale.
        mcu_stale_ = mcu_stale_ || !reached;
        cache.time_ns = time_ns;
        cache.from_ns = from_ns;
    }
    if (ref.pin >= cache.drives.size()) {
        return 0.0;
    }
    return ref.conductance ? cache.drives[ref.pin].conductance : cache.drives[ref.pin].current;
}

void SimulationSession::pass_serial_bytes()
{
    for (auto& link : serial_links_) {
        std::string bytes;
        const std::uint64_t total = mcus_.at(link.from)->uart_received(link.from_uart, link.sent, bytes);
        if (!bytes.empty()) {
            mcus_.at(link.to)->uart_send(link.to_uart, bytes); // arrive one frame apart, as on the wire
        }
        link.sent = total;
    }
}

void SimulationSession::feed_edges(NgspiceSession& engine, double point_time)
{
    if (edge_feeds_.empty() || !engine.supports_breakpoints() || stop_requested_) {
        return;
    }
    const double global = segment_t0_ + point_time;
    const auto now_ns = static_cast<std::uint64_t>(std::llround(global * 1e9));
    std::uint64_t known_ns = std::numeric_limits<std::uint64_t>::max();
    for (auto& feed : edge_feeds_) {
        // The firmware runs to the window boundary below its limit; wait for it so the
        // edges up to there are known before ngspice chooses its next step.
        const std::uint64_t limit = now_ns + request_.mcu_lead_us * 1000;
        const std::uint64_t horizon = limit / feed.window_ns * feed.window_ns;
        if (horizon <= feed.done_ns) {
            known_ns = std::min(known_ns, feed.done_ns);
            continue;
        }
        feed.runner->set_limit_ns(limit);
        if (!feed.runner->wait_time_ns(horizon, stop_requested_)) {
            return;
        }
        for (const std::uint64_t edge : feed.runner->drive_changes(feed.pins, feed.done_ns, horizon)) {
            // A time point 0.7 ns before the edge still reads the old level (drives
            // are looked up at whole ns); every later point reads the new one.
            const double t = static_cast<double>(edge) * 1e-9 - 0.7e-9 - segment_t0_;
            if (t > point_time && engine.set_breakpoint(t)) {
                edge_breakpoints_.push_back(t);
            }
        }
        feed.done_ns = horizon;
        known_ns = std::min(known_ns, horizon);
    }
    // Steps stop where the firmware's edges are known; a capped step, unlike a
    // breakpoint, does not make ngspice restart with small steps.
    const double limit = static_cast<double>(known_ns) * 1e-9 - segment_t0_;
    step_limit_ = limit > point_time ? limit : std::numeric_limits<double>::infinity();
}

bool SimulationSession::start_mcus()
{
    if (mcus_.empty()) {
        return true;
    }
    {
        std::lock_guard lock(mutex_);
        status_.starting_mcu = true;
    }
    for (auto& [id, runner] : mcus_) {
        runner->start();
    }
    for (auto& [id, runner] : mcus_) {
        if (!runner->wait_ready(stop_requested_) && !stop_requested_) {
            fail("The microcontroller could not start. " + runner->status().message, nullptr);
            return false;
        }
    }
    std::lock_guard lock(mutex_);
    status_.starting_mcu = false;
    return !stop_requested_;
}

void SimulationSession::stop_mcus()
{
    for (auto& [id, runner] : mcus_) {
        runner->stop();
    }
    std::lock_guard lock(mutex_);
    status_.starting_mcu = false;
}

std::optional<std::string> SimulationSession::mcu_failure() const
{
    for (const auto& [id, runner] : mcus_) {
        const auto status = runner->status();
        if (status.state == mcu::McuState::Failed) {
            return status.message;
        }
    }
    return std::nullopt;
}

std::optional<mcu::McuStatus> SimulationSession::mcu_status(model::Id component) const
{
    const auto it = mcus_.find(component);
    return it == mcus_.end() ? std::nullopt : std::optional<mcu::McuStatus>(it->second->status());
}

std::vector<std::pair<model::Id, std::string>> SimulationSession::take_mcu_diagnostics()
{
    std::vector<std::pair<model::Id, std::string>> result;
    for (auto& [id, runner] : mcus_) {
        for (auto& text : runner->take_diagnostics()) {
            result.emplace_back(id, std::move(text));
        }
    }
    return result;
}

std::map<std::string, mcu::PinState> SimulationSession::mcu_pins(model::Id component) const
{
    const auto it = mcus_.find(component);
    return it == mcus_.end() ? std::map<std::string, mcu::PinState>{} : it->second->pins();
}

void SimulationSession::mcu_uart_send(model::Id component, const std::string& uart, const std::string& bytes)
{
    if (const auto it = mcus_.find(component); it != mcus_.end() && active()) {
        it->second->uart_send(uart, bytes);
    }
}

std::uint64_t SimulationSession::mcu_uart_received(model::Id component, const std::string& uart, std::uint64_t from,
    std::string& bytes, std::vector<std::uint64_t>* times) const
{
    const auto it = mcus_.find(component);
    if (it == mcus_.end()) {
        bytes.clear();
        return 0;
    }
    return it->second->uart_received(uart, from, bytes, times);
}

mcu::PinLog SimulationSession::mcu_pin_log(model::Id component, const std::string& pin, std::uint64_t from_ns) const
{
    const auto it = mcus_.find(component);
    if (it == mcus_.end()) {
        return {};
    }
    const auto& names = it->second->pin_names();
    const auto index = std::find(names.begin(), names.end(), pin);
    if (index == names.end()) {
        return {};
    }
    return it->second->pin_log(static_cast<std::size_t>(index - names.begin()), from_ns);
}

void SimulationSession::mcu_reset(model::Id component)
{
    if (const auto it = mcus_.find(component); it != mcus_.end() && active()) {
        it->second->request_reset();
    }
}

void SimulationSession::run_interactive()
{
    // Renode starts first (seconds); the circuit waits for the firmware.
    if (!start_mcus()) {
        stop_mcus();
        std::lock_guard lock(mutex_);
        if (status_.state != RunState::Failed) {
            status_.state = RunState::Stopped;
        }
        return;
    }
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

    engine->set_step_handler([this](double time, double& delta) {
        if (edge_feeds_.empty()) {
            return;
        }
        // After a breakpoint ngspice restarts with a tenth of the step and doubles
        // it per step: about 11 extra time points per edge. After an edge breakpoint
        // the step goes back to the maximum step; ngspice's error control still
        // shortens a step that is too long (ADR 0093).
        while (!edge_breakpoints_.empty() && edge_breakpoints_.front() < time - 1e-9) {
            edge_breakpoints_.pop_front();
        }
        if (!edge_breakpoints_.empty() && edge_breakpoints_.front() <= time + 1e-9) {
            edge_breakpoints_.pop_front();
            delta = std::max(delta, settings_.max_step);
            if (!edge_breakpoints_.empty()) {
                delta = std::min(delta, edge_breakpoints_.front() - time);
            }
        }
        if (time + delta > step_limit_ && step_limit_ > time) {
            delta = step_limit_ - time;
        }
    });
    engine->set_source_handler([this](const std::string& source, double time) {
        const std::string key = lower(source);
        if (const auto e = external_refs_.find(key); e != external_refs_.end()) {
            return external_value(e->second, segment_t0_ + time);
        }
        std::lock_guard lock(mutex_);
        const auto it = controls_.find(key);
        if (it == controls_.end()) {
            return 0.0;
        }
        if (const auto b = bounces_.find(key); b != bounces_.end()) {
            const double since = segment_t0_ + time - b->second.start;
            if (since >= 0 && since < 3.5e-3) {
                return bounce_value(since, it->second, b->second.previous);
            }
        }
        return it->second;
    });
    engine->set_data_handler([this, engine, &last_speed](const AcceptedPoint& point) {
        // Points computed after the firmware stopped would show stale pin levels.
        if (mcu_stale_) {
            return;
        }
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
            // Input pins: Schmitt trigger on the pin voltage; changes go to the firmware
            // before its next quantum (sampled exchange, labeled approximate).
            const auto global_us = static_cast<std::uint64_t>(std::llround(global * 1e6));
            for (auto& [runner, volts] : analog_points_) {
                std::fill(volts.begin(), volts.end(), std::nan(""));
            }
            for (auto& w : input_watches_) {
                const auto v = latest_.find(w.node);
                const auto g = w.ground == "0" ? latest_.end() : latest_.find(w.ground);
                if (v == latest_.end()) {
                    continue;
                }
                const double volts = v->second - (g == latest_.end() ? 0.0 : g->second);
                auto& pins = analog_points_[w.runner];
                if (w.pin < pins.size()) {
                    pins[w.pin] = volts;
                }
                const bool level = mcu::schmitt(w.level, volts, w.model);
                if (level != w.level || !w.sent) {
                    w.level = level;
                    w.sent = true;
                    w.runner->set_input(w.pin, level, global_us * 1000);
                }
            }
        }
        // Pin voltages for the ADC, sent before the limit lets the firmware run on.
        const auto global_ns = static_cast<std::uint64_t>(std::llround(global * 1e9));
        for (const auto& [runner, volts] : analog_points_) {
            runner->set_analog(global_ns, volts);
        }
        // Lockstep window: the firmware stays within mcu_lead_us of the circuit in
        // both directions (the circuit waits for a slower firmware).
        const auto analog_us = static_cast<std::uint64_t>(global * 1e6);
        for (auto& [id, runner] : mcus_) {
            runner->set_limit_ns((analog_us + request_.mcu_lead_us) * 1000);
            if (analog_us > request_.mcu_lead_us) {
                runner->wait_time_ns((analog_us - request_.mcu_lead_us) * 1000, stop_requested_);
            }
        }
        feed_edges(*engine, point.time);
        pass_serial_bytes();
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
        // A debugger halted and resumed a firmware: the wall time it stood still
        // is not to be caught up.
        if (request_.debugging) {
            int resumes = 0;
            for (const auto& [id, runner] : mcus_) {
                resumes += runner->status().debug_resumes;
            }
            if (resumes != debug_resumes_) {
                debug_resumes_ = resumes;
                wall_origin_ = Clock::now();
                sim_origin_ = global;
                history_.clear();
            }
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
        {
            std::lock_guard lock(mutex_);
            status_.segments += 1;
        }
        if (const auto failure = mcu_failure()) {
            fail(*failure, nullptr);
            break;
        }
    }
    engine->set_data_handler(nullptr);
    engine->set_source_handler(nullptr);
    engine->set_step_handler(nullptr);
    stop_mcus();
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
