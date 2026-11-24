// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "diagram_export.hpp"

#include "schematic_scene.hpp"
#include "theme.hpp"

#include <QCoreApplication>
#include <QGraphicsItem>
#include <QImage>
#include <QPainter>
#include <QSignalBlocker>
#include <QSvgGenerator>

#include <cmath>
#include <set>

namespace simnodus::ui {

namespace {

constexpr double margin = 2 * scene_grid;
constexpr double max_png_side = 16000; // pixels

// Draws the scene in the chosen colors and without the selection, and puts
// both back afterwards.
class ExportState {
public:
    ExportState(SchematicScene& scene, bool dark) : scene_(scene), colors_(theme::canvas_colors()), selected_(scene.selected_ids())
    {
        theme::set_canvas_colors(theme::canvas_colors(dark));
        const QSignalBlocker blocker(&scene_);
        scene_.clearSelection();
    }
    ~ExportState()
    {
        theme::set_canvas_colors(colors_);
        const QSignalBlocker blocker(&scene_);
        for (const model::Id id : selected_) {
            if (auto* item = scene_.item_of(id)) {
                item->setSelected(true);
            }
        }
    }
    ExportState(const ExportState&) = delete;
    ExportState& operator=(const ExportState&) = delete;

private:
    SchematicScene& scene_;
    theme::CanvasColors colors_;
    std::set<model::Id> selected_;
};

} // namespace

QRectF diagram_rect(const SchematicScene& scene)
{
    QRectF target;
    for (auto* item : scene.items()) {
        if (scene.id_of(item) != model::no_id && item->isVisible()) {
            target = target.isNull() ? item->sceneBoundingRect() : target.united(item->sceneBoundingRect());
        }
    }
    if (target.isEmpty()) {
        return {};
    }
    return target.adjusted(-margin, -margin, margin, margin);
}

bool export_diagram(SchematicScene& scene, const QString& path, const DiagramExportOptions& options, QString& error)
{
    const QRectF source = diagram_rect(scene);
    if (source.isEmpty()) {
        error = QCoreApplication::translate("DiagramExport", "The diagram is empty.");
        return false;
    }
    const ExportState state(scene, options.dark);
    if (options.svg) {
        QSvgGenerator svg;
        svg.setFileName(path);
        svg.setSize(source.size().toSize());
        svg.setViewBox(QRectF(QPointF(0, 0), source.size()));
        svg.setTitle(QCoreApplication::translate("DiagramExport", "SimNodus diagram"));
        svg.setDescription(QStringLiteral("SimNodus"));
        QPainter painter;
        if (!painter.begin(&svg)) {
            error = QCoreApplication::translate("DiagramExport", "Could not write %1.").arg(path);
            return false;
        }
        const QRectF target(QPointF(0, 0), source.size());
        painter.fillRect(target, theme::canvas_background);
        scene.render(&painter, target, source);
        painter.end();
        return true;
    }
    double scale = std::max(0.1, options.scale);
    scale = std::min(scale, max_png_side / std::max(source.width(), source.height()));
    const QSize size(static_cast<int>(std::ceil(source.width() * scale)), static_cast<int>(std::ceil(source.height() * scale)));
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(theme::canvas_background);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        scene.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)), source);
    }
    // One scene unit is a pixel of a 96 dpi screen at scale 1: the printed size stays the same at any scale.
    const int dots_per_meter = static_cast<int>(std::lround(scale * 3780.0));
    image.setDotsPerMeterX(dots_per_meter);
    image.setDotsPerMeterY(dots_per_meter);
    if (!image.save(path, "PNG")) {
        error = QCoreApplication::translate("DiagramExport", "Could not write %1.").arg(path);
        return false;
    }
    return true;
}

} // namespace simnodus::ui
