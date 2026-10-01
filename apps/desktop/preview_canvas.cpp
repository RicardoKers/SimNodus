// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "preview_canvas.hpp"
#include <QPainter>
#include <algorithm>

PreviewCanvas::PreviewCanvas()
{
    setMinimumSize(100, 64);
    setAccessibleName("Captured owned fixture artwork; pin anchors unverified");
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
}
