// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "run_controller.hpp"

#include "document.hpp"
#include "schematic_scene.hpp"

#include "library/component_library.hpp"
#include "mcu/renode_engine.hpp"
#include "model/engineering.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cctype>

namespace simnodus::ui {
namespace {

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
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
    : QObject(parent), document_(document), scene_(scene), session_(std::make_unique<sim::SimulationSession>())
{
    timer_.setInterval(50); // live display at 20 Hz (INT-03)
    connect(&timer_, &QTimer::timeout, this, &RunController::refresh);
}

RunController::~RunController()
{
    session_->stop();
}

bool RunController::start(QString& error)
{
    stop();
    snapshot_ = document_.schematic();
    mode_ = snapshot_.settings().mode;
    const auto& library = document_.library();

    sim::RunRequest request;
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
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (d && d->readout && d->readout->kind == library::Readout::Kind::Current) {
            const auto vector = lower(element_name(d->readout->element, c.designator)) + "#branch";
            readout_vectors_[c.id] = vector;
            request.vectors.push_back(vector);
        }
    }
    for (const auto& p : snapshot_.probes()) {
        if (p.kind == model::ProbeKind::Current && snapshot_.component(p.target.item)) {
            request.current_probes.push_back({p.target.item, p.target.pin});
        }
    }
    std::string reason;
    // Microcontroller boards: firmware relative to the project file, Renode next to the application.
    sim::McuEnvironment environment;
    const QString base = document_.base_dir();
    environment.project_dir = std::filesystem::path((base.isEmpty() ? QDir::currentPath() : base).toStdWString());
    const std::filesystem::path application_dir = QCoreApplication::applicationDirPath().toStdWString();
    environment.renode = mcu::find_renode(application_dir);
    environment.resources = mcu::find_mcu_resources(application_dir);
    if (!sim::build_mcu_requests(snapshot_, library, environment, request.mcus, reason)) {
        error = QString::fromStdString(reason);
        emit state_changed();
        return false;
    }
    if (!session_->start(std::move(request), reason)) {
        error = QString::fromStdString(reason);
        emit state_changed();
        return false;
    }
    build_channels();
    last_state_ = sim::RunState::Running;
    timer_.start();
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
    } else if (s == sim::RunState::Paused) {
        session_->resume();
    }
}

void RunController::stop()
{
    session_->stop();
    timer_.stop();
    scene_.clear_live();
    if (last_state_ != sim::RunState::Idle) {
        last_state_ = session_->status().state;
        emit state_changed();
    }
}

bool RunController::active() const
{
    return session_->active();
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

std::vector<sim::Sample> RunController::samples(const Channel& channel, double from) const
{
    if (!channel.valid) {
        return {};
    }
    auto values = session_->samples_since(channel.vector, from);
    if (!channel.reference.empty() && channel.reference != "0") {
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

void RunController::mcu_reset(model::Id component)
{
    session_->mcu_reset(component);
}

void RunController::refresh()
{
    const auto status = session_->status();
    LiveState live;
    live.running = status.state == sim::RunState::Running || status.state == sim::RunState::Paused
        || (status.state == sim::RunState::Finished && mode_ == model::SimulationMode::Transient);
    const auto latest = session_->latest_all();
    live.nets = latest;
    const auto& n = session_->netlist();
    const auto& library = document_.library();
    for (const auto& control : n.controls) {
        if (const auto v = session_->control(control.component)) {
            live.controls[control.component] = *v;
        }
    }
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (!d || !d->readout) {
            continue;
        }
        if (d->readout->kind == library::Readout::Kind::Current) {
            const auto it = latest.find(readout_vectors_[c.id]);
            if (it == latest.end()) {
                continue;
            }
            if (c.type == "led") {
                live.led_current[c.id] = it->second;
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
    QString text = tr("interactive (approximate): %1s segments, maximum step %2s; switch, button and generator edges "
                      "are resolved to the time step; semiconductor charge is not carried across segments")
                       .arg(QString::fromStdString(model::format_engineering(s.segment)), step);
    const auto& library = document_.library();
    const auto& components = snapshot_.components();
    if (std::any_of(components.begin(), components.end(), [&library](const model::Component& c) {
            const auto* d = library.find(c.type);
            return d && d->mcu;
        })) {
        text += tr("; microcontroller firmware runs in Renode within 20 ms of the circuit's time, and its pins are "
                   "not yet connected electrically");
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
