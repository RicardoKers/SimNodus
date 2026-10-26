// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "run_controller.hpp"

#include "document.hpp"
#include "engine_process.hpp"
#include "schematic_scene.hpp"

#include "library/component_library.hpp"
#include "library/spice_template.hpp"
#include "model/engineering.hpp"
#include "sim/engine_host.hpp"
#include "sim/remote_session.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace simnodus::ui {
namespace {

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// Brightness 0..1 as the eye sees a current: dark below 1/10000 of the full
// current, then 40 % at once and the rest growing with the square root
// (an LED at a tenth of its current still looks clearly lit, and a multiplexed
// one at a few percent is visible, as on a bench).
double perceived(double current, double full)
{
    if (full <= 0) {
        return 0.0;
    }
    const double ratio = std::abs(current) / full;
    if (ratio < 1e-4) {
        return 0.0;
    }
    return 0.4 + 0.6 * std::sqrt(std::min(ratio, 1.0));
}

QString quantity(double value, const char* unit)
{
    return QString::fromStdString(model::format_engineering(value, 3)) + QString::fromLatin1(unit);
}

// Expands "{ref}" in a library readout element name.
std::string element_name(const std::string& pattern, const std::string& designator)
{
    std::string result = pattern;
    const auto at = result.find("{ref}");
    if (at != std::string::npos) {
        result.replace(at, 5, netlist::sanitize_name(designator));
    }
    return result;
}

} // namespace

RunController::RunController(Document& document, SchematicScene& scene, QObject* parent)
    : QObject(parent), document_(document), scene_(scene)
{
    // Runs happen in the engine process (SN-151, ADR 0101), so that a crash of
    // ngspice or of the emulator cannot close the editor. SIMNODUS_ENGINE=inprocess
    // runs them in this process instead (debugging).
    if (qEnvironmentVariable("SIMNODUS_ENGINE") == QStringLiteral("inprocess")) {
        session_ = std::make_unique<sim::SimulationSession>();
    } else {
        sim::protocol::EngineSetup setup;
        setup.application_dir = QCoreApplication::applicationDirPath().toStdWString();
        for (const auto& folder : library::library_candidates(setup.application_dir)) {
            std::error_code ec;
            if (std::filesystem::is_directory(folder, ec)) {
                setup.library_dir = folder;
                break;
            }
        }
        setup.ngspice = sim::EngineHost::instance().location();
        engine_ = std::make_unique<EngineProcess>();
        session_ = std::make_unique<sim::RemoteSession>(*engine_, setup);
    }
    timer_.setInterval(50); // live display at 20 Hz (INT-03)
    connect(&timer_, &QTimer::timeout, this, &RunController::refresh);
}

RunController::~RunController()
{
    session_->stop();
}

