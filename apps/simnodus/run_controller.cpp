// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "run_controller.hpp"

#include "document.hpp"
#include "engine_process.hpp"
#include "schematic_scene.hpp"

#include "editing/operations.hpp"
#include "emu/elf.hpp"
#include "library/component_library.hpp"
#include "library/spice_template.hpp"
#include "mcu/mcu_machine.hpp"
#include "model/engineering.hpp"
#include "netlist/netlist.hpp"
#include "sim/engine_host.hpp"
#include "sim/remote_session.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <tuple>

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

// Below a picovolt or a picoampere a value is the solver's rounding, not a
// reading: "0.5fV" is shown as 0.
double reading(double value)
{
    return std::abs(value) < 1e-12 ? 0.0 : value;
}

QString quantity(double value, const char* unit)
{
    return QString::fromStdString(model::format_engineering(reading(value), 3)) + QString::fromLatin1(unit);
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
    scene_.set_chart_records(&charts_);
    timer_.setInterval(50); // live display at 20 Hz (INT-03)
    connect(&timer_, &QTimer::timeout, this, &RunController::refresh);
}

RunController::~RunController()
{
    session_->stop();
}

void RunController::take_elf_fuses()
{
    // An avr-gcc ELF with the FUSES or LOCKBITS macro (sections .fuse at
    // 0x820000, .lock at 0x830000) carries what a production programmer
    // writes: the chip takes them, and the project shows them (E-10, SN-258).
    const auto& schematic = document_.schematic();
    model::Schematic work = schematic;
    std::vector<std::pair<model::Id, QString>> changed;
    for (const auto& c : schematic.components()) {
        const auto* d = document_.library().find(c.type);
        if (!d || !d->mcu || !library::avr_chip(d->mcu->chip) || !d->parameter("fuse_low")) {
            continue; // a board (the UNO) fixes its fuses
        }
        const auto firmware = c.parameters.find(d->mcu->firmware_parameter);
        if (firmware == c.parameters.end() || firmware->second.empty()) {
            continue;
        }
        std::filesystem::path path(std::u8string(firmware->second.begin(), firmware->second.end()));
        if (path.is_relative()) {
            const QString folder = document_.base_dir();
            path = std::filesystem::path((folder.isEmpty() ? QDir::currentPath() : folder).toStdWString()) / path;
        }
        std::string error;
        const auto image = emu::read_elf(path, error, emu::ElfMachine::Avr);
        if (!image) {
            continue; // a .hex file (no fuses), or an error the run reports
        }
        std::vector<std::pair<std::string, std::uint8_t>> wanted;
        for (const auto& section : image->sections) {
            if (section.address == 0x820000 && section.data.size() >= 2) {
                wanted.emplace_back("fuse_low", section.data[0]);
                wanted.emplace_back("fuse_high", section.data[1]);
                if (section.data.size() >= 3 && d->parameter("fuse_extended")) {
                    wanted.emplace_back("fuse_extended", section.data[2]);
                }
            } else if (section.address == 0x830000 && !section.data.empty()) {
                wanted.emplace_back("lock", section.data[0]);
            }
        }
        bool any = false;
        for (const auto& [name, value] : wanted) {
            const QString text = QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
            const auto it = c.parameters.find(name);
            const auto* p = d->parameter(name);
            const QString now = QString::fromStdString(it != c.parameters.end() ? it->second : p ? p->default_value : std::string());
            bool ok = false;
            if (now.trimmed().remove(QStringLiteral("0x"), Qt::CaseInsensitive).toUInt(&ok, 16) != value || !ok) {
                editing::set_parameter(work, c.id, name, text.toStdString()).apply(work);
                any = true;
            }
        }
        if (any) {
            const auto* now = work.component(c.id);
            changed.emplace_back(c.id, tr("%1: the firmware's ELF sets the fuses (low %2, high %3, lock %4), now the chip's.")
                           .arg(QString::fromStdString(c.designator), QString::fromStdString(now->parameters.at("fuse_low")),
                               QString::fromStdString(now->parameters.count("fuse_high") ? now->parameters.at("fuse_high") : "D9"),
                               QString::fromStdString(now->parameters.count("lock") ? now->parameters.at("lock") : "FF")));
        }
    }
    if (changed.empty()) {
        return;
    }
    document_.apply(model::make_edit(schematic, work, "Fuses from the firmware"));
    for (const auto& [id, text] : changed) {
        emit mcu_message(id, text);
    }
}

