// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Logic analyzer lanes (AN-07): one row per digital signal, drawn in the same
// time window as the oscilloscope, with the scope's cursors (dragged here too).
// Hovering a lane shows the time of the nearest transition. When the time
// axis can be navigated (a record, a paused run), the wheel zooms it around
// the pointer and dragging pans it, as on the scope.
#pragma once

#include <QColor>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <optional>
#include <utility>
#include <vector>

class QPainter;

namespace simnodus::ui {

// What a protocol decoder read (SN-286): a box over its time with its text;
// a start or stop condition has no length and is drawn as a mark.
struct LogicLaneItem {
    double start = 0.0; // seconds
    double end = 0.0;
    QString text;
    bool mark = false;  // a condition (I2C start, stop), not a byte
    bool error = false; // not acknowledged, framing or parity error
};

struct LogicLaneView {
    QString title;
    QColor color;
    bool initial = false;                          // level at the window start
    std::vector<std::pair<double, bool>> changes; // seconds, new level
    // A decoder's lane: these instead of a waveform.
    bool decoded = false;
    std::vector<LogicLaneItem> items;
};

class LogicWidget : public QWidget {
    Q_OBJECT
public:
    explicit LogicWidget(QWidget* parent = nullptr);

    void set_lanes(std::vector<LogicLaneView> lanes);
    const std::vector<LogicLaneView>& lanes() const { return lanes_; }
    void set_window(double start, double end);
    void set_cursors(bool enabled, double a, double b);
    void set_navigation_enabled(bool enabled) { navigation_ = enabled; }
    // Transitions of a lane inside the window (for the list under the lanes).
    std::vector<std::pair<double, bool>> visible_transitions(int lane) const;

    QSize minimumSizeHint() const override { return {320, 120}; }

signals:
    // A cursor was dragged (0 = A, 1 = B), like on the scope; the cursors are shared.
    void cursor_moved(int index, double time);
    // The wheel or a drag asks for another time window (navigation only).
    void window_requested(double start, double end);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool event(QEvent* event) override; // tooltips

private:
    QRectF plot_rect() const;
    double x_at(double time) const;
    double time_at(double x) const;
    int lane_at(double y) const;
    int cursor_near(double x) const; // -1, 0 (A) or 1 (B)
    void paint_items(QPainter& painter, const LogicLaneView& lane, const QRectF& plot, double top) const;

    std::vector<LogicLaneView> lanes_;
    double start_ = 0.0;
    double end_ = 1.0;
    bool cursors_ = false;
    double cursor_a_ = 0.0;
    double cursor_b_ = 0.0;
    int dragging_cursor_ = -1;
    bool navigation_ = false;
    bool panning_ = false;
    double pan_origin_x_ = 0.0;
    double pan_origin_start_ = 0.0;
    double pan_span_ = 0.0;
};

} // namespace simnodus::ui
