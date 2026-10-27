// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Connects the GUI to the simulation session (INT-01..INT-05): starts runs
// on a snapshot of the document, forwards interactive controls, refreshes
// live state for the canvas at 20 Hz and describes the probe channels for
// the Signal Analyzer. The GUI never calls the engine directly.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "tone_output.hpp"
#include "model/schematic.hpp"
#include "netlist/netlist.hpp"
#include "sim/sample_ring.hpp"
#include "sim/simulation_session.hpp"
#endif

#include <QColor>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <set>
#include <vector>

namespace simnodus::ui {

class Document;
class EngineProcess;
class SchematicScene;

struct Channel {
    model::Id probe = model::no_id;
    QString name;    // V(OUT), I(R1.1)
    QString unit;    // "V" or "A"
    QColor color;
    std::string vector;    // ngspice vector
    std::string reference; // voltage reference vector; empty = ground
    bool valid = true;
};

class RunController : public QObject {
    Q_OBJECT
public:
    RunController(Document& document, SchematicScene& scene, QObject* parent = nullptr);
    ~RunController() override;

    // Starts a run of the current document, or of `base` (external control,
    // SN-216: its schematic, vectors and current probes, plus what the canvas
    // shows). On failure `error` explains it and diagnostics() lists the
    // circuit problems.
    bool start(QString& error, const sim::RunRequest* base = nullptr);
    void pause_or_resume();
    void stop();
    bool active() const;
    sim::RunState state() const { return session_->status().state; }
    sim::RunStatus status() const { return session_->status(); }
    sim::Session& session() { return *session_; }
    // The schematic of the current or last run.
    const model::Schematic& snapshot() const { return snapshot_; }
    const std::vector<netlist::Diagnostic>& diagnostics() const { return session_->diagnostics(); }

    // The engine process's ID, 0 without one (in-process runs, before the first run).
    qint64 engine_process_id() const;

    void set_speed(double speed);
    void set_gdb_port(int port) { gdb_port_ = port; } // -1: no debugging
    // Buzzers through the computer's speakers (SN-228, Windows).
    void set_sound(bool on);
    // Memory chips (SN-208): the contents of the current or last run, and new contents for a running chip.
    std::optional<std::vector<std::uint8_t>> device_memory(model::Id component) const
    {
        return session_->device_memory(component);
    }
    void load_device_memory(model::Id component, const std::vector<std::uint8_t>& bytes)
    {
        session_->load_device_memory(component, bytes);
    }
    // While running: sends the document's changed live parameters (an LM75's
    // temperature) to their chips (SN-207), and new values of circuit parts
    // (a resistor) to an interactive run that keeps its topology (SN-232);
    // true if one was sent.
    bool apply_live_parameters();
    void set_control(model::Id component, double value);
    // The latest voltage at a component's pin in the current run.
    std::optional<double> pin_voltage(model::Id component, const std::string& pin) const;
    // The reset button of a microcontroller board.
    void mcu_reset(model::Id component);
    std::optional<mcu::McuStatus> mcu_status(model::Id component) const { return session_->mcu_status(component); }

    // Serial ports of the boards of the current run, for the terminal (MCU-05).
    struct UartPort {
        model::Id board = model::no_id;
        std::string uart;
        QString title; // "U1 USART1 (PA9 TX, PA10 RX)"
    };
    std::vector<UartPort> uart_ports() const;
    std::uint64_t uart_received(const UartPort& port, std::uint64_t from, std::string& bytes,
        std::vector<std::uint64_t>* times = nullptr) const
    {
        return session_->mcu_uart_received(port.board, port.uart, from, bytes, times);
    }
    void uart_send(const UartPort& port, const std::string& bytes) { session_->mcu_uart_send(port.board, port.uart, bytes); }

    // Logic analyzer lanes (AN-07): every GPIO pin of the boards, then a digital
    // view of every voltage channel (above / below a threshold).
    struct LogicLane {
        model::Id board = model::no_id; // pin lanes
        std::string pin;
        int channel = -1;               // probe lanes: index in channels()
        QString title;                  // "U1.PA0", "V(OUT)"
        QColor color;
    };
    struct LaneData {
        bool initial = false;                       // level at the start of the window
        std::vector<std::pair<double, bool>> changes; // seconds, new level
    };
    std::vector<LogicLane> logic_lanes() const;
    LaneData lane_data(const LogicLane& lane, double from, double threshold) const;

    // Analyzer data.
    const std::vector<Channel>& channels() const { return channels_; }
    // With `bucket` (seconds), thinned for display (see SampleRing::since).
    std::vector<sim::Sample> samples(const Channel& channel, double from, double bucket = 0.0) const;
    sim::TransientResult transient_result() const { return session_->transient_result(); }
    bool transient_mode() const { return mode_ == model::SimulationMode::Transient; }

    // How the current or last run was computed, including its approximations
    // (SIM-09); used in exports.
    QString mode_description() const;

    // Live values of the selected components for the Properties panel.
    QString live_text(const std::set<model::Id>& selection) const;

signals:
    void started();
    void state_changed();
    void refreshed();
    void failed(const QString& message, const QStringList& engine_output);
    // A message of a board's microcontroller engine (fault, unsimulated feature).
    void mcu_message(model::Id board, const QString& text);

private:
    void refresh();
    void build_channels();
    std::optional<double> net_voltage(const std::string& net) const;
    // A light's current as the eye sees it: averaged over the last 20 ms (SN-204).
    double glow_current(const std::string& vector, double latest, double now) const;

    Document& document_;
    SchematicScene& scene_;
    std::unique_ptr<EngineProcess> engine_; // before session_, which uses it
    std::unique_ptr<sim::Session> session_;
    QTimer timer_;
    ToneOutput tone_;
    bool sound_ = true;
    bool apply_circuit_values();

    model::Schematic snapshot_;
    std::vector<netlist::PinRef> current_probes_; // of the current run
    model::SimulationMode mode_ = model::SimulationMode::Interactive;
    std::vector<Channel> channels_;
    std::map<model::Id, std::string> readout_vectors_; // component -> current vector
    std::map<model::Id, bool> switch_states_;           // relay contacts: closed after pull-in
    std::map<model::Id, std::vector<std::string>> light_vectors_; // display segments' currents
    std::vector<std::pair<model::Id, std::string>> device_lanes_; // board, device pin (U2.Q0)
    sim::RunState last_state_ = sim::RunState::Idle;
    int gdb_port_ = -1;
};

} // namespace simnodus::ui
