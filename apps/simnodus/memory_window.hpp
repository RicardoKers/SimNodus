// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Memory Viewer (SN-208): the contents of the memory chips of the circuit
// (a 24C02 now; parallel RAM and ROM later) as a hex and ASCII table, live
// while the simulation runs, with the bytes that just changed highlighted.
// The table is a model over the bytes (SN-285): a 4 MB flash has 262 144 rows,
// of which only the ones on screen are ever formatted.
// Load... puts a .bin or Intel .hex file into the running chip (or makes it the
// chip's contents file when nothing runs); Save as... writes what the chip holds.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/component_library.hpp"
#include "model/schematic.hpp"
#endif

#include <QAbstractTableModel>
#include <QDateTime>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;

namespace simnodus::ui {

class Document;
class RunController;

// The bytes of one memory as rows of 16 with a text column.
class MemoryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    static constexpr int columns = 16;

    using QAbstractTableModel::QAbstractTableModel;

    // New contents: a memory of another size starts over; the same size marks
    // the bytes that differ as just changed. `fresh` starts over in any case
    // (another chip of the same size).
    void set_bytes(std::vector<std::uint8_t> bytes, bool fresh);
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    // Drops the marks older than the highlight time; true while some remain.
    bool expire();
    // True while the byte counts as just changed.
    bool recent(std::size_t index) const;
    // Editing (SN-289): while set, a byte cell takes one or two hex digits and
    // the model says which byte got which value; it does not change its bytes
    // itself (they are the chip's).
    void set_editable(bool editable);
    bool editable() const { return editable_; }
    // What a search field means: pairs of hex digits ("53 69 6D", "5369") are
    // those bytes, anything else is its text; empty when there is nothing to find.
    static std::vector<std::uint8_t> search_pattern(const QString& text);
    // First place of `pattern` at or after `from`, going round to the start; -1 when it is not there.
    qint64 find(const std::vector<std::uint8_t>& pattern, std::size_t from) const;
    QModelIndex index_of(std::size_t byte) const;

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    // Bytes first..last changed at `at` (ms since the epoch): a chip erase is one entry.
    struct Change {
        std::size_t first = 0;
        std::size_t last = 0;
        qint64 at = 0;
    };
    void rows_changed(std::size_t first, std::size_t last);

    std::vector<std::uint8_t> bytes_;
    std::vector<Change> changes_;
    bool editable_ = false;

signals:
    void byte_edited(std::size_t index, std::uint8_t value);
};

class MemoryWindow : public QWidget {
    Q_OBJECT
public:
    MemoryWindow(Document& document, RunController& run, QWidget* parent = nullptr);

    // Shows the window with `component` chosen (no_id: keeps the choice).
    void show_chip(model::Id component);
    // The bytes on display (tests).
    const std::vector<std::uint8_t>& bytes() const { return model_->bytes(); }

private:
    void rebuild_list();
    void refresh();
    model::Id current() const;
    std::size_t size_of(model::Id component) const;
    std::uint8_t fill_of(model::Id component) const; // the value of a byte nobody wrote
    // The parameter holding the memory's file ("contents", an AVR chip's "eeprom").
    std::string contents_parameter(model::Id component) const;
    static bool avr(const library::ComponentDefinition& d)
    {
        return d.mcu && library::avr_chip(d.mcu->chip);
    }
    // The chip's EEPROM: 512 bytes on the ATmega8/8A, 1 KB on the 328/328P and the ATmega32.
    static std::size_t avr_eeprom_bytes(const library::ComponentDefinition& d)
    {
        return d.mcu->chip == "atmega8" || d.mcu->chip == "atmega8a" ? 512 : 1024;
    }
    // The chip's contents: the running (or last) simulation's, else its contents file, else erased.
    std::vector<std::uint8_t> contents(model::Id component, QString& source) const;
    void load();
    void save();
    void go_to();
    void find_next();
    void edit_byte(std::size_t index, std::uint8_t value);

    Document& document_;
    RunController& run_;
    QComboBox* chips_ = nullptr;
    QTableView* table_ = nullptr;
    MemoryModel* model_ = nullptr;
    QLabel* status_ = nullptr;
    QLineEdit* go_to_ = nullptr;
    QLineEdit* find_ = nullptr;
    QLabel* note_ = nullptr; // what Go to, Find or an edit answered
    QTimer timer_;
    model::Id shown_ = model::no_id;
    // The contents file read last, by path, time and size: it is not read
    // again four times a second while nothing runs.
    struct FileCache {
        QString path;
        QDateTime modified;
        qint64 size = -1;
        std::size_t bytes = 0;
        std::uint8_t fill = 0;
        std::vector<std::uint8_t> contents;
        QString error;
    };
    mutable FileCache file_;
    // Bytes edited and not yet seen in the chip's copy: value and when to give up (ms since the epoch).
    std::map<std::size_t, std::pair<std::uint8_t, qint64>> pending_;
};

} // namespace simnodus::ui