bool RunController::start(QString& error, const sim::RunRequest* base)
{
    stop();
    snapshot_ = base ? base->schematic : document_.schematic();
    mode_ = snapshot_.settings().mode;
    const auto& library = document_.library();

    sim::RunRequest request;
    if (base) {
        request.vectors = base->vectors;
        request.current_probes = base->current_probes;
        request.ring_capacity = base->ring_capacity;
    }
    request.schematic = snapshot_;
    request.library = &library;
    // Every net for tooltips and wire colors, plus meter and LED currents.
    const auto connectivity = netlist::build_connectivity(snapshot_, library);
    for (const auto& net : connectivity.nets) {
        if (!net.ground) {
            request.vectors.push_back(lower(net.name));
        }
    }
    readout_vectors_.clear();
    switch_states_.clear();
    light_vectors_.clear();
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (d && d->lights) {
            // Display segments (SN-202): the current of each one's measuring source.
            auto& vectors = light_vectors_[c.id];
            for (const auto& light : d->lights->lights) {
                vectors.push_back(lower(element_name(light.element, c.designator)) + "#branch");
                request.vectors.push_back(vectors.back());
            }
        }
        if (d && d->readout && d->readout->kind != library::Readout::Kind::Voltage) {
            // Current: the branch of a measuring source; node: an internal node (motor speed).
            const bool current = d->readout->kind == library::Readout::Kind::Current;
            const auto vector = lower(element_name(d->readout->element, c.designator)) + (current ? "#branch" : "");
            readout_vectors_[c.id] = vector;
            request.vectors.push_back(vector);
        }
    }
    for (const auto& p : snapshot_.probes()) {
        const netlist::PinRef ref{p.target.item, p.target.pin};
        if (p.kind == model::ProbeKind::Current && snapshot_.component(p.target.item) &&
            std::find(request.current_probes.begin(), request.current_probes.end(), ref) == request.current_probes.end()) {
            request.current_probes.push_back(ref);
        }
    }
    std::sort(request.vectors.begin(), request.vectors.end());
    request.vectors.erase(std::unique(request.vectors.begin(), request.vectors.end()), request.vectors.end());
    std::string reason;
    // Microcontroller boards: firmware relative to the project file.
    sim::McuEnvironment environment;
    const QString folder = document_.base_dir();
    environment.project_dir = std::filesystem::path((folder.isEmpty() ? QDir::currentPath() : folder).toStdWString());
    environment.gdb_port = gdb_port_;
    std::vector<std::string> warnings;
    if (!sim::build_mcu_requests(snapshot_, library, environment, request.mcus, reason, &warnings)) {
        error = QString::fromStdString(reason);
        emit state_changed();
        return false;
    }
    for (const auto& text : warnings) {
        emit mcu_message(model::no_id, QString::fromStdString(text));
    }
    // Digital devices (SN-203) get logic lanes after their board's pins.
    device_lanes_.clear();
    for (const auto& m : request.mcus) {
        for (const auto& d : m.options.devices) {
            for (const auto& pin : d.pins) {
                device_lanes_.emplace_back(m.component, d.designator + "." + pin);
            }
        }
    }
    if (!session_->start(std::move(request), reason)) {
        error = QString::fromStdString(reason);
        emit state_changed();
        return false;
    }
    build_channels();
    last_state_ = sim::RunState::Running;
    timer_.start();
    emit started();
    emit state_changed();
    return true;
}

void RunController::build_channels()
{
    channels_.clear();
    const auto& n = session_->netlist();
    const auto& library = document_.library();
    auto net_of = [&](const model::Anchor& a) -> std::optional<std::string> {
        if (const auto* c = snapshot_.component(a.item)) {
            const auto it = n.pin_node.find({c->id, a.pin});
            if (it != n.pin_node.end()) {
                return lower(it->second);
            }
            return std::nullopt;
        }
        const auto it = n.connectivity.wire_net.find(a.item);
        if (it != n.connectivity.wire_net.end()) {
            return lower(n.connectivity.nets[static_cast<std::size_t>(it->second)].name);
        }
        return std::nullopt;
    };
    int index = 0;
    for (const auto& p : snapshot_.probes()) {
        Channel ch;
        ch.probe = p.id;
        ch.color = p.color.empty() ? probe_color(index) : QColor(QString::fromStdString(p.color));
        ++index;
        if (p.kind == model::ProbeKind::Voltage) {
            ch.unit = QStringLiteral("V");
            const auto net = net_of(p.target);
            ch.valid = net.has_value();
            ch.vector = net.value_or("");
            QString name = QStringLiteral("V(%1").arg(QString::fromStdString(net.value_or("?")).toUpper());
            if (p.reference) {
                const auto ref = net_of(*p.reference);
                ch.reference = ref.value_or("");
                ch.valid = ch.valid && ref.has_value();
                name += QStringLiteral(",%1").arg(QString::fromStdString(ref.value_or("?")).toUpper());
            }
            ch.name = name + QStringLiteral(")");
        } else {
            ch.unit = QStringLiteral("A");
            const auto* c = snapshot_.component(p.target.item);
            const auto it = n.pin_current.find({p.target.item, p.target.pin});
            ch.valid = c && it != n.pin_current.end();
            ch.vector = ch.valid ? it->second : std::string();
            ch.name = QStringLiteral("I(%1.%2)").arg(
                c ? QString::fromStdString(c->designator) : QStringLiteral("?"), QString::fromStdString(p.target.pin));
        }
        (void)library;
        channels_.push_back(ch);
    }
}

void RunController::pause_or_resume()
{
    const auto s = session_->status().state;
    if (s == sim::RunState::Running) {
        session_->pause();
        tone_.play(0.0);
    } else if (s == sim::RunState::Paused) {
        session_->resume();
    }
}

void RunController::stop()
{
    session_->stop();
    timer_.stop();
    tone_.play(0.0); // the refresh that would silence it does not come any more
    scene_.clear_live();
    if (last_state_ != sim::RunState::Idle) {
        last_state_ = session_->status().state;
        emit state_changed();
    }
}

