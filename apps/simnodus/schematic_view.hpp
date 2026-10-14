// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Schematic canvas (UI-09): dot grid, zoom at the cursor, pan with the middle
// button or Space+drag, fit to content. Editing tools arrive with SN-112.
#pragma once

#include <QGraphicsView>

namespace simnodus::ui {

class SchematicView : public QGraphicsView {
    Q_OBJECT
public:
    // One grid step in scene units; symbols and wires snap to it.
    static constexpr double grid = 10.0;
    static constexpr double min_zoom = 0.05;
    static constexpr double max_zoom = 40.0;

    explicit SchematicView(QWidget* parent = nullptr);

    double zoom() const;
    void set_zoom(double factor, const QPoint& anchor_in_view);
    // Whether minor grid dots are drawn at the current zoom.
    bool minor_grid_visible() const;

public slots:
    void zoom_in();
    void zoom_out();
    void fit_content();

signals:
    void cursor_moved(QPointF grid_position);
    void zoom_changed(double factor);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void update_cursor_shape();

    bool fitted_once_ = false;
    bool space_held_ = false;
    bool panning_ = false;
    QPoint pan_origin_;
};

} // namespace simnodus::ui
