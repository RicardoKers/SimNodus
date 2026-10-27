// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "schematic_view.hpp"

#include "document.hpp"
#include "schematic_scene.hpp"
#include "theme.hpp"

#include "editing/operations.hpp"
#include "model/engineering.hpp"

#include <QApplication>
#include <QClipboard>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QToolTip>
#include <QTransform>

#include <utility>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <vector>

namespace simnodus::ui {
namespace {

// Area shown by Fit when the schematic is empty: 120 x 80 grid steps.
const QRectF empty_sheet(0.0, 0.0, 120 * SchematicView::grid, 80 * SchematicView::grid);
// Dots closer than this many pixels are not drawn.
constexpr double min_dot_spacing_px = 7.0;
constexpr int major_every = 10;
const QString clipboard_mime = QStringLiteral("application/x-simnodus-elements");
const QString component_mime = QStringLiteral("application/x-simnodus-component");

} // namespace

SchematicView::SchematicView(QWidget* parent) : QGraphicsView(parent)
{
    auto* scene = new QGraphicsScene(this);
    scene->setSceneRect(-500 * grid, -500 * grid, 1000 * grid, 1000 * grid);
    setScene(scene);
    setRenderHint(QPainter::Antialiasing, true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setCacheMode(QGraphicsView::CacheNone);
    setDragMode(QGraphicsView::NoDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setFrameShape(QFrame::NoFrame);
    // Clean canvas: pan with the middle button or Space+drag instead.
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setObjectName(QStringLiteral("schematicView"));
}

void SchematicView::set_schematic_scene(SchematicScene* scene)
{
    QGraphicsScene* old = this->scene();
    scene_ = scene;
    setScene(scene);
    if (old && old->parent() == this) {
        old->deleteLater();
    }
}

double SchematicView::zoom() const
{
    return transform().m11();
}

void SchematicView::set_zoom(double factor, const QPoint& anchor_in_view)
{
    const double target = std::clamp(factor, min_zoom, max_zoom);
    const double current = zoom();
    if (std::abs(target - current) < 1e-12) {
        return;
    }
    // Keep the scene point under the anchor fixed.
    const QPointF before = mapToScene(anchor_in_view);
    const auto anchor = transformationAnchor();
    setTransformationAnchor(QGraphicsView::NoAnchor);
    scale(target / current, target / current);
    const QPointF after = mapToScene(anchor_in_view);
    translate(after.x() - before.x(), after.y() - before.y());
    setTransformationAnchor(anchor);
    emit zoom_changed(zoom());
}

bool SchematicView::minor_grid_visible() const
{
    return grid * zoom() >= min_dot_spacing_px;
}

void SchematicView::zoom_in()
{
    set_zoom(zoom() * 1.25, viewport()->rect().center());
}

void SchematicView::zoom_out()
{
    set_zoom(zoom() / 1.25, viewport()->rect().center());
}

QRectF SchematicView::content_rect() const
{
    QRectF target;
    if (scene_) {
        for (auto* item : scene_->items()) {
            if (scene_->id_of(item) != model::no_id) {
                target = target.isNull() ? item->sceneBoundingRect() : target.united(item->sceneBoundingRect());
            }
        }
    }
    return target.isEmpty() ? empty_sheet : target;
}

void SchematicView::fit_content()
{
    const QRectF target = content_rect();
    fitInView(target.adjusted(-4 * grid, -4 * grid, 4 * grid, 4 * grid), Qt::KeepAspectRatio);
    if (zoom() > 3.0) {
        set_zoom(3.0, viewport()->rect().center()); // a single part should not fill the screen
    }
    if (zoom() > max_zoom || zoom() < min_zoom) {
        set_zoom(zoom(), viewport()->rect().center());
    }
    emit zoom_changed(zoom());
}

void SchematicView::fit_area(const QRectF& target, const QRectF& region)
{
    const QRectF view(viewport()->rect());
    const QRectF frame(view.left() + region.left() * view.width(), view.top() + region.top() * view.height(),
        region.width() * view.width(), region.height() * view.height());
    const QRectF padded = target.adjusted(-4 * grid, -4 * grid, 4 * grid, 4 * grid);
    double factor = std::min(frame.width() / padded.width(), frame.height() / padded.height());
    factor = std::clamp(std::min(factor, 3.0), min_zoom, max_zoom);
    setTransform(QTransform::fromScale(factor, factor));
    // The target's center goes to the frame's center, not the view's.
    centerOn(padded.center() + (view.center() - frame.center()) / factor);
    emit zoom_changed(zoom());
}

void SchematicView::center_on_item(model::Id id)
{
    if (!scene_) {
        return;
    }
    if (auto* item = scene_->item_of(id)) {
        scene_->select_ids({id});
        centerOn(item);
    }
}

void SchematicView::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, theme::canvas_background);
    const double pixels_per_step = grid * zoom();
    const bool minor = pixels_per_step >= min_dot_spacing_px;
    const bool major = pixels_per_step * major_every >= min_dot_spacing_px;
    if (!major) {
        return;
    }
    const double step = minor ? grid : grid * major_every;
    const long long x0 = static_cast<long long>(std::floor(rect.left() / step));
    const long long x1 = static_cast<long long>(std::ceil(rect.right() / step));
    const long long y0 = static_cast<long long>(std::floor(rect.top() / step));
    const long long y1 = static_cast<long long>(std::ceil(rect.bottom() / step));
    std::vector<QPointF> minor_points;
    std::vector<QPointF> major_points;
    const int ratio = minor ? major_every : 1;
    for (long long ix = x0; ix <= x1; ++ix) {
        for (long long iy = y0; iy <= y1; ++iy) {
            const QPointF point(static_cast<double>(ix) * step, static_cast<double>(iy) * step);
            if (ix % ratio == 0 && iy % ratio == 0) {
                major_points.push_back(point);
            } else {
                minor_points.push_back(point);
            }
        }
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen(theme::grid_minor);
    pen.setCosmetic(true);
    pen.setWidthF(1.0);
    painter->setPen(pen);
    painter->drawPoints(minor_points.data(), static_cast<int>(minor_points.size()));
    pen.setColor(theme::grid_major);
    pen.setWidthF(2.0);
    painter->setPen(pen);
    painter->drawPoints(major_points.data(), static_cast<int>(major_points.size()));
    painter->restore();
}

model::Point SchematicView::grid_at(const QPoint& view_point) const
{
    return to_grid(mapToScene(view_point));
}

bool SchematicView::allowed()
{
    return !guard_ || guard_();
}

void SchematicView::set_running(bool running)
{
    running_ = running;
    if (running && tool_ != Tool::Select) {
        set_tool(Tool::Select);
    }
    held_button_ = model::no_id;
    pot_drag_ = model::no_id;
}

void SchematicView::set_tool(Tool tool)
{
    if (tool != Tool::Select && running_ && !allowed()) {
        return;
    }
    cancel_wire();
    if (scene_) {
        scene_->clear_ghost();
    }
    if (tool != Tool::Place) {
        placing_ = nullptr;
    }
    tool_ = tool;
    update_cursor_shape();
    emit tool_changed(tool_);
    switch (tool_) {
    case Tool::Wire:
        emit status_message(tr("Wire: click to start, click to add bends, click a pin or wire to finish. Space "
                               "changes the corner, Esc cancels."));
        break;
    case Tool::Label:
        emit status_message(tr("Net label: click a wire or pin. Labels with the same name are connected."));
        break;
    case Tool::Note:
        emit status_message(tr("Text note: click where the text starts. Double-click a note to edit it."));
        break;
    case Tool::Bus:
        emit status_message(tr("Bus: click to start, click to add bends, double-click or right-click to finish; "
                               "then give it a name. Splitters join its bits to wires."));
        break;
    case Tool::Splitter:
        emit status_message(tr("Splitter: click a point of a bus. Its bit pins act as labels named after the bus "
                               "(D0, D1, ...); Ctrl+R rotates it."));
        break;
    case Tool::Probe:
        emit status_message(tr("Probe: click a wire for a voltage, a component pin for its current (Shift: "
                               "voltage at the pin, Alt: reference of the last voltage probe)."));
        break;
    case Tool::Place:
    case Tool::Select:
        break;
    }
}

void SchematicView::begin_place(const library::ComponentDefinition* definition)
{
    if (!definition || !scene_ || (running_ && !allowed())) {
        return;
    }
    set_tool(Tool::Place);
    placing_ = definition;
    place_orientation_ = {};
    ghost_at_ = grid_at(mapFromGlobal(QCursor::pos()));
    scene_->set_ghost(placing_, ghost_at_, place_orientation_);
    emit status_message(tr("Placing %1: click to place, Ctrl+R rotates, Ctrl+E mirrors, Esc ends.")
                            .arg(QString::fromStdString(definition->name)));
}

void SchematicView::place_at(const model::Point& at)
{
    if (!scene_ || !placing_ || !allowed()) {
        return;
    }
    auto& document = scene_->document();
    model::Id id = model::no_id;
    document.apply(editing::add_component(document.id_source(), *placing_, at, place_orientation_, &id));
    if (id) {
        emit status_message(tr("Placed %1.").arg(QString::fromStdString(document.schematic().component(id)->designator)));
    }
}

void SchematicView::delete_selection()
{
    if (!scene_ || scene_->selected_ids().empty() || !allowed()) {
        return;
    }
    scene_->document().apply(editing::delete_items(scene_->document().schematic(), scene_->selected_ids()));
}

void SchematicView::rotate()
{
    if (tool_ == Tool::Place && placing_) {
        place_orientation_.rotation = model::normalized_rotation(place_orientation_.rotation + 90);
        scene_->set_ghost(placing_, ghost_at_, place_orientation_);
        return;
    }
    if (!scene_ || scene_->selected_ids().empty() || !allowed()) {
        return;
    }
    auto& d = scene_->document();
    d.apply(editing::rotate_items(d.schematic(), d.library(), scene_->selected_ids()));
}

void SchematicView::mirror()
{
    if (tool_ == Tool::Place && placing_) {
        place_orientation_ = {model::normalized_rotation(360 - place_orientation_.rotation), !place_orientation_.mirrored};
        scene_->set_ghost(placing_, ghost_at_, place_orientation_);
        return;
    }
    if (!scene_ || scene_->selected_ids().empty() || !allowed()) {
        return;
    }
    auto& d = scene_->document();
    d.apply(editing::mirror_items(d.schematic(), d.library(), scene_->selected_ids()));
}

void SchematicView::copy_selection()
{
    if (!scene_ || scene_->selected_ids().empty()) {
        return;
    }
    const std::string text = editing::copy_items(scene_->document().schematic(), scene_->selected_ids());
    auto* mime = new QMimeData;
    mime->setData(clipboard_mime, QByteArray::fromStdString(text));
    QApplication::clipboard()->setMimeData(mime);
}

void SchematicView::cut_selection()
{
    if (!scene_ || scene_->selected_ids().empty() || !allowed()) {
        return;
    }
    copy_selection();
    delete_selection();
}

void SchematicView::paste()
{
    if (!scene_ || !allowed()) {
        return;
    }
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(clipboard_mime)) {
        return;
    }
    auto& d = scene_->document();
    std::set<model::Id> created;
    const model::Point offset = grid_at(mapFromGlobal(QCursor::pos()));
    // Paste near the cursor: shift so that the first pasted component lands there.
    model::Point shift{4, 4};
    std::string error;
    const auto text = mime->data(clipboard_mime).toStdString();
    if (viewport()->rect().contains(mapFromGlobal(QCursor::pos()))) {
        // Find the clipboard's top-left point to align it with the cursor.
        model::Schematic probe;
        std::set<model::Id> ids;
        auto edit = editing::paste_items(probe, d.library(), text, {}, &ids);
        edit.apply(probe);
        int min_x = INT32_MAX, min_y = INT32_MAX;
        for (const auto& c : probe.components()) {
            min_x = std::min(min_x, c.position.x);
            min_y = std::min(min_y, c.position.y);
        }
        for (const auto& w : probe.wires()) {
            for (const auto& p : w.points) {
                min_x = std::min(min_x, p.x);
                min_y = std::min(min_y, p.y);
            }
        }
        if (min_x != INT32_MAX) {
            shift = offset - model::Point{min_x, min_y};
        }
    }
    if (d.apply(editing::paste_items(d.id_source(), d.library(), text, shift, &created))) {
        scene_->select_ids(created);
    }
}

void SchematicView::select_all()
{
    if (!scene_) {
        return;
    }
    std::set<model::Id> ids;
    const auto& s = scene_->document().schematic();
    for (const auto& c : s.components()) {
        ids.insert(c.id);
    }
    for (const auto& w : s.wires()) {
        ids.insert(w.id);
    }
    for (const auto& l : s.labels()) {
        ids.insert(l.id);
    }
    for (const auto& p : s.probes()) {
        ids.insert(p.id);
    }
    for (const auto& n : s.notes()) {
        ids.insert(n.id);
    }
    scene_->select_ids(ids);
}

void SchematicView::move_selection(model::Point delta)
{
    if (!scene_ || scene_->selected_ids().empty() || !allowed()) {
        return;
    }
    auto& d = scene_->document();
    d.apply(editing::move_items(d.schematic(), d.library(), scene_->selected_ids(), delta));
}

std::vector<model::Point> SchematicView::route(const model::Point& from, const model::Point& to) const
{
    if (from.x == to.x || from.y == to.y) {
        return {from, to};
    }
    const model::Point corner = horizontal_first_ ? model::Point{to.x, from.y} : model::Point{from.x, to.y};
    return {from, corner, to};
}

void SchematicView::update_wire_preview(const model::Point& cursor)
{
    if (!scene_ || wire_points_.empty()) {
        return;
    }
    auto points = wire_points_;
    const auto tail = route(points.back(), cursor);
    points.insert(points.end(), tail.begin() + 1, tail.end());
    scene_->set_wire_preview(editing::normalize_polyline(points));
}

void SchematicView::finish_wire()
{
    if (scene_ && wire_points_.size() >= 2 && allowed()) {
        if (tool_ == Tool::Bus) {
            const auto points = wire_points_;
            cancel_wire();
            emit bus_name_needed(points);
            return;
        }
        auto& d = scene_->document();
        d.apply(editing::add_wire(d.id_source(), wire_points_));
    }
    cancel_wire();
}

void SchematicView::cancel_wire()
{
    wire_points_.clear();
    if (scene_) {
        scene_->set_wire_preview({});
    }
}

void SchematicView::add_probe_at(const model::Point& at, bool force_voltage)
{
    if (!scene_ || !allowed()) {
        return;
    }
    model::Probe probe;
    probe.color = probe_color(next_probe_color_).name().toStdString();
    model::Id wire = model::no_id;
    if (const auto pin = scene_->pin_at(at)) {
        probe.kind = force_voltage ? model::ProbeKind::Voltage : model::ProbeKind::Current;
        probe.target = {pin->first, pin->second, at};
    } else if (scene_->wire_at(at, &wire)) {
        probe.kind = model::ProbeKind::Voltage;
        probe.target = {wire, {}, at};
    } else {
        emit status_message(tr("Click on a wire or a component pin."));
        return;
    }
    auto& d = scene_->document();
    if (d.apply(editing::add_probe(d.id_source(), probe))) {
        ++next_probe_color_;
        emit status_message(probe.kind == model::ProbeKind::Voltage ? tr("Voltage probe added.")
                                                                    : tr("Current probe added."));
    }
}

bool SchematicView::move_probe_to(model::Id probe, const model::Point& at)
{
    auto& d = scene_->document();
    const auto* current = d.schematic().probe(probe);
    if (!current) {
        return false;
    }
    model::Probe updated = *current;
    model::Id wire = model::no_id;
    if (const auto pin = scene_->pin_at(at)) {
        updated.target = {pin->first, pin->second, at}; // current probes stay current, voltage probes measure the pin
    } else if (updated.kind == model::ProbeKind::Voltage && scene_->wire_at(at, &wire)) {
        updated.target = {wire, {}, at};
    } else {
        emit status_message(updated.kind == model::ProbeKind::Current
                ? tr("A current probe goes on a component pin.")
                : tr("Drop the probe on a wire or a component pin."));
        return false;
    }
    if (updated.target == current->target) {
        return false;
    }
    model::Schematic work = d.schematic();
    work.put(updated);
    if (!d.apply(model::make_edit(d.schematic(), work, "Move probe"))) {
        return false;
    }
    emit status_message(tr("Probe moved."));
    return true;
}

void SchematicView::set_probe_reference(const model::Point& at)
{
    if (!scene_ || !allowed()) {
        return;
    }
    auto& d = scene_->document();
    const model::Probe* last = nullptr;
    for (const auto& p : d.schematic().probes()) {
        if (p.kind == model::ProbeKind::Voltage) {
            last = &p; // probes are ordered by ID: the newest is last
        }
    }
    if (!last) {
        emit status_message(tr("Add a voltage probe first, then Alt+click the reference net."));
        return;
    }
    model::Anchor reference;
    model::Id wire = model::no_id;
    if (const auto pin = scene_->pin_at(at)) {
        reference = {pin->first, pin->second, at};
    } else if (scene_->wire_at(at, &wire)) {
        reference = {wire, {}, at};
    } else {
        emit status_message(tr("Alt+click on a wire or a pin to set the reference."));
        return;
    }
    model::Schematic work = d.schematic();
    model::Probe updated = *last;
    updated.reference = reference;
    work.put(updated);
    if (d.apply(model::make_edit(d.schematic(), work, "Set probe reference"))) {
        emit status_message(tr("The last voltage probe now measures relative to this net."));
    }
}

bool SchematicView::handle_control_press(model::Id id, const QPointF& scene_point)
{
    if (!scene_) {
        return false;
    }
    const auto* c = scene_->document().schematic().component(id);
    const auto* d = c ? scene_->library().find(c->type) : nullptr;
    if (d && d->mcu && d->mcu->reset_button) {
        // Symbol coordinates of the click: undo the item position, rotation and mirror.
        QPointF local = scene_->item_of(id)->mapFromScene(scene_point) / scene_grid;
        QTransform orientation;
        orientation.rotate(c->orientation.rotation);
        orientation.scale(c->orientation.mirrored ? -1 : 1, 1);
        local = orientation.inverted().map(local);
        const auto& r = *d->mcu->reset_button;
        if (QRectF(r[0].x, r[0].y, r[1].x, r[1].y).contains(local)) {
            emit mcu_reset_requested(id);
            return true;
        }
        return false;
    }
    if (!d || !d->interactive) {
        return false;
    }
    if (d->interactive->kind == library::Interactive::Kind::Keypad) {
        // The key under the mouse, held until the button is released.
        QPointF local = scene_->item_of(id)->mapFromScene(scene_point) / scene_grid;
        QTransform orientation;
        orientation.rotate(c->orientation.rotation);
        orientation.scale(c->orientation.mirrored ? -1 : 1, 1);
        local = orientation.inverted().map(local);
        const auto& keys = d->interactive->keys;
        for (std::size_t k = 0; k < keys.size(); ++k) {
            if (QRectF(keys[k][0], keys[k][1], keys[k][2], keys[k][3]).contains(local)) {
                held_button_ = id;
                emit control_changed(id, static_cast<double>(k + 1));
                return true;
            }
        }
        return false;
    }
    const auto& live = scene_->live();
    const auto current = live.controls.find(id);
    const double value = current == live.controls.end() ? 0.0 : current->second;
    switch (d->interactive->kind) {
    case library::Interactive::Kind::Toggle:
        emit control_changed(id, value >= 0.5 ? 0.0 : 1.0);
        break;
    case library::Interactive::Kind::Button:
        held_button_ = id;
        emit control_changed(id, 1.0);
        break;
    case library::Interactive::Kind::Potentiometer: {
        pot_drag_ = id;
        pot_start_value_ = value;
        pot_start_point_ = mapFromScene(scene_point);
        // Dragging along the wiper's track on screen (0 % towards 100 %) turns it
        // that way, whatever the symbol's rotation; for a part without a track
        // (adjustable sources), dragging up raises the value, like the wheel.
        pot_direction_ = QPointF(0, -1);
        if (const auto& w = d->interactive->wiper) {
            QTransform orientation;
            orientation.rotate(c->orientation.rotation);
            orientation.scale(c->orientation.mirrored ? -1 : 1, 1);
            const QPointF track = orientation.map(QPointF(w->end.x - w->start.x, w->end.y - w->start.y));
            const double length = std::hypot(track.x(), track.y());
            if (length > 0) {
                pot_direction_ = track / length;
            }
        }
        break;
    }
    case library::Interactive::Kind::Keypad:
        break; // handled above
    }
    return true;
}

void SchematicView::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    // While running, the wheel over a potentiometer turns it.
    if (running_ && scene_) {
        const model::Id id = scene_->element_at(mapToScene(event->position().toPoint()));
        const auto* c = scene_->document().schematic().component(id);
        const auto* d = c ? scene_->library().find(c->type) : nullptr;
        if (d && d->interactive && d->interactive->kind == library::Interactive::Kind::Potentiometer) {
            const auto& live = scene_->live();
            const auto it = live.controls.find(id);
            const double value = it == live.controls.end() ? 0.5 : it->second;
            emit control_changed(id, std::clamp(value + steps * 0.05, 0.0, 1.0));
            event->accept();
            return;
        }
    }
    set_zoom(zoom() * std::pow(1.2, steps), event->position().toPoint());
    event->accept();
}