qint64 RunController::engine_process_id() const
{
    return engine_ ? engine_->process_id() : 0;
}

bool RunController::active() const
{
    return session_->active();
}

bool RunController::apply_live_parameters()
{
    if (!active()) {
        return false;
    }
    bool applied = false;
    const auto& library = document_.library();
    for (const auto& original : std::vector<model::Component>(snapshot_.components())) {
        model::Component c = original;
        const auto* d = library.find(c.type);
        const auto* now = document_.schematic().component(c.id);
        if (!d || !d->device || !now) {
            continue;
        }
        for (const auto& p : d->parameters) {
            if (!p.live) {
                continue;
            }
            const auto was = c.parameters.find(p.name);
            const auto is = now->parameters.find(p.name);
            const std::string old_value = was == c.parameters.end() ? p.default_value : was->second;
            const std::string new_value = is == now->parameters.end() ? p.default_value : is->second;
            if (old_value != new_value) {
                session_->set_device_parameter(c.id, p.name, new_value);
                c.parameters[p.name] = new_value;
                snapshot_.put(c);
                applied = true;
            }
        }
    }
    return applied;
}

void RunController::set_sound(bool on)
{
    sound_ = on;
    if (!on) {
        tone_.play(0.0);
    }
}

void RunController::set_speed(double speed)
{
    session_->set_speed(speed);
}

void RunController::set_control(model::Id component, double value)
{
    session_->set_control(component, value);
    refresh(); // immediate visual feedback
}

std::optional<double> RunController::pin_voltage(model::Id component, const std::string& pin) const
{
    const auto& n = session_->netlist();
    const auto it = n.pin_node.find({component, pin});
    return it == n.pin_node.end() ? std::nullopt : net_voltage(lower(it->second));
}

std::vector<sim::Sample> RunController::samples(const Channel& channel, double from, double bucket) const
{
    if (!channel.valid) {
        return {};
    }
    const bool differential = !channel.reference.empty() && channel.reference != "0";
    auto values = session_->samples_since(channel.vector, from, differential ? 0.0 : bucket);
    if (differential) {
        const auto reference = session_->samples_since(channel.reference, from);
        // Both vectors come from the same accepted time points.
        const std::size_t n = std::min(values.size(), reference.size());
        const std::size_t skip_v = values.size() - n;
        const std::size_t skip_r = reference.size() - n;
        std::vector<sim::Sample> diff;
        diff.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            diff.push_back({values[skip_v + i].time, values[skip_v + i].value - reference[skip_r + i].value});
        }
        return diff;
    }
    return values;
}

std::optional<double> RunController::net_voltage(const std::string& net) const
{
    if (net == "0") {
        return 0.0;
    }
    return session_->latest(net);
}

std::vector<RunController::UartPort> RunController::uart_ports() const
{
    std::vector<UartPort> ports;
    const auto& library = document_.library();
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (!d || !d->mcu || !session_->mcu_status(c.id)) {
            continue;
        }
        for (const auto& uart : d->mcu->uarts) {
            ports.push_back({c.id, uart.name,
                QString::fromStdString(c.designator) + QLatin1Char(' ') + QString::fromStdString(uart.label)});
        }
    }
    return ports;
}

std::vector<RunController::LogicLane> RunController::logic_lanes() const
{
    std::vector<LogicLane> lanes;
    const auto& library = document_.library();
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (!d || !d->mcu || !session_->mcu_status(c.id)) {
            continue;
        }
        std::vector<std::pair<std::pair<char, int>, std::string>> pins; // (port, number) -> name
        for (const auto& p : d->pins) {
            if (p.name.size() >= 3 && p.name[0] == 'P') {
                pins.push_back({{p.name[1], std::atoi(p.name.c_str() + 2)}, p.name});
            }
        }
        std::sort(pins.begin(), pins.end()); // PA0, PA1, ..., PB0, ..., PC13
        for (const auto& [order, name] : pins) {
            lanes.push_back({c.id, name, -1, QString::fromStdString(c.designator + "." + name), QColor(0x2f, 0x5f, 0xd0)});
        }
        for (const auto& [board, name] : device_lanes_) {
            if (board == c.id) {
                lanes.push_back({c.id, name, -1, QString::fromStdString(name), QColor(0x1a, 0x8a, 0x5a)});
            }
        }
    }
    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        const auto& ch = channels_[static_cast<std::size_t>(i)];
        if (ch.unit == QStringLiteral("V") && ch.valid) {
            lanes.push_back({model::no_id, {}, i, ch.name, ch.color});
        }
    }
    return lanes;
}

