// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "chart_painter.hpp"

#include "schematic_scene.hpp"
#include "theme.hpp"

#include "model/engineering.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QObject>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace simnodus::ui {
namespace {

std::string parameter(const ChartInput& input, const std::string& name)
{
    if (input.parameters) {
        if (const auto it = input.parameters->find(name); it != input.parameters->end()) {
            return it->second;
        }
    }
    const auto* p = input.definition ? input.definition->parameter(name) : nullptr;
    return p ? p->default_value : std::string();
}

double number(const ChartInput& input, const std::string& name, const char* unit, double fallback)
{
    return model::parse_engineering(parameter(input, name), unit).value_or(fallback);
}

QFont chart_font(bool bold = false)
{
    QFont font;
    font.setPixelSize(10);
    font.setBold(bold);
    return font;
}

QString tick_text(double value, double step)
{
    const int decimals = step > 0 ? std::clamp(static_cast<int>(-std::floor(std::log10(step) + 1e-9)), 0, 6) : 0;
    if (std::abs(value) < step * 1e-6) {
        value = 0.0;
    }
    return QString::number(value, 'f', decimals);
}

QString volts(double value)
{
    if (std::abs(value) < 1e-12) {
        return QStringLiteral("0 V");
    }
    return QString::fromStdString(model::format_engineering(value, 4)) + QStringLiteral(" V");
}

} // namespace

std::vector<double> nice_ticks(double low, double high, int max_count, double* step_out)
{
    std::vector<double> ticks;
    if (!(high > low) || max_count < 1 || !std::isfinite(low) || !std::isfinite(high)) {
        return ticks;
    }
    const double raw = (high - low) / max_count;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    double step = magnitude;
    for (const double m : {1.0, 2.0, 5.0, 10.0}) {
        step = m * magnitude;
        if (step >= raw) {
            break;
        }
    }
    const double first = std::ceil(low / step - 1e-9) * step;
    for (double v = first; v <= high + step * 1e-9; v += step) {
        ticks.push_back(std::abs(v) < step * 1e-9 ? 0.0 : v);
        if (ticks.size() > 1000) {
            break;
        }
    }
    if (step_out) {
        *step_out = step;
    }
    return ticks;
}

ChartAxes chart_axes(const ChartInput& input)
{
    ChartAxes axes;
    const double window = std::max(number(input, "time_window", "s", 60.0), 1e-6);
    double now = 0.0;
    if (input.records) {
        for (const auto& r : *input.records) {
            now = std::max(now, r.end_time());
        }
    }
    if (parameter(input, "time_mode") == "rolling" && now > window) {
        axes.t0 = now - window;
        axes.t1 = now;
    } else {
        axes.t0 = 0.0;
        axes.t1 = window;
    }
    std::string unit = parameter(input, "time_unit");
    if (unit != "s" && unit != "min" && unit != "h") {
        unit = window <= 180.0 ? "s" : window <= 3.0 * 3600.0 ? "min" : "h";
    }
    axes.unit = QString::fromStdString(unit);
    axes.divisor = unit == "h" ? 3600.0 : unit == "min" ? 60.0 : 1.0;

    if (parameter(input, "vertical") == "manual") {
        axes.v0 = number(input, "v_min", "V", 0.0);
        axes.v1 = number(input, "v_max", "V", 5.0);
        if (axes.v1 < axes.v0) {
            std::swap(axes.v0, axes.v1);
        }
        if (axes.v1 - axes.v0 < 1e-12) {
            axes.v1 = axes.v0 + 1.0;
        }
        return axes;
    }
    double low = std::numeric_limits<double>::infinity();
    double high = -low;
    if (input.records) {
        for (const auto& r : *input.records) {
            for (const auto& b : r.buckets()) {
                if (b.last_time >= axes.t0 && b.first_time <= axes.t1) {
                    low = std::min(low, b.low);
                    high = std::max(high, b.high);
                }
            }
        }
    }
    for (const auto* series : input.references) {
        if (!series) {
            continue;
        }
        for (const auto& [t, v] : *series) {
            if (t >= axes.t0 && t <= axes.t1) {
                low = std::min(low, v);
                high = std::max(high, v);
            }
        }
    }
    if (!(high >= low)) {
        axes.v0 = 0.0;
        axes.v1 = 1.0;
        return axes;
    }
    if (high - low < 1e-9) {
        const double half = std::max(std::abs(high) * 0.1, 0.5);
        low -= half;
        high += half;
    }
    // A little room, then out to round values.
    const double pad = (high - low) * 0.05;
    low -= pad;
    high += pad;
    double step = 0.0;
    nice_ticks(low, high, 6, &step);
    if (step > 0) {
        low = std::floor(low / step + 1e-9) * step;
        high = std::ceil(high / step - 1e-9) * step;
    }
    axes.v0 = low;
    axes.v1 = high;
    return axes;
}

