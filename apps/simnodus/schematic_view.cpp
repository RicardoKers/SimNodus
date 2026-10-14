// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "schematic_view.hpp"

#include "theme.hpp"

#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
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
    setFrameShape(QFrame::NoFrame);
    // Clean canvas: pan with the middle button or Space+drag instead.
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setObjectName(QStringLiteral("schematicView"));
    setToolTip(QString());
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

void SchematicView::fit_content()
{
    QRectF target = scene()->itemsBoundingRect();
    if (target.isEmpty()) {
        target = empty_sheet;
    }
    fitInView(target.adjusted(-2 * grid, -2 * grid, 2 * grid, 2 * grid), Qt::KeepAspectRatio);
    // fitInView may exceed the zoom range for tiny or huge content.
    if (zoom() > max_zoom || zoom() < min_zoom) {
        set_zoom(zoom(), viewport()->rect().center());
    }
    emit zoom_changed(zoom());
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

void SchematicView::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    set_zoom(zoom() * std::pow(1.2, steps), event->position().toPoint());
    event->accept();
}

void SchematicView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && space_held_)) {
        panning_ = true;
        pan_origin_ = event->position().toPoint();
        update_cursor_shape();
        event->accept();
        return;
    }
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
    QGraphicsView::mouseReleaseEvent(event);
}

void SchematicView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        space_held_ = true;
        update_cursor_shape();
        event->accept();
        return;
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

void SchematicView::update_cursor_shape()
{
    if (panning_) {
        viewport()->setCursor(Qt::ClosedHandCursor);
    } else if (space_held_) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else {
        viewport()->unsetCursor();
    }
}

} // namespace simnodus::ui
