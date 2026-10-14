// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Project-owned oscilloscope plot (ADR 0081): 10 x 8 divisions drawn with
// QPainter. Phase 0 draws the graticule and time axis only; traces, trigger,
// cursors and measurements arrive with SN-120.
#pragma once

#include <QWidget>

namespace simnodus::ui {

class ScopeWidget : public QWidget {
    Q_OBJECT
public:
    static constexpr int horizontal_divisions = 10;
    static constexpr int vertical_divisions = 8;

    explicit ScopeWidget(QWidget* parent = nullptr);

    double seconds_per_division() const { return seconds_per_division_; }
    void set_seconds_per_division(double value);
    bool display_running() const { return display_running_; }
    void set_display_running(bool running);
    // Plot rectangle inside the widget (for tests and future cursors).
    QRectF plot_rect() const;

    QSize minimumSizeHint() const override { return {320, 200}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double seconds_per_division_ = 1e-3;
    bool display_running_ = true;
};

// "1 ms", "200 us", "2 s": engineering format for axis labels.
QString format_seconds(double seconds);

} // namespace simnodus::ui
