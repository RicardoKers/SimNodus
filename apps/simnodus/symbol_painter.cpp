// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "symbol_painter.hpp"

#include "library/component_library.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace simnodus::ui {
namespace {

using library::Graphic;

QPointF to_q(const model::PointF& p)
{
    return {p.x, p.y};
}

void paint_graphic(QPainter& painter, const Graphic& g, const SymbolStyle& style, bool glow_target)
{
    QPen pen(style.ink, style.line_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    const QBrush fill = g.filled ? QBrush(style.ink) : (glow_target ? QBrush(style.glow) : QBrush(Qt::NoBrush));
    painter.setBrush(fill);
    switch (g.kind) {
    case Graphic::Kind::Line:
        painter.drawLine(to_q(g.points[0]), to_q(g.points[1]));
        break;
    case Graphic::Kind::Rect:
        painter.drawRect(QRectF(to_q(g.points[0]), QSizeF(g.points[1].x, g.points[1].y)));
        break;
    case Graphic::Kind::Circle:
        painter.drawEllipse(to_q(g.points[0]), g.radius, g.radius);
        break;
    case Graphic::Kind::Arc: {
        const QRectF box(g.points[0].x - g.radius, g.points[0].y - g.radius, 2 * g.radius, 2 * g.radius);
        QPainterPath path;
        path.arcMoveTo(box, g.start_degrees);
        path.arcTo(box, g.start_degrees, g.span_degrees);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        break;
    }
    case Graphic::Kind::Polyline:
    case Graphic::Kind::Polygon: {
        QPolygonF polygon;
        for (const auto& p : g.points) {
            polygon << to_q(p);
        }
        if (g.kind == Graphic::Kind::Polygon) {
            painter.drawPolygon(polygon);
        } else {
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(polygon);
        }
        break;
    }
    case Graphic::Kind::Text: {
        QFont font = painter.font();
        font.setPixelSize(100); // scaled below; avoids tiny font sizes in grid units
        font.setBold(true);
        painter.save();
        const double scale = g.text_size / 100.0;
        painter.translate(to_q(g.points[0]));
        painter.scale(scale, scale);
        painter.setFont(font);
        painter.setPen(style.ink);
        painter.drawText(QRectF(-200, -100, 400, 200), Qt::AlignCenter, QString::fromStdString(g.text));
        painter.restore();
        break;
    }
    }
}

} // namespace

QPointF wiper_tip(const library::ComponentDefinition& definition, double position)
{
    const auto& wiper = *definition.interactive->wiper;
    const double t = std::clamp(position, 0.0, 1.0);
    return {wiper.start.x + t * (wiper.end.x - wiper.start.x), wiper.start.y + t * (wiper.end.y - wiper.start.y)};
}

void paint_symbol(QPainter& painter, const library::ComponentDefinition& definition, const SymbolStyle& style)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // The first closed shape (body) receives the LED glow.
    bool glow_used = !style.glow.isValid() || style.glow.alpha() == 0;
    for (const auto& g : definition.graphics) {
        const bool target = !glow_used && g.kind == Graphic::Kind::Polygon && !g.filled;
        glow_used = glow_used || target;
        paint_graphic(painter, g, style, target);
    }
    if (!style.state.isEmpty()) {
        const auto it = definition.state_graphics.find(style.state.toStdString());
        if (it != definition.state_graphics.end()) {
            for (const auto& g : it->second) {
                paint_graphic(painter, g, style, false);
            }
        }
    }
    if (definition.interactive && definition.interactive->wiper) {
        // Wiper (UI-18): from the pin across to the track, then an arrow onto the body.
        const auto& w = *definition.interactive->wiper;
        const auto* pin = definition.pin(w.pin);
        const QPointF tip = wiper_tip(definition, style.wiper);
        if (pin) {
            const QPointF from(pin->at.x, pin->at.y);
            // The bend stays next to the body, clear of the designator and value texts.
            const double base = tip.x() + 0.7;
            const double turn = base + 0.2;
            painter.setPen(QPen(style.ink, style.line_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(QPolygonF{from, QPointF(turn, from.y()), QPointF(turn, tip.y()), QPointF(base, tip.y())});
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(style.ink);
        painter.drawPolygon(QPolygonF{tip, QPointF(tip.x() + 0.7, tip.y() - 0.4), QPointF(tip.x() + 0.7, tip.y() + 0.4)});
    }
    if (style.draw_pins) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(style.ink);
        for (const auto& pin : definition.pins) {
            painter.drawEllipse(QPointF(pin.at.x, pin.at.y), 0.18, 0.18);
        }
    }
    painter.restore();
}

QRectF symbol_bounds(const library::ComponentDefinition& definition)
{
    // QRectF::united ignores zero-size rectangles, so track the extremes.
    double left = 0, top = 0, right = 0, bottom = 0;
    bool empty = true;
    auto add = [&](double x, double y) {
        if (empty) {
            left = right = x;
            top = bottom = y;
            empty = false;
            return;
        }
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
    };
    auto add_graphic = [&](const Graphic& g) {
        switch (g.kind) {
        case Graphic::Kind::Rect:
            add(g.points[0].x, g.points[0].y);
            add(g.points[0].x + g.points[1].x, g.points[0].y + g.points[1].y);
            break;
        case Graphic::Kind::Circle:
        case Graphic::Kind::Arc:
            add(g.points[0].x - g.radius, g.points[0].y - g.radius);
            add(g.points[0].x + g.radius, g.points[0].y + g.radius);
            break;
        case Graphic::Kind::Text:
            add(g.points[0].x - g.text_size / 2, g.points[0].y - g.text_size / 2);
            add(g.points[0].x + g.text_size / 2, g.points[0].y + g.text_size / 2);
            break;
        default:
            for (const auto& p : g.points) {
                add(p.x, p.y);
            }
        }
    };
    for (const auto& g : definition.graphics) {
        add_graphic(g);
    }
    for (const auto& [state, graphics] : definition.state_graphics) {
        for (const auto& g : graphics) {
            add_graphic(g);
        }
    }
    for (const auto& pin : definition.pins) {
        add(pin.at.x, pin.at.y);
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom)).adjusted(-0.3, -0.3, 0.3, 0.3);
}

} // namespace simnodus::ui
