// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Diagram export (SN-310): the schematic as an SVG or PNG image for
// documents and slides, drawn in the light or the dark colors whatever the
// window's theme, without the grid and the selection. While a run is active
// the image shows what the canvas shows (lit LEDs, readouts, displays).
#pragma once

#include <QRectF>
#include <QString>

namespace simnodus::ui {

class SchematicScene;

struct DiagramExportOptions {
    bool svg = false;     // else PNG
    bool dark = false;    // dark background and light lines
    double scale = 2.0;   // PNG: pixels per scene unit (one grid step is 10 scene units)
};

// The circuit's bounds in scene units, with a margin; empty for an empty circuit.
QRectF diagram_rect(const SchematicScene& scene);

// Writes the image to `path`; false and `error` when it cannot.
bool export_diagram(SchematicScene& scene, const QString& path, const DiagramExportOptions& options, QString& error);

} // namespace simnodus::ui
