// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Schematic canvas: dot grid, zoom at the cursor, pan with the middle button
// or Space+drag, fit (UI-09), and the editing tools (UI-05..UI-08, UI-11):
// select/move with attached wires, place with a ghost symbol, orthogonal
// wiring, net labels and probes. While a simulation runs, clicks operate
// interactive controls instead of editing (INT-02).
#pragma once

#include "model/geometry.hpp"
#include "model/schematic.hpp"

#include <QGraphicsView>

#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace simnodus::library {
struct ComponentDefinition;
}

namespace simnodus::ui {

class SchematicScene;

enum class Tool { Select, Place, Wire, Label, Probe };

class SchematicView : public QGraphicsView {
    Q_OBJECT
public:
    // One grid step in scene units; symbols and wires snap to it.
    static constexpr double grid = 10.0;
    static constexpr double min_zoom = 0.05;
    static constexpr double max_zoom = 40.0;

    explicit SchematicView(QWidget* parent = nullptr);

    // Connects the view to the document's scene (editing becomes available).
    void set_schematic_scene(SchematicScene* scene);
    SchematicScene* schematic_scene() const { return scene_; }

    double zoom() const;
    void set_zoom(double factor, const QPoint& anchor_in_view);
    bool minor_grid_visible() const;

    Tool tool() const { return tool_; }
    void set_tool(Tool tool);
    void begin_place(const library::ComponentDefinition* definition);
    const library::ComponentDefinition* placing() const { return placing_; }

    // Called before any change of the circuit's structure; returns false to
    // refuse it (for example while a simulation runs and the user declines
    // to stop it).
    void set_edit_guard(std::function<bool()> guard) { guard_ = std::move(guard); }
    void set_running(bool running);
    bool running() const { return running_; }

    // Editing commands (menu and shortcuts).
    void delete_selection();
    void rotate();
    void mirror();
    void copy_selection();
    void cut_selection();
    void paste();
    void select_all();
    void move_selection(model::Point delta);
    void center_on_item(model::Id id);

public slots:
    void zoom_in();
    void zoom_out();
    void fit_content();

signals:
    void cursor_moved(QPointF grid_position);
    void zoom_changed(double factor);
    void tool_changed(Tool tool);
    void status_message(const QString& text);
    void edit_value_requested(model::Id component);
    // Interactive control while running: value 0..1 (INT-02).
    void control_changed(model::Id component, double value);
    void label_name_needed(model::Point at);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    model::Point grid_at(const QPoint& view_point) const;
    bool allowed();
    void update_cursor_shape();
    void update_wire_preview(const model::Point& cursor);
    std::vector<model::Point> route(const model::Point& from, const model::Point& to) const;
    void finish_wire();
    void cancel_wire();
    void place_at(const model::Point& at);
    void add_probe_at(const model::Point& at, bool force_voltage);
    bool handle_control_press(model::Id id, const QPointF& scene_point);
    void show_live_tooltip(const QPoint& view_point);

    SchematicScene* scene_ = nullptr;
    std::function<bool()> guard_;
    bool fitted_once_ = false;
    bool space_held_ = false;
    bool panning_ = false;
    QPoint pan_origin_;
    Tool tool_ = Tool::Select;
    bool running_ = false;

    // Select tool drag state.
    bool dragging_ = false;
    bool rubber_band_ = false;
    model::Point drag_origin_;
    model::Point drag_delta_;
    std::set<model::Id> drag_ids_;
    std::optional<std::pair<model::Id, bool>> text_drag_; // component, value text
    model::Point text_start_;

    // Place tool.
    const library::ComponentDefinition* placing_ = nullptr;
    model::Orientation place_orientation_;
    model::Point ghost_at_;

    // Wire tool.
    std::vector<model::Point> wire_points_;
    bool horizontal_first_ = true;

    // Interactive controls while running.
    model::Id held_button_ = model::no_id;
    model::Id pot_drag_ = model::no_id;
    double pot_start_value_ = 0.0;
    QPoint pot_start_point_;

    int next_probe_color_ = 0;
};

} // namespace simnodus::ui