bool RunController::start(QString& error, const sim::RunRequest* base)
{
    stop();
    if (!base) {
        take_elf_fuses();
    }
    snapshot_ = base ? base->schematic : document_.schematic();
    if (!base && !network_allowed_ && snapshot_.settings().wifi.gateway) {
        // The gateway was not allowed on this computer: the network stays
        // inside the simulation (DE9c).
        auto settings = snapshot_.settings();
        settings.wifi.gateway = false;
        snapshot_.set_settings(settings);
        emit mcu_message(model::no_id, tr("The Wi-Fi gateway to this computer stays off in this run."));
    }
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
    live_vectors_.clear();
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
        if (d && !d->live_values.empty()) {
            auto& vectors = live_vectors_[c.id];
            for (const auto& value : d->live_values) {
                vectors.push_back(lower(element_name(value.element, c.designator)) + (value.current ? "#branch" : ""));
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
    current_probes_ = request.current_probes;
    build_charts(connectivity);
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

void RunController::build_charts(const netlist::Connectivity& connectivity)
{
    chart_feeds_.clear();
    charts_.clear();
    const auto& library = document_.library();
    // The net of a pin when something else is on it; nullopt for a pin left
    // unconnected, "" for ground.
    auto net_of = [&](model::Id component, const std::string& pin) -> std::optional<std::string> {
        const auto it = connectivity.pin_net.find({component, pin});
        if (it == connectivity.pin_net.end()) {
            return std::nullopt;
        }
        const auto& net = connectivity.nets[static_cast<std::size_t>(it->second)];
        if (net.pins.size() < 2 && net.wires.empty() && net.labels.empty()) {
            return std::nullopt;
        }
        return net.ground ? std::string() : lower(net.name);
    };
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (!d || !d->chart) {
            continue;
        }
        auto& records = charts_[c.id];
        records.resize(d->chart->channels.size());
        for (std::size_t i = 0; i < d->chart->channels.size(); ++i) {
            const auto& channel = d->chart->channels[i];
            const auto plus = net_of(c.id, channel.plus);
            if (!plus || plus->empty()) {
                continue; // off: + unconnected (or on ground, which reads nothing)
            }
            chart_feeds_.push_back({c.id, static_cast<int>(i), *plus, net_of(c.id, channel.minus).value_or("")});
        }
    }
}

bool RunController::chart_channel_on(model::Id chart, int channel) const
{
    return std::any_of(chart_feeds_.begin(), chart_feeds_.end(),
        [&](const ChartFeed& f) { return f.chart == chart && f.channel == channel; });
}

void RunController::pull_charts()
{
    for (const auto& feed : chart_feeds_) {
        auto& record = charts_[feed.chart][static_cast<std::size_t>(feed.channel)];
        const double from = record.end_time();
        // Thinned in the engine to half the record's bucket: the record keeps no more.
        const double bucket = record.bucket_time() / 2;
        const auto plus = session_->samples_since(feed.plus, from, bucket);
        const auto values = feed.minus.empty()
            ? plus
            : sim::difference(plus, session_->samples_since(feed.minus, from, bucket));
        for (const auto& sample : values) {
            if (record.empty() || sample.time > from) {
                record.add(sample.time, sample.value);
            }
        }
    }
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

bool RunController::keep_after_save()
{
    if (!unkept_ || session_->active()) {
        return false;
    }
    unkept_ = false;
    return keep_written_flash();
}

bool RunController::keep_written_flash()
{
    bool edited = false;
    for (const auto& c : snapshot_.components()) {
        const auto* d = document_.library().find(c.type);
        const auto* now = document_.schematic().component(c.id);
        if (!d || !d->mcu || !now) {
            continue;
        }
        // What a run writes and the project keeps: the ESP32-C3's flash
        // (uploads, the firmware's files) and an AVR's flash after an upload
        // through its bootloader (SN-261) become the firmware; an AVR's
        // EEPROM goes into its EEPROM parameter (E-10, SN-258).
        const bool is_avr = library::avr_chip(d->mcu->chip);
        if (d->mcu->chip != "esp32c3" && !is_avr) {
            continue;
        }
        for (const bool avr : {false, true}) {
            if (avr && !is_avr) {
                continue;
            }
            const auto bytes = session_->device_memory(avr ? sim::SimulationSession::eeprom_memory(c.id) : c.id);
            if (!bytes || bytes->empty()) {
                continue;
            }
            const QString designator = QString::fromStdString(c.designator);
            if (document_.file_path().isEmpty()) { // an example copy keeps its folder untouched too
                unkept_ = true;
                emit mcu_message(c.id, avr ? tr("%1: what this run wrote to its EEPROM is kept when you save the project.")
                                                 .arg(designator)
                                           : tr("%1: what this run wrote to its flash (uploaded firmware, the firmware's "
                                                "files) is kept when you save the project.")
                                                 .arg(designator));
                continue;
            }
            // The project's name first: projects sharing a folder (the
            // examples) keep their own files.
            const QString name = QFileInfo(document_.file_path()).completeBaseName() + QLatin1Char('-')
                + file_name_part(designator) + (avr ? QStringLiteral("-eeprom.bin") : QStringLiteral("-flash.bin"));
            QFile file(QDir(document_.base_dir()).absoluteFilePath(name));
            if (!file.open(QIODevice::WriteOnly)
                || file.write(reinterpret_cast<const char*>(bytes->data()), static_cast<qint64>(bytes->size()))
                    != static_cast<qint64>(bytes->size())) {
                emit mcu_message(c.id, avr ? tr("%1: its EEPROM could not be saved as %2: %3").arg(designator, name, file.errorString())
                                           : tr("%1: its flash could not be saved as %2: %3").arg(designator, name, file.errorString()));
                continue;
            }
            file.close();
            const std::string parameter = avr ? std::string("eeprom") : d->mcu->firmware_parameter;
            const auto* holder = document_.schematic().component(c.id); // the flash's edit may have replaced it
            const auto current = holder ? holder->parameters.find(parameter) : decltype(holder->parameters.end()){};
            if (holder && current != holder->parameters.end() && current->second == name.toStdString()) {
                emit mcu_message(c.id, avr ? tr("%1: its EEPROM was saved in %2.").arg(designator, name)
                                           : tr("%1: its flash was saved in %2.").arg(designator, name));
                continue;
            }
            document_.apply(editing::set_parameter(document_.schematic(), c.id, parameter, name.toStdString()));
            edited = true;
            emit mcu_message(c.id, avr ? tr("%1: what this run wrote to its EEPROM was saved as %2, now the chip's EEPROM.")
                                             .arg(designator, name)
                                       : tr("%1: what this run wrote to its flash was saved as %2, now the board's "
                                            "firmware.")
                                             .arg(designator, name));
        }
    }
    return edited;
}

void RunController::stop()
{
    const bool was_active = session_->active();
    if (was_active) {
        pull_charts(); // the samples since the last refresh
    }
    session_->stop();
    if (was_active) {
        keep_written_flash(); // the engine's last update carries the last writes
    }
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
        if (d && d->chart && now && now->parameters != c.parameters) {
            // A chart's size, axes and references only change what it shows (SN-281).
            c.parameters = now->parameters;
            snapshot_.put(c);
            applied = true;
            continue;
        }
        if (!d || (!d->device && !d->mcu) || !now) { // chips, and a board's chip temperature
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
    return apply_circuit_values() || applied;
}

bool RunController::apply_circuit_values()
{
    // Parts simulated by ngspice (SN-232): an interactive run takes new values
    // when the circuit keeps its elements and nodes; the engine continues from
    // the present state in a new segment. Boards and chips are excluded (a new
    // firmware needs a new run; chips have their own live parameters).
    if (mode_ != model::SimulationMode::Interactive) {
        return false;
    }
    const auto& library = document_.library();
    model::Schematic candidate = snapshot_;
    std::vector<std::tuple<model::Id, std::string, std::string>> changes;
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        const auto* now = document_.schematic().component(c.id);
        if (!d || d->device || d->mcu || !now || now->type != c.type || now->parameters == c.parameters) {
            continue;
        }
        model::Component changed = c;
        for (const auto& p : d->parameters) {
            const auto was = c.parameters.find(p.name);
            const auto is = now->parameters.find(p.name);
            const std::string old_value = was == c.parameters.end() ? p.default_value : was->second;
            const std::string new_value = is == now->parameters.end() ? p.default_value : is->second;
            if (old_value != new_value) {
                changes.emplace_back(c.id, p.name, new_value);
                changed.parameters[p.name] = new_value;
            }
        }
        candidate.put(changed);
    }
    if (changes.empty()) {
        return false;
    }
    const auto next = netlist::generate_netlist(candidate, library, {true, 0.0, current_probes_});
    if (!next.ok() || !netlist::same_topology(next, session_->netlist())) {
        return false;
    }
    for (const auto& [id, name, value] : changes) {
        session_->set_device_parameter(id, name, value);
    }
    snapshot_ = std::move(candidate);
    return true;
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

void RunController::set_control(model::Id component, double value, int slot)
{
    session_->set_control(component, value, slot);
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
        std::vector<std::pair<std::pair<std::string, int>, std::string>> pins; // (prefix, number) -> name
        for (const auto& p : d->pins) {
            if (mcu::chip_gpio(d->mcu->chip, p.name)) {
                const auto digits = p.name.find_first_of("0123456789");
                pins.push_back({{p.name.substr(0, digits), std::atoi(p.name.c_str() + digits)}, p.name});
            }
        }
        std::sort(pins.begin(), pins.end()); // PA0, PA1, ..., PB0, ..., PC13; GPIO0, GPIO1, ..., GPIO21
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
    pull_charts();
    for (const auto& [id, text] : session_->device_texts()) {
        live.texts[id] = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    }
    const auto& n = session_->netlist();
    const auto& library = document_.library();
    for (const auto& control : n.controls) {
        if (const auto v = session_->control(control.component, control.slot)) {
            if (control.slot == 0) {
                live.controls[control.component] = *v;
            } else {
                live.more_controls[{control.component, control.slot}] = *v;
            }
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
    const library::McuBoard* first = nullptr;
    bool stm32 = false;
    bool esp32c3 = false;
    for (const auto& c : snapshot_.components()) {
        const auto* d = library.find(c.type);
        if (d && d->mcu) {
            first = first ? first : &*d->mcu;
            stm32 = stm32 || d->mcu->chip == "stm32f103";
            esp32c3 = esp32c3 || d->mcu->chip == "esp32c3";
        }
    }
    if (first) {
        const auto& pm = first->pin_model;
        text += tr("; microcontroller firmware runs in the SimNodus emulator at most 0.1 ms ahead of the circuit; "
                   "GPIO outputs drive the circuit through %L2 Ohm, %1; circuit voltages reach GPIO inputs "
                   "through Schmitt thresholds (%L3 V / %L4 V) less than 0.1 ms late, and the ADC reads them "
                   "0.1-0.2 ms back")
                    .arg(s.exact_edges ? tr("with a time point at every edge (exact edges)")
                                       : tr("averaged over each time step (an edge is spread over one step)"))
                    .arg(pm.output_resistance)
                    .arg(pm.input_low)
                    .arg(pm.input_high);
        if (stm32) {
            text += s.cortex_m3_timing
                ? tr("; firmware instructions take the Cortex-M3 cycles with flash wait states (approximate, within 5 % "
                     "of a real Blue Pill in measured tests)")
                : tr("; firmware instructions take one cycle each (faster than a real chip)");
        }
        if (esp32c3) {
            text += tr("; the ESP32-C3 is experimental: not yet compared with a real board, one instruction per CPU "
                       "clock cycle (approximate)");
        }
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
            if (node == n.pin_node.end() || n.open_drivers.count({c->id, pin.name})) {
                continue; // a pin connected to nothing is not in the circuit: no voltage to show
            }
            if (const auto v = net_voltage(lower(node->second))) {
                volts.push_back(*v);
                lines << QStringLiteral("V(%1) = %2").arg(QString::fromStdString(pin.name), quantity(*v, "V"));
            }
        }
        std::optional<double> current;
        QStringList more;
        if (const auto it = live_vectors_.find(c->id); it != live_vectors_.end()) {
            // The library's live values (SN-279): the line labeled "I" is the part's current.
            for (std::size_t i = 0; i < d->live_values.size() && i < it->second.size(); ++i) {
                const auto& value = d->live_values[i];
                const auto latest = session_->latest(it->second[i]);
                if (!latest) {
                    continue;
                }
                const double shown = *latest * value.scale;
                if (value.label == "I") {
                    current = shown;
                    continue;
                }
                more << QStringLiteral("%1 = %2%3").arg(QCoreApplication::translate("ComponentLibrary", value.label.c_str()),
                                                       QString::fromStdString(model::format_engineering(reading(shown), 3)),
                                                       QString::fromStdString(value.unit));
            }
        } else if (const auto readout = readout_vectors_.find(c->id);
                   readout != readout_vectors_.end() && d->readout->kind == library::Readout::Kind::Current) {
            // A meter's or an LED's measuring source; node readouts (a motor's speed) are no current.
            current = session_->latest(readout->second);
        } else if (c->type == "resistor" && volts.size() == 2) {
            const auto r = model::parse_engineering(c->parameters.count("R") ? c->parameters.at("R") : "", "Ohm");
            if (r && *r > 0) {
                current = (volts[0] - volts[1]) / *r;
            }
        } else if (const auto vs = session_->latest("v_" + lower(netlist::sanitize_name(c->designator)) + "#branch")) {
            current = -*vs; // SPICE source current flows from + through the source: report the delivered current
        }
        // The voltage across: two-pin parts, or the measured pins of a current readout.
        const bool two_terminals = d->pins.size() == 2
            || (d->readout && d->readout->kind == library::Readout::Kind::Current);
        if (volts.size() >= 2 && two_terminals) {
            const double across = volts[0] - volts[1];
            lines << QStringLiteral("V = %1").arg(quantity(across, "V"));
            if (current) {
                lines << QStringLiteral("I = %1").arg(quantity(*current, "A"));
                lines << QStringLiteral("P = %1").arg(quantity(std::abs(across * *current), "W"));
            }
        } else if (current) {
            lines << QStringLiteral("I = %1").arg(quantity(*current, "A"));
        }
        lines << more;
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace simnodus::ui
