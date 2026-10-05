// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_inspection.hpp"
#include <QJsonObject>
#include <QStringList>
#include <QWidget>
#include <array>
#include <functional>

// Disposable presentation geometry for one closed declared RC shape.
// The notation is owned drawing code, independent of project symbol resources.
class RcCanvas final : public QWidget {
public:
    RcCanvas();
    void setGraph(std::shared_ptr<const simnodus::ProjectGraph> graph, const QStringList& context);
    void setSelection(const QStringList& path);
    bool supported() const { return supported_; }
    const QStringList& selection() const { return selected_; }
    QJsonObject snapshot() const;
    static QString parameterCaption(const simnodus::EffectiveParameter& parameter);
    QPoint componentPoint(const QString& id) const;
    int zoomPercent() const { return 100 + 25 * zoom_step_; }
    bool zoomIn();
    bool zoomOut();
    void fitView();
    std::function<void(const QStringList&)> selected;
    std::function<void()> zoomed;
protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
private:
    QRectF viewRect() const;
    void cancelPan();
    bool resolve();
    std::shared_ptr<const simnodus::ProjectGraph> graph_;
    QStringList context_, selected_;
    std::array<simnodus::ComponentOccurrenceView, 2> components_;
    std::array<simnodus::DeclaredLocalNetDetailsView, 3> nets_;
    bool supported_ = false;
    int zoom_step_ = 0;
    QPointF pan_offset_, drag_position_;
    bool panning_ = false;
};