void SchematicView::mousePressEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && space_held_ && !drawing_line())) {
        panning_ = true;
        pan_origin_ = position;
        update_cursor_shape();
        event->accept();
        return;
    }
    if (!scene_) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    const model::Point at = grid_at(position);
    if (event->button() == Qt::RightButton) {
        if (drawing_line() && !wire_points_.empty()) {
            finish_wire();
        } else if (tool_ != Tool::Select) {
            set_tool(Tool::Select);
        }
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    switch (tool_) {
    case Tool::Place:
        place_at(ghost_at_);
        event->accept();
        return;
    case Tool::Splitter:
        emit splitter_needed(at);
        event->accept();
        return;
    case Tool::Bus:
    case Tool::Wire: {
        if (wire_points_.empty()) {
            wire_points_.push_back(at);
        } else {
            const auto tail = route(wire_points_.back(), at);
            wire_points_.insert(wire_points_.end(), tail.begin() + 1, tail.end());
            wire_points_ = editing::normalize_polyline(wire_points_);
            // Ending on a pin, a wire or a label connects and finishes the wire; a
            // bus finishes on double-click or right-click.
            const bool connection = tool_ == Tool::Wire
                && (scene_->pin_at(at) || scene_->wire_at(at) || scene_->net_name_at(at));
            if (connection && wire_points_.size() >= 2) {
                finish_wire();
            }
        }
        update_wire_preview(at);
        event->accept();
        return;
    }
    case Tool::Label:
        emit label_name_needed(at);
        event->accept();
        return;
    case Tool::Note:
        emit note_text_needed(at, model::no_id);
        event->accept();
        return;
    case Tool::Probe:
        if (event->modifiers().testFlag(Qt::AltModifier)) {
            set_probe_reference(at);
        } else {
            add_probe_at(at, event->modifiers().testFlag(Qt::ShiftModifier));
        }
        event->accept();
        return;
    case Tool::Select:
        break;
    }

    const QPointF scene_point = mapToScene(position);
    model::Id id = scene_->element_at(scene_point);
    if (!id) {
        // Designator and value texts select their component (and can be dragged).
        if (const auto text = scene_->text_at(scene_point)) {
            id = text->first;
        }
    }
    if (running_ && id && handle_control_press(id, scene_point)) {
        event->accept();
        return;
    }
    if (id) {
        const bool toggle = event->modifiers().testFlag(Qt::ControlModifier);
        QGraphicsItem* item = scene_->item_of(id);
        if (toggle) {
            item->setSelected(!item->isSelected());
            event->accept();
            return;
        }
        if (!item->isSelected()) {
            scene_->select_ids({id});
        }
        if (!running_ && scene_->document().schematic().probe(id)) {
            // Probes are dragged to another wire or pin, not moved with the selection.
            scene_->select_ids({id});
            probe_drag_ = id;
            drag_origin_ = at;
            drag_delta_ = {};
            event->accept();
            return;
        }
        if (!running_) {
            dragging_ = true;
            drag_origin_ = at;
            drag_delta_ = {};
            drag_ids_ = scene_->selected_ids();
            if (const auto text = scene_->text_at(scene_point); text && drag_ids_.size() == 1) {
                text_drag_ = text;
                text_start_ = scene_->text_offset(text->first, text->second);
            }
        }
        event->accept();
        return;
    }
    // Empty area: rubber-band selection.
    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        scene_->clearSelection();
    }
    rubber_band_ = true;
    setDragMode(QGraphicsView::RubberBandDrag);
    QGraphicsView::mousePressEvent(event);
}

