// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Draws a chart recorder (SN-281, AN-11): the plot of its channels' records
// and reference data, with time and value axes from the part's parameters.
// Used by the schematic item and by the PNG export.
#pragma once

#ifndef Q_MOC_RUN
#include "io/reference_csv.hpp"
#include "library/component_library.hpp"
#include "sim/chart_record.hpp"
#endif

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>

#include <map>
#include <string>
#include <vector>

class QPainter;

namespace simnodus::ui {

struct ChartInput {
    const library::ComponentDefinition* definition = nullptr; // with a chart
    const std::map<std::string, std::string>* parameters = nullptr;
    const std::vector<sim::ChartRecord>* records = nullptr; // per channel; null before the first run
    std::vector<const io::Series*> references;              // per channel; null or empty for none
    std::vector<QColor> colors;                             // per channel
    bool selected = false;
};

struct ChartAxes {
    double t0 = 0.0; // seconds
    double t1 = 1.0;
    double v0 = 0.0; // volts
    double v1 = 1.0;
    QString unit = QStringLiteral("s"); // of the time axis
    double divisor = 1.0;               // seconds per unit
};

// The axes the part's parameters and data give.
ChartAxes chart_axes(const ChartInput& input);

// Round tick values from `low` to `high`, at most `max_count` of them (steps
// of 1, 2 or 5 times a power of ten); `step` receives the step.
std::vector<double> nice_ticks(double low, double high, int max_count, double* step = nullptr);

// The plot in `box` (scene or image units; the text size is fixed).
void paint_chart(QPainter& painter, const QRectF& box, const ChartInput& input);

// The chart of a placed part as an image (the PNG export): its plot size on
// the schematic at 100 % zoom times `scale`.
QImage render_chart(const library::ComponentDefinition& definition, const model::Component& component,
    const std::vector<sim::ChartRecord>* records, double scale = 2.0);

} // namespace simnodus::ui
