// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// External control of the window (SN-216, ADR 0104): the automation commands
// of simnodus-cli on a local endpoint (a named pipe on Windows, a local
// socket elsewhere) that only this user can open, one JSON request per line.
// It is off until the user turns it on (Tools > Allow External Control) or
// starts SimNodus with --control. Edits go through the document's undo stack
// and are selected on the canvas, optionally paced for recordings; runs go
// through the run controller, so the canvas and the Signal Analyzer show them.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "automation/automation.hpp"
#endif

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include <deque>
#include <memory>
#include <vector>

class QLabel;
class QLocalServer;
class QLocalSocket;

namespace simnodus::ui {

class MainWindow;
class ControlWorkspace;

class ControlServer : public QObject {
    Q_OBJECT
public:
    explicit ControlServer(MainWindow& window);
    ~ControlServer() override;

    // simnodus-control-<user>, or the environment variable SIMNODUS_CONTROL_NAME.
    static QString default_name();

    // Starts accepting connections; false with `error` if another window has the endpoint.
    bool listen(QString& error, const QString& name = default_name());
    void close();
    bool listening() const;
    // The pipe or socket path simnodus-cli connects to.
    QString endpoint() const;
    int clients() const;

    // A command is running (it may be waiting for a run).
    bool busy() const { return busy_; }
    // Makes a waiting command end with error "cancelled" (the window is closing).
    void cancel();

    // Runs one request outside any connection (tests); its own run and folder.
    automation::json execute(const automation::json& request);

    // Milliseconds the window waits after each edit, so that recordings can follow (view.pace).
    int pace() const { return pace_; }

signals:
    // Listening or connections changed.
    void status_changed();

private:
    struct Client {
        QPointer<QLocalSocket> socket;
        std::unique_ptr<automation::Automation> automation;
        QByteArray buffer;
        bool gone = false;
    };
    void accept();
    void read(Client* client);
    void process();
    void drop_gone();
    std::unique_ptr<automation::Automation> make_automation();
    void forget_runs();
    void show_caption(const QString& text, int ms);
    // analyzer.set: changes the Signal Analyzer's settings; every setting as JSON.
    automation::json analyzer_set(const automation::json& args);
    automation::json analyzer_state() const;
    void place_caption();
    bool eventFilter(QObject* watched, QEvent* event) override;

    friend class ControlWorkspace;
    MainWindow& window_;
    std::unique_ptr<ControlWorkspace> workspace_;
    QLocalServer* server_ = nullptr;
    std::vector<std::unique_ptr<Client>> clients_;
    std::deque<std::pair<Client*, QByteArray>> queue_;
    std::unique_ptr<automation::Automation> local_; // execute()
    QLabel* caption_ = nullptr;
    int caption_serial_ = 0;
    bool busy_ = false;
    bool starting_ = false; // a command is starting a run
    int pace_ = 0;
};

} // namespace simnodus::ui