void SchematicView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    if (panning_) {
        const QPoint delta = position - pan_origin_;
        pan_origin_ = position;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    const QPointF scene_position = mapToScene(position);
    emit cursor_moved(QPointF(scene_position.x() / grid, scene_position.y() / grid));
    if (!scene_) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    const model::Point at = grid_at(position);
    if (pot_drag_) {
        // Moving along the track (pot_direction_) turns the wiper the same way; 200 px is the full range.
        const QPointF moved = position - pot_start_point_;
        const double change = (moved.x() * pot_direction_.x() + moved.y() * pot_direction_.y()) / 200.0;
        emit control_changed(pot_drag_, std::clamp(pot_start_value_ + change, 0.0, 1.0));
        event->accept();
        return;
    }
    if (tool_ == Tool::Place && placing_) {
        if (!(at == ghost_at_)) {
            ghost_at_ = at;
            scene_->set_ghost(placing_, ghost_at_, place_orientation_);
        }
        event->accept();
        return;
    }
    if (drawing_line()) {
        update_wire_preview(at);
        event->accept();
        return;
    }
    if (probe_drag_) {
        const model::Point delta = at - drag_origin_;
        if (!(delta == drag_delta_)) {
            drag_delta_ = delta;
            // Preview: the marker follows the mouse.
            model::Schematic preview = scene_->document().schematic();
            if (const auto* p = preview.probe(probe_drag_)) {
                model::Probe moved = *p;
                moved.target = {model::no_id, {}, at};
                preview.put(moved);
                scene_->sync_from(preview);
            }
        }
        event->accept();
        return;
    }
    if (dragging_) {
        const model::Point delta = at - drag_origin_;
        if (!(delta == drag_delta_)) {
            drag_delta_ = delta;
            auto& d = scene_->document();
            model::Schematic preview = d.schematic();
            if (text_drag_) {
                editing::move_text(preview, text_drag_->first, text_drag_->second, text_start_ + delta).apply(preview);
            } else {
                editing::move_items(preview, d.library(), drag_ids_, delta).apply(preview);
            }
            scene_->sync_from(preview);
        }
        event->accept();
        return;
    }
    if (tool_ == Tool::Select) {
        show_live_tooltip(position);
    }
    QGraphicsView::mouseMoveEvent(event);
}

