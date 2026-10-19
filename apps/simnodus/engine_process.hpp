// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The engine process (SN-151, ADR 0101): starts simnodus-engine next to the
// application on the first run and keeps it for the next ones, carries the
// messages over its standard input and output, and reports when it ends
// unexpectedly. The environment variable SIMNODUS_ENGINE names another
// executable (tests).
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "sim/engine_protocol.hpp"
#include "sim/remote_session.hpp"
#endif

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace simnodus::ui {

class EngineProcess : public QObject, public sim::EngineLink {
    Q_OBJECT
public:
    explicit EngineProcess(QObject* parent = nullptr);
    ~EngineProcess() override;

    bool open(std::string& error) override;
    bool send(const std::string& payload) override;
    bool pump(int timeout_ms) override;
    void kill() override;

    // The engine executable this application uses.
    static QString program();
    // The running engine's process ID, 0 without one.
    qint64 process_id() const { return process_ && process_->state() == QProcess::Running ? process_->processId() : 0; }

private:
    void drain();
    void ended();

    QProcess* process_ = nullptr;
    sim::protocol::FrameReader frames_;
    QStringList errors_; // the engine's last lines on standard error
    bool closing_ = false;
};

} // namespace simnodus::ui
