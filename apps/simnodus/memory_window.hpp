// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Memory Viewer (SN-208): the contents of the memory chips of the circuit
// (a 24C02 now; parallel RAM and ROM later) as a hex and ASCII table, live
// while the simulation runs, with the bytes that just changed highlighted.
// Load... puts a .bin or Intel .hex file into the running chip (or makes it the
// chip's contents file when nothing runs); Save as... writes what the chip holds.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "model/schematic.hpp"
#endif

#include <QDateTime>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <vector>

class QComboBox;
class QLabel;
class QTableWidget;

namespace simnodus::ui {

class Document;
class RunController;

class MemoryWindow : public QWidget {
    Q_OBJECT
public:
    MemoryWindow(Document& document, RunController& run, QWidget* parent = nullptr);

    // Shows the window with `component` chosen (no_id: keeps the choice).
    void show_chip(model::Id component);
    // The bytes on display (tests).
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    void rebuild_list();
    void refresh();
    model::Id current() const;
    std::size_t size_of(model::Id component) const;
    // The chip's contents: the running (or last) simulation's, else its contents file, else erased.
    std::vector<std::uint8_t> contents(model::Id component, QString& source) const;
    void load();
    void save();

    Document& document_;
    RunController& run_;
    QComboBox* chips_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer timer_;
    model::Id shown_ = model::no_id;
    std::vector<std::uint8_t> bytes_;
    std::vector<QDateTime> changed_at_; // per byte: when it last changed
};

} // namespace simnodus::ui
