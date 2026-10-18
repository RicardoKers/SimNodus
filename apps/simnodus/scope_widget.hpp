// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Project-owned oscilloscope plot (ADR 0081, AN-03/AN-04): 10 x 8 divisions
// drawn with QPainter, min/max decimation per pixel column, per-channel
// scale and offset, two time cursors, and time zoom/pan for complete
// (transient) records. The left margin shows the selected channel's vertical
// scale and a 0 V marker per channel; dragging a marker (or the wheel over
// the margin) moves its channel up and down, a double-click centers 0 V (SN-189).
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "sim/sample_ring.hpp"
#endif

#include <QColor>
#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

namespace simnodus::ui {

struct ScopeTrace {
    QString name;
    QString unit; // "V" or "A"
    QColor color;
    std::vector<sim::Sample> samples; // increasing time
    double per_division = 1.0;
    double offset = 0.0; // value at the center line
    bool visible = true;
};

class ScopeWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int horizontal_divisions = 10;
    static constexpr int vertical_divisions = 8;

    explicit ScopeWidget(QWidget* parent = nullptr);

    void set_traces(std::vector<ScopeTrace> traces);
    const std::vector<ScopeTrace>& traces() const { return traces_; }

    double seconds_per_division() const { return (end_ - start_) / horizontal_divisions; }
    void set_seconds_per_division(double value); // keeps the window end
    void set_window(double start, double end);
    double window_start() const { return start_; }
    double window_end() const { return end_; }

    bool display_running() const { return display_running_; }
    void set_display_running(bool running);
    // Mouse zoom and pan of the time axis (complete records only).
    void set_navigation_enabled(bool enabled) { navigation_ = enabled; }

    void set_cursors_enabled(bool enabled);
    bool cursors_enabled() const { return cursors_; }
    double cursor(int index) const { return index == 0 ? cursor_a_ : cursor_b_; }
    void set_cursor(int index, double time);

    void set_trigger_marker(std::optional<double> level, QColor color);
    // Channel whose scale the left margin shows (index in traces()), -1 for none.
    void set_selected_trace(int index);
    int selected_trace() const { return selected_; }
    void set_message(const QString& message);

    QRectF plot_rect() const;
    QSize minimumSizeHint() const override { return {320, 200}; }

signals:
    void window_changed(double start, double end);
    void cursors_changed();
    void trace_selected(int index);
    void offset_changed(int index, double offset); // value at the center line

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    double time_at(double x) const;
    double x_at(double time) const;
    void paint_trace(QPainter& painter, const ScopeTrace& trace, const QRectF& plot) const;
    void paint_scale(QPainter& painter, const QRectF& plot) const;
    double zero_y(const ScopeTrace& trace, const QRectF& plot) const; // unclamped
    int marker_at(const QPointF& position) const; // trace whose 0 V marker is there, -1 for none
    void set_offset(int index, double offset);

    std::vector<ScopeTrace> traces_;
    double start_ = 0.0;
    double end_ = 10e-3;
    bool display_running_ = true;
    bool navigation_ = false;
    bool cursors_ = false;
    double cursor_a_ = 0.0;
    double cursor_b_ = 0.0;
    int dragging_cursor_ = -1;
    bool panning_ = false;
    double pan_origin_x_ = 0.0;
    double pan_origin_start_ = 0.0;
    std::optional<double> trigger_level_;
    QColor trigger_color_;
    QString message_;
    int selected_ = -1;
    int dragging_marker_ = -1;
    double drag_origin_y_ = 0.0;
    double drag_origin_offset_ = 0.0;
};

// "1 ms", "200 µs", "2 s": engineering format for axis labels.
QString format_seconds(double seconds);
// Next value in the 1-2-5 sequence that is >= value.
double nice_division(double value);

} // namespace simnodus::ui
