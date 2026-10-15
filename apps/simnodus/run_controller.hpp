// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Connects the GUI to the simulation session (INT-01..INT-05): starts runs
// on a snapshot of the document, forwards interactive controls, refreshes
// live state for the canvas at 20 Hz and describes the probe channels for
// the Signal Analyzer. The GUI never calls the engine directly.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
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

    // Starts a run of the current document. On failure `error` explains it
    // and diagnostics() lists the circuit problems.
    bool start(QString& error);
    void pause_or_resume();
    void stop();
    bool active() const;
    sim::RunState state() const { return session_->status().state; }
    sim::RunStatus status() const { return session_->status(); }
    const std::vector<netlist::Diagnostic>& diagnostics() const { return session_->diagnostics(); }

    void set_speed(double speed);
    void set_control(model::Id component, double value);
    // The reset button of a microcontroller board.
    void mcu_reset(model::Id component);
    std::optional<mcu::McuStatus> mcu_status(model::Id component) const { return session_->mcu_status(component); }

    // Analyzer data.
    const std::vector<Channel>& channels() const { return channels_; }
    std::vector<sim::Sample> samples(const Channel& channel, double from) const;
    sim::TransientResult transient_result() const { return session_->transient_result(); }
    bool transient_mode() const { return mode_ == model::SimulationMode::Transient; }

    // How the current or last run was computed, including its approximations
    // (SIM-09); used in exports.
    QString mode_description() const;

    // Live values of the selected components for the Properties panel.
    QString live_text(const std::set<model::Id>& selection) const;

signals:
    void state_changed();
    void refreshed();
    void failed(const QString& message, const QStringList& engine_output);

private:
    void refresh();
    void build_channels();
    std::optional<double> net_voltage(const std::string& net) const;

    Document& document_;
    SchematicScene& scene_;
    std::unique_ptr<sim::SimulationSession> session_;
    QTimer timer_;
    model::Schematic snapshot_;
    model::SimulationMode mode_ = model::SimulationMode::Interactive;
    std::vector<Channel> channels_;
    std::map<model::Id, std::string> readout_vectors_; // component -> current vector
    sim::RunState last_state_ = sim::RunState::Idle;
};

} // namespace simnodus::ui
