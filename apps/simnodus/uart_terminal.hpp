// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Serial terminal of the Signal Analyzer (MCU-05, AN-08): shows what a
// board's UART transmits and sends typed text to its receiver. Received text
// keeps accumulating in the engine while the terminal is hidden.
#pragma once

#include <QString>
#include <QWidget>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace simnodus::ui {

class RunController;

class UartTerminal : public QWidget {
    Q_OBJECT
public:
    explicit UartTerminal(QWidget* parent = nullptr);
    void set_controller(RunController* controller);

    // Rebuilds the port list for a new run and clears the transcripts.
    void reload();
    // Reads new bytes of every port; called with the analyzer refresh.
    void refresh();

    QComboBox* ports() const { return ports_; }
    QPlainTextEdit* output() const { return output_; }
    QLineEdit* input() const { return input_; }
    QCheckBox* timestamps() const { return stamps_; }
    // Sends the input line plus the chosen line ending.
    void send();

private:
    void show_current();

    RunController* controller_ = nullptr;
    QComboBox* ports_ = nullptr;
    QPlainTextEdit* output_ = nullptr;
    QLineEdit* input_ = nullptr;
    QComboBox* ending_ = nullptr;
    QCheckBox* stamps_ = nullptr;
    QPushButton* send_ = nullptr;
    struct Transcript {
        std::uint64_t next = 0; // first byte not yet read
        QString text;
        bool after_cr = false;
        bool line_start = true;
    };
    std::map<int, Transcript> transcripts_; // by port index
};

} // namespace simnodus::ui
