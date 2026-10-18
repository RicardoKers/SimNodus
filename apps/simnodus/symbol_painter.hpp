// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Draws library symbols (ComponentDefinition graphics) with QPainter. The
// painter's coordinate system must be in grid units with the symbol origin
// at (0, 0); one generic painter serves every component type (ADR 0081).
#pragma once

#include <QColor>
#include <QRectF>
#include <QString>

class QPainter;

namespace simnodus::library {
struct ComponentDefinition;
}

namespace simnodus::ui {

struct SymbolStyle {
    QColor ink;
    double line_width = 0.14;  // grid units
    QString state;             // control state graphics to draw ("0", "1"), empty = none
    QColor glow;               // LED glow (transparent = off)
    bool draw_pins = false;    // small pin markers (preview, placement)
    double wiper = 0.5;        // potentiometer position, 0..1 (UI-18)
};

// Tip of a potentiometer's wiper arrow at `position` (0..1), in grid units; the
// definition must have an interactive wiper.
QPointF wiper_tip(const library::ComponentDefinition& definition, double position);

void paint_symbol(QPainter& painter, const library::ComponentDefinition& definition, const SymbolStyle& style);

// Bounding box of the symbol graphics and pins, in grid units.
QRectF symbol_bounds(const library::ComponentDefinition& definition);

} // namespace simnodus::ui