void paint_chart(QPainter& painter, const QRectF& box, const ChartInput& input)
{
    if (!input.definition || !input.definition->chart) {
        return;
    }
    const auto& chart = *input.definition->chart;
    const ChartAxes axes = chart_axes(input);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Frame.
    painter.setPen(QPen(input.selected ? theme::selection : theme::ink, 1.4));
    painter.setBrush(theme::plot_background);
    painter.drawRect(box);

    const QFont font = chart_font();
    const QFontMetricsF metrics(font);
    const double line = metrics.height();

    // Value labels: the widest one sets the left margin.
    const QRectF probe = box.adjusted(0, line + 6, 0, -(line + 6));
    double v_step = 0.0;
    const auto v_ticks = nice_ticks(axes.v0, axes.v1, std::max(2, static_cast<int>(probe.height() / (line * 2.2))), &v_step);
    double label_width = metrics.horizontalAdvance(QStringLiteral("0"));
    for (const double v : v_ticks) {
        label_width = std::max(label_width, metrics.horizontalAdvance(tick_text(v, v_step)));
    }
    const QRectF plot(box.left() + label_width + 10, box.top() + line + 10, 0, 0);
    QRectF area = plot;
    area.setRight(box.right() - 8);
    area.setBottom(box.bottom() - line - 6);
    if (area.width() < 20 || area.height() < 20) {
        painter.restore();
        return;
    }
    auto x_of = [&](double t) { return area.left() + (t - axes.t0) / (axes.t1 - axes.t0) * area.width(); };
    auto y_of = [&](double v) { return area.bottom() - (v - axes.v0) / (axes.v1 - axes.v0) * area.height(); };

    // Grid and tick labels.
    painter.setFont(font);
    double t_step = 0.0;
    const int t_count = std::max(2, static_cast<int>(area.width() / (metrics.horizontalAdvance(QStringLiteral("0000.0")) * 1.6)));
    const auto t_ticks = nice_ticks(axes.t0 / axes.divisor, axes.t1 / axes.divisor, t_count, &t_step);
    painter.setPen(QPen(theme::plot_grid, 1.0));
    for (const double v : v_ticks) {
        painter.drawLine(QPointF(area.left(), y_of(v)), QPointF(area.right(), y_of(v)));
    }
    for (const double t : t_ticks) {
        painter.drawLine(QPointF(x_of(t * axes.divisor), area.top()), QPointF(x_of(t * axes.divisor), area.bottom()));
    }
    painter.setPen(QPen(theme::plot_axis, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(area);
    painter.setPen(theme::muted_text);
    for (const double v : v_ticks) {
        const QRectF r(box.left() + 2, y_of(v) - line / 2, label_width + 4, line);
        painter.drawText(r, Qt::AlignRight | Qt::AlignVCenter, tick_text(v, v_step));
    }
    const QString unit_text = axes.unit;
    const double unit_width = metrics.horizontalAdvance(unit_text) + 6;
    for (const double t : t_ticks) {
        const QString text = tick_text(t, t_step);
        const double w = metrics.horizontalAdvance(text);
        const double x = std::clamp(x_of(t * axes.divisor) - w / 2, area.left() - 4, area.right() - w - unit_width);
        if (x + w > area.right() - unit_width + 1) {
            continue;
        }
        painter.drawText(QRectF(x, area.bottom() + 2, w + 2, line), Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    painter.drawText(QRectF(area.right() - unit_width, area.bottom() + 2, unit_width, line), Qt::AlignRight | Qt::AlignVCenter,
        unit_text);
    painter.drawText(QRectF(box.left() + 2, box.top() + 3, label_width + 4, line), Qt::AlignRight | Qt::AlignVCenter,
        QStringLiteral("V"));

    // Header: each channel's latest value in its color.
    double x = area.left();
    painter.setFont(chart_font(true));
    const QFontMetricsF bold(chart_font(true));
    for (std::size_t i = 0; i < chart.channels.size(); ++i) {
        const QColor color = i < input.colors.size() ? input.colors[i] : theme::ink;
        QString text = QStringLiteral("CH%1").arg(i + 1);
        const sim::ChartRecord* record = input.records && i < input.records->size() ? &(*input.records)[i] : nullptr;
        if (record && record->latest()) {
            text += QStringLiteral(" ") + volts(record->latest()->value);
        }
        const io::Series* reference = i < input.references.size() ? input.references[i] : nullptr;
        const bool referenced = reference && !reference->empty();
        bool recorded = false;
        if (input.records) {
            for (const auto& r : *input.records) {
                recorded = recorded || !r.empty();
            }
        }
        if (recorded && (!record || record->empty()) && !referenced) {
            continue; // a channel left open in this run
        }
        if (referenced) {
            text += QObject::tr("  (ref ---)");
        }
        painter.setPen(color);
        const double w = bold.horizontalAdvance(text);
        if (x + w > box.right() - 4) {
            break;
        }
        painter.drawText(QRectF(x, box.top() + 3, w + 2, line), Qt::AlignLeft | Qt::AlignVCenter, text);
        x += w + 14;
    }

    // Traces and references, clipped to the plot area.
    painter.setClipRect(area.adjusted(-1, -1, 1, 1));
    for (std::size_t i = 0; i < chart.channels.size(); ++i) {
        const QColor color = i < input.colors.size() ? input.colors[i] : theme::ink;
        if (const io::Series* reference = i < input.references.size() ? input.references[i] : nullptr;
            reference && !reference->empty()) {
            QPainterPath path;
            bool started = false;
            for (std::size_t k = 0; k < reference->size(); ++k) {
                const auto& [t, v] = (*reference)[k];
                const bool next_inside = k + 1 < reference->size() && (*reference)[k + 1].first >= axes.t0;
                if (t > axes.t1 && started) {
                    path.lineTo(x_of(t), y_of(v));
                    break;
                }
                if (t < axes.t0 && !next_inside) {
                    continue;
                }
                const QPointF p(x_of(t), y_of(v));
                started ? path.lineTo(p) : path.moveTo(p);
                started = true;
            }
            QColor faded = color;
            faded.setAlphaF(0.85f);
            QPen pen(faded, 1.4, Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
            if (reference->size() <= 60) {
                // Few points are measurements: mark each one.
                painter.setPen(QPen(faded, 1.2));
                for (const auto& [t, v] : *reference) {
                    if (t >= axes.t0 && t <= axes.t1) {
                        painter.drawEllipse(QPointF(x_of(t), y_of(v)), 2.0, 2.0);
                    }
                }
            }
        }
        const sim::ChartRecord* record = input.records && i < input.records->size() ? &(*input.records)[i] : nullptr;
        if (!record || record->empty()) {
            continue;
        }
        QPainterPath path;
        bool started = false;
        const auto& buckets = record->buckets();
        for (std::size_t k = 0; k < buckets.size(); ++k) {
            const auto& b = buckets[k];
            if (b.last_time < axes.t0 && !(k + 1 < buckets.size() && buckets[k + 1].first_time >= axes.t0)) {
                continue;
            }
            const QPointF first(x_of(b.first_time), y_of(b.first));
            started ? path.lineTo(first) : path.moveTo(first);
            started = true;
            if (y_of(b.low) - y_of(b.high) > 1.0) {
                // Faster than the bucket: its whole range, at the bucket's middle.
                const double mid = x_of((b.first_time + b.last_time) / 2);
                path.lineTo(mid, y_of(b.high));
                path.lineTo(mid, y_of(b.low));
            }
            path.lineTo(x_of(b.last_time), y_of(b.last));
            if (b.first_time > axes.t1) {
                break;
            }
        }
        painter.setPen(QPen(color, 1.6));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
    painter.setClipping(false);
    bool data = false;
    if (input.records) {
        for (const auto& r : *input.records) {
            data = data || !r.empty();
        }
    }
    for (const auto* reference : input.references) {
        data = data || (reference && !reference->empty());
    }
    if (!data) {
        painter.setPen(theme::muted_text);
        painter.setFont(font);
        painter.drawText(area, Qt::AlignCenter, QObject::tr("Records the run from its start"));
    }
    painter.restore();
}

QImage render_chart(const library::ComponentDefinition& definition, const model::Component& component,
    const std::vector<sim::ChartRecord>* records, double scale)
{
    if (!definition.chart) {
        return {};
    }
    const auto& chart = *definition.chart;
    const auto r = library::chart_rect(chart, definition, component.parameters);
    const QSizeF size(r[2] * scene_grid, r[3] * scene_grid);
    QImage image((size * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(theme::plot_background);
    std::vector<io::Series> references;
    for (const auto& channel : chart.channels) {
        const auto it = component.parameters.find(channel.reference_parameter);
        references.push_back(io::decode_series(it == component.parameters.end() ? std::string() : it->second));
    }
    ChartInput input;
    input.definition = &definition;
    input.parameters = &component.parameters;
    input.records = records;
    for (std::size_t i = 0; i < references.size(); ++i) {
        input.references.push_back(&references[i]);
        input.colors.push_back(probe_color(static_cast<int>(i)));
    }
    QPainter painter(&image);
    painter.scale(scale, scale);
    paint_chart(painter, QRectF(QPointF(0.5, 0.5), size - QSizeF(1, 1)), input);
    return image;
}

} // namespace simnodus::ui
