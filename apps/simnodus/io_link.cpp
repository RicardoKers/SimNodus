// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "io_link.hpp"

#include "document.hpp"
#include "run_controller.hpp"

#include "model/engineering.hpp"

#include <QSettings>
#include <QTcpSocket>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace simnodus::ui {

using nlohmann::json;

namespace {

bool is_io(const std::string& type)
{
    return type == "io_bit_in" || type == "io_bit_out" || type == "io_word_in" || type == "io_word_out";
}

} // namespace

IoLink::IoLink(RunController& run, Document& document, QSettings& settings, QObject* parent)
    : QObject(parent), run_(run), document_(document), settings_(settings)
{
    options_.enabled = settings_.value(QStringLiteral("external_io/enabled"), options_.enabled).toBool();
    options_.host = settings_.value(QStringLiteral("external_io/host"), options_.host).toString();
    options_.port = settings_.value(QStringLiteral("external_io/port"), options_.port).toInt();
    options_.period_ms = settings_.value(QStringLiteral("external_io/period"), options_.period_ms).toInt();
    connect(&run_, &RunController::started, this, &IoLink::started);
    connect(&run_, &RunController::state_changed, this, &IoLink::state_changed);
    connect(&timer_, &QTimer::timeout, this, &IoLink::tick);
}

IoLink::~IoLink()
{
    blockSignals(true); // the window is being destroyed
    finish();
}

void IoLink::set_options(const Options& options)
{
    options_ = options;
    options_.port = std::clamp(options_.port, 1, 65535);
    options_.period_ms = std::clamp(options_.period_ms, 5, 1000);
    settings_.setValue(QStringLiteral("external_io/enabled"), options_.enabled);
    settings_.setValue(QStringLiteral("external_io/host"), options_.host);
    settings_.setValue(QStringLiteral("external_io/port"), options_.port);
    settings_.setValue(QStringLiteral("external_io/period"), options_.period_ms);
    timer_.setInterval(options_.period_ms);
}

bool IoLink::has_io(const model::Schematic& schematic)
{
    const auto& c = schematic.components();
    return std::any_of(c.begin(), c.end(), [](const model::Component& x) { return is_io(x.type); });
}

bool IoLink::connected() const
{
    return socket_ && socket_->state() == QAbstractSocket::ConnectedState;
}

void IoLink::started()
{
    finish();
    const auto& snapshot = run_.snapshot();
    if (!options_.enabled || !has_io(snapshot)) {
        return;
    }
    const auto& library = document_.library();
    points_.clear();
    for (const auto& c : snapshot.components()) {
        if (!is_io(c.type)) {
            continue;
        }
        const auto* d = library.find(c.type);
        auto value = [&](const char* name) {
            const auto it = c.parameters.find(name);
            if (it != c.parameters.end()) {
                return it->second;
            }
            const auto* p = d ? d->parameter(name) : nullptr;
            return p ? p->default_value : std::string();
        };
        auto number = [&](const char* name, double fallback) {
            return model::parse_engineering(value(name)).value_or(fallback);
        };
        Point p;
        p.component = c.id;
        p.kind = c.type == "io_bit_in" ? Kind::BitIn
            : c.type == "io_bit_out"   ? Kind::BitOut
            : c.type == "io_word_in"   ? Kind::WordIn
                                       : Kind::WordOut;
        p.address = value("address");
        p.inverted = value("polarity") == "inverted";
        p.threshold = number("Vth", 1.65);
        p.wmin = number("Wmin", 0.0);
        p.wmax = number("Wmax", 1000.0);
        p.vmin = number("Vmin", 0.0);
        p.vmax = number("Vmax", 3.3);
        if (p.address.empty()) {
            emit message(true, tr("External I/O: %1 has no address and is not exchanged.")
                                   .arg(QString::fromStdString(c.designator)));
            continue;
        }
        points_.push_back(p);
    }
    active_ = true;
    reported_failure_ = false;
    timer_.setInterval(options_.period_ms);
    timer_.start();
    emit message(false, tr("External I/O: exchanging %n signal(s) with %1:%2 every %3 ms. The link runs in real "
                           "time, so results are not repeatable.", nullptr, static_cast<int>(points_.size()))
                            .arg(options_.host)
                            .arg(options_.port)
                            .arg(options_.period_ms));
    connect_now();
    emit status_changed();
}