RunController::LaneData RunController::lane_data(const LogicLane& lane, double from, double threshold) const
{
    LaneData data;
    if (lane.channel < 0) {
        const auto log = session_->mcu_pin_log(
            lane.board, lane.pin, static_cast<std::uint64_t>(std::llround(std::max(0.0, from) * 1e9)));
        data.initial = log.initial;
        for (const auto& [t, level] : log.changes) {
            data.changes.emplace_back(static_cast<double>(t) * 1e-9, level);
        }
        return data;
    }
    if (lane.channel >= static_cast<int>(channels_.size())) {
        return data;
    }
    const auto& channel = channels_[static_cast<std::size_t>(lane.channel)];
    if (channel.valid && (channel.reference.empty() || channel.reference == "0")) {
        // The ring finds the edges itself: no copy of the whole window.
        data.changes = session_->transitions_since(channel.vector, from, threshold, data.initial);
        return data;
    }
    const auto samples_in = samples(channel, from);
    if (samples_in.empty()) {
        return data;
    }
    bool level = samples_in.front().value > threshold;
    data.initial = level;
    for (const auto& s : samples_in) {
        const bool now = s.value > threshold;
        if (now != level) {
            data.changes.emplace_back(s.time, now);
            level = now;
        }
    }
    return data;
}

void RunController::mcu_reset(model::Id component)
{
    session_->mcu_reset(component);
}

double RunController::glow_current(const std::string& vector, double latest, double now) const
{
    // The eye averages a multiplexed LED over a few milliseconds (SN-204): the
    // mean magnitude over the last 20 ms, weighted by time; the latest value
    // when there are not enough samples.
    constexpr double window = 0.02;
    if (mode_ != model::SimulationMode::Interactive || now < window) {
        return std::abs(latest);
    }
    const auto samples = session_->samples_since(vector, now - window);
    if (samples.size() < 2) {
        return std::abs(latest);
    }
    double sum = 0.0;
    double span = 0.0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        const double from = std::max(samples[i - 1].time, now - window);
        const double dt = samples[i].time - from;
        if (dt > 0) {
            sum += std::abs(samples[i - 1].value) * dt;
            span += dt;
        }
    }
    return span > 0 ? sum / span : std::abs(latest);
}

