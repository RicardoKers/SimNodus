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
    QPoint componentPoint(const QString& id) const;
    std::function<void(const QStringList&)> selected;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
private:
    QRectF fittedView() const;
    bool resolve();
    std::shared_ptr<const simnodus::ProjectGraph> graph_;
    QStringList context_, selected_;
    std::array<simnodus::ComponentOccurrenceView, 2> components_;
    std::array<simnodus::DeclaredLocalNetDetailsView, 3> nets_;
    bool supported_ = false;
};