void SchematicView::mouseReleaseEvent(QMouseEvent* event)
{
    if (panning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        panning_ = false;
        update_cursor_shape();
        event->accept();
        return;
    }
    if (held_button_) {
        emit control_changed(held_button_, 0.0);
        held_button_ = model::no_id;
        event->accept();
        return;
    }
    if (pot_drag_) {
        pot_drag_ = model::no_id;
        event->accept();
        return;
    }
    if (probe_drag_) {
        const model::Id probe = std::exchange(probe_drag_, model::no_id);
        const bool moved = !(drag_delta_ == model::Point{}) && allowed()
            && move_probe_to(probe, grid_at(event->position().toPoint()));
        if (!moved) {
            scene_->sync();
        }
        scene_->select_ids({probe});
        event->accept();
        return;
    }
    if (dragging_) {
        dragging_ = false;
        auto& d = scene_->document();
        const auto ids = drag_ids_;
        if (drag_delta_ == model::Point{}) {
            scene_->sync();
        } else if (!allowed()) {
            scene_->sync();
        } else if (text_drag_) {
            if (!d.apply(editing::move_text(d.schematic(), text_drag_->first, text_drag_->second, text_start_ + drag_delta_))) {
                scene_->sync();
            }
        } else if (!d.apply(editing::move_items(d.schematic(), d.library(), ids, drag_delta_))) {
            scene_->sync();
        }
        text_drag_.reset();
        scene_->select_ids(ids);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if (rubber_band_) {
        rubber_band_ = false;
        setDragMode(QGraphicsView::NoDrag);
    }
}

void SchematicView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!scene_) {
        QGraphicsView::mouseDoubleClickEvent(event);
        return;
    }
    if (drawing_line()) {
        // Double-click ends the wire at an open point.
        finish_wire();
        event->accept();
        return;
    }
    if (tool_ == Tool::Select && !running_) {
        const model::Id id = scene_->element_at(mapToScene(event->position().toPoint()));
        if (scene_->document().schematic().component(id)) {
            emit edit_value_requested(id);
            event->accept();
            return;
        }
        if (const auto* note = scene_->document().schematic().note(id)) {
            emit note_text_needed(note->position, id);
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void SchematicView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        if (drawing_line()) {
            horizontal_first_ = !horizontal_first_;
            update_wire_preview(grid_at(mapFromGlobal(QCursor::pos())));
        } else {
            space_held_ = true;
            update_cursor_shape();
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (drawing_line() && !wire_points_.empty()) {
            cancel_wire();
        } else if (tool_ != Tool::Select) {
            set_tool(Tool::Select);
        } else if (scene_) {
            scene_->clearSelection();
        }
        event->accept();
        return;
    }
    if (tool_ == Tool::Select && scene_ && !scene_->selectedItems().isEmpty()) {
        const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
        switch (event->key()) {
        case Qt::Key_Left:
            move_selection({-step, 0});
            event->accept();
            return;
        case Qt::Key_Right:
            move_selection({step, 0});
            event->accept();
            return;
        case Qt::Key_Up:
            move_selection({0, -step});
            event->accept();
            return;
        case Qt::Key_Down:
            move_selection({0, step});
            event->accept();
            return;
        default:
            break;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void SchematicView::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        space_held_ = false;
        panning_ = false;
        update_cursor_shape();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void SchematicView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    // Show the whole sheet the first time the view gets a real size.
    if (!fitted_once_ && viewport()->width() > 50 && viewport()->height() > 50) {
        fitted_once_ = true;
        fit_content();
    }
}

void SchematicView::leaveEvent(QEvent* event)
{
    if (scene_ && tool_ == Tool::Place) {
        scene_->clear_ghost();
    }
    QGraphicsView::leaveEvent(event);
}

void SchematicView::dragEnterEvent(QDragEnterEvent* event)
{
    if (scene_ && event->mimeData()->hasFormat(component_mime)) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void SchematicView::dragMoveEvent(QDragMoveEvent* event)
{
    if (scene_ && event->mimeData()->hasFormat(component_mime)) {
        const auto type = event->mimeData()->data(component_mime).toStdString();
        if (const auto* d = scene_->library().find(type)) {
            scene_->set_ghost(d, grid_at(event->position().toPoint()), {});
        }
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void SchematicView::dragLeaveEvent(QDragLeaveEvent* event)
{
    if (scene_) {
        scene_->clear_ghost();
    }
    QGraphicsView::dragLeaveEvent(event);
}

void SchematicView::dropEvent(QDropEvent* event)
{
    if (scene_ && event->mimeData()->hasFormat(component_mime)) {
        scene_->clear_ghost();
        const auto type = event->mimeData()->data(component_mime).toStdString();
        if (const auto* d = scene_->library().find(type); d && allowed()) {
            auto& doc = scene_->document();
            model::Id id = model::no_id;
            doc.apply(editing::add_component(doc.id_source(), *d, grid_at(event->position().toPoint()), {}, &id));
            if (id) {
                scene_->select_ids({id});
            }
        }
        event->acceptProposedAction();
        setFocus();
        return;
    }
    QGraphicsView::dropEvent(event);
}

void SchematicView::show_live_tooltip(const QPoint& view_point)
{
    const model::Point at = grid_at(view_point);
    const auto net = scene_->net_name_at(at);
    if (!net) {
        QToolTip::hideText();
        return;
    }
    std::string key = *net;
    for (auto& c : key) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const QString name = key == "0" ? tr("Ground (0)") : QString::fromStdString(*net);
    if (!running_) {
        // UI-07: the net name on hover; labels set names, others are numbered.
        QToolTip::showText(mapToGlobal(view_point), tr("Net %1").arg(name), this);
        return;
    }
    double volts = 0.0;
    if (key != "0") {
        const auto it = scene_->live().nets.find(key);
        if (it == scene_->live().nets.end()) {
            return;
        }
        volts = it->second;
    }
    QToolTip::showText(mapToGlobal(view_point),
        tr("%1: %2V").arg(name, QString::fromStdString(model::format_engineering(volts))), this);
}

void SchematicView::update_cursor_shape()
{
    if (panning_) {
        viewport()->setCursor(Qt::ClosedHandCursor);
    } else if (space_held_) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else if (drawing_line() || tool_ == Tool::Splitter || tool_ == Tool::Probe || tool_ == Tool::Label || tool_ == Tool::Note
               || tool_ == Tool::Place) {
        viewport()->setCursor(Qt::CrossCursor);
    } else {
        viewport()->unsetCursor();
    }
}

} // namespace simnodus::ui