void RunController::refresh()
{
    for (const auto& [board, text] : session_->take_mcu_diagnostics()) {
        emit mcu_message(board, QString::fromStdString(text));
    }
    const auto status = session_->status();
    LiveState live;
    live.running = status.state == sim::RunState::Running || status.state == sim::RunState::Paused
        || (status.state == sim::RunState::Finished && mode_ == model::SimulationMode::Transient);
    const auto latest = session_->latest_all();
    live.nets = latest;
    for (const auto& [id, text] : session_->device_texts()) {
        live.texts[id] = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }
    const auto& n = session_->netlist();
    const auto& library = document_.library();
    for (const auto& control : n.controls) {
        if (const auto v = session_->control(control.component)) {
            live.controls[control.component] = *v;
        }
    }
    for (const auto& [id, vectors] : light_vectors_) {
        const auto* c = snapshot_.component(id);
        const auto* d = c ? library.find(c->type) : nullptr;
        if (!d || !d->lights) {
            continue;
        }
        LiveState::Lights lit;
        lit.color = d->lights->color;
        if (!d->lights->color_parameter.empty()) {
            const auto p = c->parameters.find(d->lights->color_parameter);
            const auto* definition = d->parameter(d->lights->color_parameter);
            lit.color = p != c->parameters.end() ? p->second : definition ? definition->default_value : lit.color;
        }
        for (const auto& vector : vectors) {
            const auto it = latest.find(vector);
            lit.levels.push_back(
                it == latest.end() ? 0.0 : perceived(glow_current(vector, it->second, status.sim_time), d->lights->full));
        }
        live.lights[id] = std::move(lit);
    }
    double tone = 0.0; // the highest buzzer frequency (SN-228)
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (!d || !d->readout) {
            continue;
        }
        if (d->sound) {
            if (const auto it = latest.find(readout_vectors_[c.id]); it != latest.end()) {
                tone = std::max(tone, it->second * d->readout->scale);
            }
        }
        if (d->readout->kind == library::Readout::Kind::Node) {
            const auto it = latest.find(readout_vectors_[c.id]);
            if (it != latest.end()) {
                double value = it->second * d->readout->scale;
                if (std::abs(value) < 1e-6) {
                    value = 0.0; // numerical dust of a part at rest (a motor's speed), not a reading
                }
                live.readouts[c.id] = QStringLiteral("%1 %2")
                                          .arg(std::abs(value) >= 100 ? QString::number(value, 'f', 0) : QString::number(value, 'g', 3),
                                              QString::fromStdString(d->readout->unit));
            }
        } else if (d->readout->kind == library::Readout::Kind::Current) {
            const auto it = latest.find(readout_vectors_[c.id]);
            if (it == latest.end()) {
                continue;
            }
            if (d->readout->switches()) {
                // A relay's contact follows its coil current, with hysteresis like the part.
                const library::ValueLookup lookup = [&](std::string_view name) -> std::optional<double> {
                    const auto* p = d->parameter(std::string(name));
                    if (!p) {
                        return std::nullopt;
                    }
                    const auto v = c.parameters.find(p->name);
                    return model::parse_engineering(v == c.parameters.end() ? p->default_value : v->second, p->unit);
                };
                std::string error;
                const auto on = library::evaluate_expression(d->readout->switch_on, lookup, error);
                const auto off = library::evaluate_expression(d->readout->switch_off, lookup, error);
                bool& closed = switch_states_[c.id];
                const double current = std::abs(it->second);
                if (on && current >= *on) {
                    closed = true;
                } else if (off && current < *off) {
                    closed = false;
                }
                live.states[c.id] = closed ? QStringLiteral("1") : QStringLiteral("0");
            } else if (d->readout->glows()) {
                const auto& r = *d->readout;
                auto parameter = [&c](const std::string& name) {
                    const auto p = c.parameters.find(name);
                    return p == c.parameters.end() ? std::string() : p->second;
                };
                double full = r.glow_full;
                if (!r.glow_full_parameter.empty()) {
                    full = model::parse_engineering(parameter(r.glow_full_parameter), "A").value_or(0.0);
                }
                const std::string color = r.glow_color_parameter.empty() ? r.glow_color : parameter(r.glow_color_parameter);
                const double current = glow_current(readout_vectors_[c.id], it->second, status.sim_time);
                live.glow[c.id] = {perceived(current, full), color};
            } else {
                live.readouts[c.id] = quantity(it->second, "A");
            }
        } else {
            const auto plus = n.pin_node.find({c.id, d->readout->pins[0]});
            const auto minus = n.pin_node.find({c.id, d->readout->pins[1]});
            if (plus == n.pin_node.end() || minus == n.pin_node.end()) {
                continue;
            }
            const auto a = net_voltage(lower(plus->second));
            const auto b = net_voltage(lower(minus->second));
            if (a && b) {
                live.readouts[c.id] = quantity(*a - *b, "V");
            }
        }
    }
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (d && d->mcu) {
            live.mcu_pins[c.id] = session_->mcu_pins(c.id);
        }
    }
    tone_.play(sound_ && status.state == sim::RunState::Running ? tone : 0.0);
    scene_.set_live(std::move(live));
    emit refreshed();
    if (status.state != last_state_) {
        last_state_ = status.state;
        if (status.state == sim::RunState::Failed) {
            QStringList output;
            for (const auto& line : status.engine_output) {
                output << QString::fromStdString(line);
            }
            emit failed(QString::fromStdString(status.message), output);
        }
        if (status.state != sim::RunState::Running && status.state != sim::RunState::Paused) {
            timer_.stop();
            if (mode_ != model::SimulationMode::Transient) {
                scene_.clear_live();
            }
        }
        emit state_changed();
    }
}

