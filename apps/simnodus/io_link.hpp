// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// External I/O link (SN-218, ADR 0104): exchanges the signals of the
// external I/O parts (bit and word inputs and outputs) with a program such as
// a virtual industrial plant, over TCP with one JSON object per line. While a
// run is going it sends every output's address and value each period ({}
// when there are none, or while paused) and applies the inputs the program
// answers with. Pausing or stopping sends every output's safe value (false,
// 0), because such programs keep the last command. Active only in runs of
// projects that contain these parts; the coupling is in wall-clock time and
// not repeatable.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "model/schematic.hpp"
#include "sim/simulation_session.hpp"
#endif

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <vector>

class QSettings;
class QTcpSocket;

namespace simnodus::library {
class ComponentLibrary;
}

namespace simnodus::ui {

class Document;
class RunController;

class IoLink : public QObject {
    Q_OBJECT
public:
    struct Options {
        bool enabled = true;
        QString host = QStringLiteral("127.0.0.1");
        int port = 8765;
        int period_ms = 20;
    };

    IoLink(RunController& run, Document& document, QSettings& settings, QObject* parent = nullptr);
    ~IoLink() override;

    Options options() const { return options_; }
    void set_options(const Options& options);

    // The project has external I/O parts.
    static bool has_io(const model::Schematic& schematic);
    bool active() const { return active_; }
    bool connected() const;

signals:
    // Something to report in the Messages panel.
    void message(bool error, const QString& text);
    void status_changed();

private:
    enum class Kind { BitIn, BitOut, WordIn, WordOut };
    struct Point {
        model::Id component = model::no_id;
        Kind kind = Kind::BitIn;
        std::string address;
        bool inverted = false;
        double threshold = 1.65;
        double wmin = 0, wmax = 1000, vmin = 0, vmax = 3.3;
        double last_control = -1.0;
    };

    void started();
    void state_changed();
    void tick();
    void read();
    void connect_now();
    void send(const QByteArray& line);
    // The outputs' values now, or their safe values.
    QByteArray outputs(bool safe) const;
    void finish();

    RunController& run_;
    Document& document_;
    QSettings& settings_;
    Options options_;
    std::vector<Point> points_;
    QTcpSocket* socket_ = nullptr;
    QTimer timer_;
    QElapsedTimer since_attempt_;
    QByteArray buffer_;
    bool active_ = false;
    bool safe_owed_ = false;   // circuit values were sent since the last safe values
    bool reported_failure_ = false;
};

} // namespace simnodus::ui
