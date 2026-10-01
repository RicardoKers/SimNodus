// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "preview_canvas.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <algorithm>

PreviewCanvas::PreviewCanvas()
{
    setMinimumSize(100, 64);
    setAccessibleName("Captured owned fixture artwork and available declared pin annotations");
}
void PreviewCanvas::setCapture(std::shared_ptr<const simnodus::SymbolPreviewCapture> capture)
{
    capture_ = std::move(capture);
    update();
}
QRectF PreviewCanvas::fittedView() const
{
    if(!capture_ || width() <= 24 || height() <= 24) return {};
    const auto& artwork = capture_->artwork;
    const auto scale = std::min((width() - 24.0) / artwork.width, (height() - 24.0) / artwork.height);
    const auto w = artwork.width * scale, h = artwork.height * scale;
    return {(width() - w) / 2, (height() - h) / 2, w, h};
}
void PreviewCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    if(!capture_) return;
    const auto view = fittedView();
    if(view.isEmpty()) return;
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    const auto& artwork = capture_->artwork;
    const auto point = [&](double x, double y) {
        return QPointF(view.left() + x * view.width() / artwork.width,
            view.top() + y * view.height() / artwork.height);
    };
    for(const auto& line : artwork.lines) painter.drawLine(point(line.x1, line.y1), point(line.x2, line.y2));
    if(capture_->pins) {
        painter.setPen(QColor(0, 96, 192));
        painter.setBrush(QColor(0, 96, 192));
        const QFontMetrics metrics(painter.font());
        const auto label_width = std::max(0, static_cast<int>(view.width() / 2) - 12);
        for(const auto& pin : *capture_->pins) {
            const auto anchor = point(pin.x, pin.y);
            painter.drawEllipse(anchor, 4, 4);
            const auto label = metrics.elidedText(QString::fromStdString(pin.logical_pin + "/" + pin.symbol_pin), Qt::ElideRight, label_width);
            const auto left = pin.symbol_pin == "a" ? anchor.x() + 6 : anchor.x() - label_width - 6;
            painter.drawText(QRectF(left, anchor.y() + 6, label_width, metrics.height()),
                pin.symbol_pin == "a" ? Qt::AlignLeft : Qt::AlignRight, label);
        }
    }
}
