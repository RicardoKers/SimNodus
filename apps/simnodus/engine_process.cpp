// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "engine_process.hpp"

#include <QCoreApplication>
#include <QDir>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace simnodus::ui {

EngineProcess::EngineProcess(QObject* parent) : QObject(parent) {}

EngineProcess::~EngineProcess()
{
    closing_ = true;
    if (process_ && process_->state() != QProcess::NotRunning) {
        // Quit lets the engine stop its run; closing its input ends it in any case.
        process_->write(QByteArray::fromStdString(sim::protocol::frame(sim::protocol::Writer(sim::protocol::Type::Quit).payload())));
        process_->closeWriteChannel();
        if (!process_->waitForFinished(3000)) {
            process_->kill();
            process_->waitForFinished(2000);
        }
    }
}

QString EngineProcess::program()
{
    const QString chosen = qEnvironmentVariable("SIMNODUS_ENGINE");
    if (!chosen.isEmpty()) {
        return chosen;
    }
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("simnodus-engine.exe");
#else
    const QString name = QStringLiteral("simnodus-engine");
#endif
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

bool EngineProcess::open(std::string& error)
{
    if (process_ && process_->state() == QProcess::Running) {
        return true;
    }
    if (process_) {
        process_->disconnect(this);
        process_->deleteLater();
    }
    frames_ = {};
    errors_.clear();
    process_ = new QProcess(this);
    process_->setProgram(program());
#ifdef Q_OS_WIN
    // A console program started from a window would open a console window.
    process_->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* arguments) { arguments->flags |= CREATE_NO_WINDOW; });
#endif
    connect(process_, &QProcess::readyReadStandardOutput, this, &EngineProcess::drain);
    connect(process_, &QProcess::readyReadStandardError, this, [this] {
        // Kept for a crash report; the pipe must be read so the engine never blocks on it.
        for (const auto& line : process_->readAllStandardError().split('\n')) {
            if (!line.trimmed().isEmpty()) {
                errors_ << QString::fromLocal8Bit(line.trimmed());
            }
        }
        while (errors_.size() > 20) {
            errors_.removeFirst();
        }
    });
    connect(process_, &QProcess::finished, this, &EngineProcess::ended);
    process_->start();
    if (!process_->waitForStarted(10000)) {
        error = tr("The simulation engine could not be started (%1): %2")
                    .arg(QDir::toNativeSeparators(program()), process_->errorString())
                    .toStdString();
        process_->disconnect(this);
        process_->deleteLater();
        process_ = nullptr;
        return false;
    }
    return true;
}

bool EngineProcess::send(const std::string& payload)
{
    if (!process_ || process_->state() != QProcess::Running) {
        return false;
    }
    process_->write(QByteArray::fromStdString(sim::protocol::frame(payload)));
    return true;
}

bool EngineProcess::pump(int timeout_ms)
{
    if (!process_ || process_->state() != QProcess::Running) {
        return false;
    }
    if (process_->bytesAvailable() == 0) {
        process_->waitForReadyRead(timeout_ms);
    }
    drain();
    return process_ && process_->state() == QProcess::Running;
}

void EngineProcess::kill()
{
    if (!process_) {
        return;
    }
    closing_ = true;
    process_->kill();
    process_->waitForFinished(2000);
    closing_ = false;
    frames_ = {};
}

void EngineProcess::drain()
{
    if (!process_) {
        return;
    }
    const QByteArray bytes = process_->readAllStandardOutput();
    frames_.append(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    while (auto payload = frames_.next()) {
        if (on_message) {
            on_message(*payload);
        }
    }
    if (frames_.invalid()) {
        kill();
        if (on_lost) {
            on_lost(tr("The simulation engine sent damaged data and was stopped.").toStdString());
        }
    }
}

void EngineProcess::ended()
{
    if (closing_ || !process_) {
        return;
    }
    drain();
    QString reason = process_->exitStatus() == QProcess::CrashExit || process_->exitCode() != 0
        ? tr("The simulation engine stopped unexpectedly (exit code 0x%1). Your project is not affected; "
             "press F5 to run again.")
              .arg(static_cast<quint32>(process_->exitCode()), 8, 16, QLatin1Char('0'))
        : tr("The simulation engine ended. Press F5 to run again.");
    if (!errors_.isEmpty()) {
        reason += QLatin1Char(' ') + tr("Its last message: %1").arg(errors_.last());
    }
    if (on_lost) {
        on_lost(reason.toStdString());
    }
}

} // namespace simnodus::ui
