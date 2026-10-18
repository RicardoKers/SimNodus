// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Graphics scene of the schematic (ADR 0081): one item per component, wire,
// net label, probe, text note, bus and splitter, kept in sync with the document by ID. Junction dots
// and open-end markers come from the connectivity (UI-06). While a run is
// active the scene also shows live state (INT-03).
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/component_library.hpp"
#include "mcu/mcu_backend.hpp"
#include "model/schematic.hpp"
#include "netlist/connectivity.hpp"
#endif

#include <QGraphicsScene>
#include <QPointer>

#include <map>
#include <optional>
#include <set>

namespace simnodus::ui {

class Document;
class ComponentItem;
class WireItem;
class LabelItem;
class ProbeItem;
class NoteItem;
class BusItem;
class SplitterItem;
class DecorationItem;

// Grid unit in scene coordinates (same as SchematicView::grid).
constexpr double scene_grid = 10.0;

inline QPointF to_scene(const model::Point& p)
{
    return {p.x * scene_grid, p.y * scene_grid};
}

inline model::Point to_grid(const QPointF& p)
{
    return {static_cast<int>(std::lround(p.x() / scene_grid)), static_cast<int>(std::lround(p.y() / scene_grid))};
}

// Values shown while a simulation runs, refreshed by the run controller.
struct LiveState {
    bool running = false;
    std::map<model::Id, double> controls;   // interactive component -> control value
    // Glowing parts (LED, lamp): brightness 0..1 and color ("red", "#ffd27a").
    struct Glow {
        double level = 0.0;
        std::string color;
    };
    std::map<model::Id, Glow> glow;
    std::map<model::Id, QString> readouts;  // meter/instrument text on the symbol
    std::map<std::string, double> nets;     // net name (lower case) -> volts
    // Microcontroller boards: GPIO pin name -> level and mode (INT-06).
    std::map<model::Id, std::map<std::string, mcu::PinState>> mcu_pins;
};

class SchematicScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit SchematicScene(Document& document, QObject* parent = nullptr);

    Document& document() const { return document_; }
    const library::ComponentLibrary& library() const;
    const netlist::Connectivity& connectivity() const { return connectivity_; }
    const LiveState& live() const { return live_; }

    // Rebuilds items from the document (or from a preview copy while dragging).
    void sync();
    void sync_from(const model::Schematic& schematic);

    std::set<model::Id> selected_ids() const;
    void select_ids(const std::set<model::Id>& ids);
    model::Id id_of(const QGraphicsItem* item) const;
    QGraphicsItem* item_of(model::Id id) const;
    // Topmost element at a scene point (components, labels, probes, notes, wires).
    model::Id element_at(const QPointF& scene_point) const;
    // Pin at a grid point: component ID and pin name.
    std::optional<netlist::PinRef> pin_at(const model::Point& point) const;
    bool wire_at(const model::Point& point, model::Id* wire = nullptr) const;
    // Designator (false) or value (true) text of a component at a scene point.
    std::optional<std::pair<model::Id, bool>> text_at(const QPointF& scene_point) const;
    // Current offset of a component's designator or value text, in grid units.
    model::Point text_offset(model::Id component, bool value) const;
    // Net name at a grid point, if a pin, wire or label is there.
    std::optional<std::string> net_name_at(const model::Point& point) const;

    void set_live(LiveState live);
    void clear_live();
    bool show_voltages() const { return show_voltages_; }
    void set_show_voltages(bool show);

    void set_ghost(const library::ComponentDefinition* definition, const model::Point& position,
        const model::Orientation& orientation);
    void clear_ghost();
    void set_wire_preview(const std::vector<model::Point>& points);

    const model::Schematic& shown() const { return *shown_; }

signals:
    void synced();

private:
    void rebuild(const model::Schematic& schematic);

    Document& document_;
    const model::Schematic* shown_ = nullptr;
    netlist::Connectivity connectivity_;
    std::map<model::Id, ComponentItem*> components_;
    std::map<model::Id, WireItem*> wires_;
    std::map<model::Id, LabelItem*> labels_;
    std::map<model::Id, ProbeItem*> probes_;
    std::map<model::Id, NoteItem*> notes_;
    std::map<model::Id, BusItem*> buses_;
    std::map<model::Id, SplitterItem*> splitters_;
    DecorationItem* decorations_ = nullptr;
    ComponentItem* ghost_ = nullptr;
    WireItem* wire_preview_ = nullptr;
    LiveState live_;
    bool show_voltages_ = false;
    model::Schematic preview_copy_;
};

// Probe colors, also used by the analyzer traces.
QColor probe_color(int index);

} // namespace simnodus::ui