QString RunController::mode_description() const
{
    const auto& s = snapshot_.settings();
    const auto step = QString::fromStdString(model::format_engineering(
        mode_ == model::SimulationMode::Interactive ? s.max_step : s.transient_max_step));
    if (mode_ == model::SimulationMode::Transient) {
        return tr("transient analysis, stop %1s, maximum step %2s")
            .arg(QString::fromStdString(model::format_engineering(s.transient_stop)), step);
    }
    QString text = tr("interactive (approximate): %1s segments, maximum step %2s, Gear integration; switch, button and "
                      "generator edges are resolved to the time step; semiconductor charge is not carried across segments")
                       .arg(QString::fromStdString(model::format_engineering(s.segment)), step);
    const auto& library = document_.library();
    const auto& components = snapshot_.components();
    if (std::any_of(components.begin(), components.end(), [&library](const model::Component& c) {
            const auto* d = library.find(c.type);
            return d && d->mcu;
        })) {
        text += tr("; microcontroller firmware runs in the SimNodus emulator at most 0.1 ms ahead of the circuit; "
                   "GPIO outputs drive the circuit through 40 Ohm, %1; circuit voltages reach GPIO inputs "
                   "through Schmitt thresholds (1.16 V / 1.83 V) less than 0.1 ms late, and the ADC reads them "
                   "0.1-0.2 ms back")
                    .arg(s.exact_edges ? tr("with a time point at every edge (exact edges)")
                                       : tr("averaged over each time step (an edge is spread over one step)"));
        text += s.cortex_m3_timing
            ? tr("; firmware instructions take the Cortex-M3 cycles with flash wait states (approximate, within 5 % "
                 "of a real Blue Pill in measured tests)")
            : tr("; firmware instructions take one cycle each (faster than a real chip)");
    }
    return text;
}

QString RunController::live_text(const std::set<model::Id>& selection) const
{
    if (!session_->active() && !(mode_ == model::SimulationMode::Transient && state() == sim::RunState::Finished)) {
        return {};
    }
    const auto& n = session_->netlist();
    const auto& library = document_.library();
    QStringList lines;
    for (model::Id id : selection) {
        // A probe: its reading, as in the Signal Analyzer.
        if (const auto ch = std::find_if(channels_.begin(), channels_.end(), [id](const Channel& c) { return c.probe == id; });
            ch != channels_.end()) {
            if (ch->valid) {
                auto value = session_->latest(ch->vector);
                if (value && !ch->reference.empty()) {
                    const auto reference = net_voltage(ch->reference);
                    value = reference ? std::optional<double>(*value - *reference) : std::nullopt;
                }
                if (value) {
                    lines << QStringLiteral("%1 = %2").arg(ch->name, quantity(*value, ch->unit == QStringLiteral("A") ? "A" : "V"));
                }
            }
            continue;
        }
        // A wire or a net label: the voltage of its net.
        const auto& wires = n.connectivity.wire_net;
        const auto& labels = n.connectivity.label_net;
        const auto net = wires.count(id) ? wires.at(id) : labels.count(id) ? labels.at(id) : -1;
        if (net >= 0) {
            const std::string name = n.connectivity.nets[static_cast<std::size_t>(net)].name;
            if (const auto v = net_voltage(lower(name))) {
                lines << QStringLiteral("V(%1) = %2").arg(QString::fromStdString(name).toUpper(), quantity(*v, "V"));
            }
            continue;
        }
        const auto* c = snapshot_.component(id);
        const auto* d = c ? library.find(c->type) : nullptr;
        if (!c || !d || !d->ties_net.empty()) {
            continue;
        }
        if (selection.size() > 1) {
            lines << QString::fromStdString(c->designator);
        }
        std::vector<double> volts;
        for (const auto& pin : d->pins) {
            const auto node = n.pin_node.find({c->id, pin.name});
            if (node == n.pin_node.end()) {
                continue;
            }
            if (const auto v = net_voltage(lower(node->second))) {
                volts.push_back(*v);
                lines << QStringLiteral("V(%1) = %2").arg(QString::fromStdString(pin.name), quantity(*v, "V"));
            }
        }
        std::optional<double> current;
        if (const auto it = readout_vectors_.find(c->id); it != readout_vectors_.end()) {
            current = session_->latest(it->second);
        } else if (c->type == "resistor" && volts.size() == 2) {
            const auto r = model::parse_engineering(c->parameters.count("R") ? c->parameters.at("R") : "", "Ohm");
            if (r && *r > 0) {
                current = (volts[0] - volts[1]) / *r;
            }
        } else if (const auto vs = session_->latest("v_" + lower(netlist::sanitize_name(c->designator)) + "#branch")) {
            current = -*vs; // SPICE source current flows from + through the source: report the delivered current
        }
        if (volts.size() >= 2) {
            const double across = volts[0] - volts[1];
            lines << QStringLiteral("V = %1").arg(quantity(across, "V"));
            if (current) {
                lines << QStringLiteral("I = %1").arg(quantity(*current, "A"));
                lines << QStringLiteral("P = %1").arg(quantity(std::abs(across * *current), "W"));
            }
        }
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace simnodus::ui
