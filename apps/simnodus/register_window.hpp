// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Register Viewer (SN-290): the peripheral registers of a board's or chip's
// microcontroller while it runs, as a tree of peripherals, registers and
// named fields. The values come from the engine, looked at without the side
// effects of a read by the firmware (McuBackend::watch_registers), also
// while the run is paused or the firmware is halted by a debugger.
#pragma once

#include "mcu/register_map.hpp"
#include "model/schematic.hpp"

#include <QAbstractItemModel>
#include <QTimer>
#include <QWidget>

#include <cstdint>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

namespace simnodus::ui {

class Document;
class RunController;

// The tree: peripherals, their registers, the fields of each register.
// Columns: name, value, bits, description.
class RegisterModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column { Name, Value, Bits, Description, ColumnCount };

    explicit RegisterModel(QObject* parent = nullptr);

    // The chip's map (nullptr: an empty tree); the values start unknown.
    void set_map(const mcu::RegisterMap* map);
    const mcu::RegisterMap* map() const { return map_; }
    // The registers as the engine has them now; a register that is not among
    // them keeps no value. Values that changed are marked for a moment.
    void set_values(const mcu::RegisterBytes& bytes);
    void clear_values();
    // Takes the marks of changes older than a moment away; true if some were.
    bool expire();
    // A register's value by its names (tests); false while it is unknown.
    bool value(const QString& peripheral, const QString& reg, std::uint32_t& out) const;
    QModelIndex index_of(const QString& peripheral, const QString& reg = {}, const QString& field = {}) const;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    struct State {
        bool known = false;
        std::uint32_t value = 0;
        std::uint32_t changed = 0; // the bits that changed last
        qint64 changed_at = 0;     // ms since the epoch; 0: no mark
    };
    // Where an index points: -1 for the levels below it.
    struct Place {
        int peripheral = -1;
        int reg = -1;
        int field = -1;
    };
    static quintptr pack(const Place& place);
    static Place unpack(quintptr id);
    Place place(const QModelIndex& index) const { return index.isValid() ? unpack(index.internalId()) : Place{}; }
    bool recent(const State& state, std::uint32_t mask) const;

    const mcu::RegisterMap* map_ = nullptr;
    std::vector<std::vector<State>> states_; // per peripheral, per register
};

class RegisterWindow : public QWidget {
    Q_OBJECT

public:
    RegisterWindow(Document& document, RunController& run, QWidget* parent = nullptr);

    // Shows the window with `component` chosen (no_id: keeps the choice).
    void show_board(model::Id component);
    RegisterModel* model() const { return model_; }

protected:
    void hideEvent(QHideEvent* event) override;

private:
    void rebuild_list();
    void refresh();
    model::Id current() const;
    // Tells the engine which registers to show: all of the chosen chip's while
    // the window is on screen, none otherwise.
    void watch(bool on);

    Document& document_;
    RunController& run_;
    QComboBox* boards_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QTreeView* tree_ = nullptr;
    RegisterModel* model_ = nullptr;
    QSortFilterProxyModel* proxy_ = nullptr;
    QLabel* status_ = nullptr;
    QTimer timer_;
    model::Id shown_ = model::no_id;
    model::Id watched_ = model::no_id; // the board whose registers the engine was asked for
    qint64 watch_sent_ = 0;            // when it was asked (ms since the epoch)
};

} // namespace simnodus::ui