void IoLink::connect_now()
{
    if (!socket_) {
        socket_ = new QTcpSocket(this);
        connect(socket_, &QTcpSocket::readyRead, this, &IoLink::read);
        connect(socket_, &QTcpSocket::connected, this, [this] {
            reported_failure_ = false;
            emit message(false, tr("External I/O: connected to %1:%2.").arg(options_.host).arg(options_.port));
            emit status_changed();
        });
        connect(socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
            if (active_ && !reported_failure_) {
                reported_failure_ = true;
                emit message(true, tr("External I/O: no connection to %1:%2 (%3); trying again while the run goes on.")
                                       .arg(options_.host)
                                       .arg(options_.port)
                                       .arg(socket_->errorString()));
            }
            emit status_changed();
        });
    }
    since_attempt_.start();
    buffer_.clear();
    socket_->abort();
    socket_->connectToHost(options_.host, static_cast<quint16>(options_.port));
}

void IoLink::state_changed()
{
    if (!active_) {
        return;
    }
    const auto state = run_.state();
    if (state != sim::RunState::Running && safe_owed_) {
        send(outputs(true));
        safe_owed_ = false;
    }
    if (!run_.active()) {
        finish();
    }
}

void IoLink::tick()
{
    if (!active_) {
        return;
    }
    if (!connected()) {
        if (socket_->state() == QAbstractSocket::UnconnectedState && since_attempt_.elapsed() >= 1000) {
            connect_now();
        }
        return;
    }
    if (run_.state() == sim::RunState::Running) {
        send(outputs(false));
        safe_owed_ = true;
    } else {
        if (safe_owed_) {
            send(outputs(true));
            safe_owed_ = false;
        } else {
            send(QByteArrayLiteral("{}")); // the inputs, while paused
        }
    }
}

QByteArray IoLink::outputs(bool safe) const
{
    json out = json::object();
    for (const auto& p : points_) {
        if (p.kind == Kind::BitOut) {
            if (safe) {
                out[p.address] = false;
            } else if (const auto v = run_.pin_voltage(p.component, "1")) {
                out[p.address] = (*v > p.threshold) != p.inverted;
            }
        } else if (p.kind == Kind::WordOut) {
            if (safe) {
                out[p.address] = 0;
            } else if (const auto v = run_.pin_voltage(p.component, "1"); v && p.vmax != p.vmin) {
                const double w = p.wmin + (*v - p.vmin) / (p.vmax - p.vmin) * (p.wmax - p.wmin);
                const double low = std::min(p.wmin, p.wmax);
                const double high = std::max(p.wmin, p.wmax);
                out[p.address] = static_cast<long long>(std::llround(std::clamp(w, low, high)));
            }
        }
    }
    return QByteArray::fromStdString(out.dump(-1, ' ', false, json::error_handler_t::replace));
}

void IoLink::send(const QByteArray& line)
{
    if (connected()) {
        socket_->write(line + '\n');
    }
}

void IoLink::read()
{
    buffer_ += socket_->readAll();
    if (buffer_.size() > 1024 * 1024) {
        buffer_.clear(); // no line end: not this protocol
    }
    for (qsizetype end = buffer_.indexOf('\n'); end >= 0; end = buffer_.indexOf('\n')) {
        const QByteArray line = buffer_.left(end).trimmed();
        buffer_.remove(0, end + 1);
        json inputs;
        try {
            inputs = json::parse(line.toStdString());
        } catch (const json::exception&) {
            continue;
        }
        if (!inputs.is_object() || !run_.active()) {
            continue;
        }
        for (auto& p : points_) {
            if (p.kind != Kind::BitIn && p.kind != Kind::WordIn) {
                continue;
            }
            const auto it = inputs.find(p.address);
            if (it == inputs.end() || !(it->is_boolean() || it->is_number())) {
                continue;
            }
            double control = 0.0;
            if (p.kind == Kind::BitIn) {
                const bool bit = it->is_boolean() ? it->get<bool>() : it->get<double>() != 0.0;
                control = bit != p.inverted ? 1.0 : 0.0;
            } else if (p.wmax != p.wmin) {
                control = std::clamp((it->get<double>() - p.wmin) / (p.wmax - p.wmin), 0.0, 1.0);
            }
            if (control != p.last_control) {
                p.last_control = control;
                run_.set_control(p.component, control);
            }
        }
    }
}

void IoLink::finish()
{
    if (!active_) {
        return;
    }
    active_ = false;
    timer_.stop();
    if (socket_) {
        if (safe_owed_ && connected()) {
            send(outputs(true));
        }
        if (connected()) {
            socket_->waitForBytesWritten(200); // the safe values must leave even when the application exits
        }
        socket_->disconnectFromHost(); // after the bytes already written
    }
    safe_owed_ = false;
    points_.clear();
    emit status_changed();
}

} // namespace simnodus::ui
