// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "scope_widget.hpp"

#include "theme.hpp"

#include "model/engineering.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace simnodus::ui {

QString format_seconds(double seconds)
{
    if (seconds == 0.0) {
        return QStringLiteral("0");
    }
    const double magnitude = std::abs(seconds);
    struct Unit {
        double scale;
        const char* suffix;
    };
    static const Unit units[] = {{1.0, "s"}, {1e-3, "ms"}, {1e-6, "µs"}, {1e-9, "ns"}};
    for (const auto& unit : units) {
        if (magnitude >= unit.scale * 0.9999) {
            return QStringLiteral("%1 %2").arg(
                QString::number(seconds / unit.scale, 'g', 4), QString::fromUtf8(unit.suffix));
        }
    }
    return QStringLiteral("%1 ns").arg(QString::number(seconds / 1e-9, 'g', 4));
}

double nice_division(double value)
{
    if (value <= 0 || !std::isfinite(value)) {
        return 1.0;
    }
    const double decade = std::pow(10.0, std::floor(std::log10(value)));
    for (double m : {1.0, 2.0, 5.0, 10.0}) {
        if (m * decade >= value * (1 - 1e-9)) {
            return m * decade;
        }
    }
    return 10 * decade;
}

ScopeWidget::ScopeWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("scopeWidget"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
}

void ScopeWidget::set_traces(std::vector<ScopeTrace> traces)
{
    traces_ = std::move(traces);
    update();
}

void ScopeWidget::set_seconds_per_division(double value)
{
    if (value > 0.0) {
        set_window(end_ - value * horizontal_divisions, end_);
    }
}

void ScopeWidget::set_window(double start, double end)
{
    if (!(end > start) || !std::isfinite(start) || !std::isfinite(end)) {
        return;
    }
    start_ = start;
    end_ = end;
    update();
}

void ScopeWidget::set_display_running(bool running)
{
    display_running_ = running;
    update();
}

void ScopeWidget::set_cursors_enabled(bool enabled)
{
    cursors_ = enabled;
    if (enabled && (cursor_a_ < start_ || cursor_a_ > end_ || cursor_b_ < start_ || cursor_b_ > end_)) {
        cursor_a_ = start_ + (end_ - start_) * 0.3;
        cursor_b_ = start_ + (end_ - start_) * 0.6;
    }
    update();
    emit cursors_changed();
}

void ScopeWidget::set_cursor(int index, double time)
{
    (index == 0 ? cursor_a_ : cursor_b_) = time;
    update();
    emit cursors_changed();
}

void ScopeWidget::set_trigger_marker(std::optional<double> level, QColor color)
{
    trigger_level_ = level;
    trigger_color_ = color;
    update();
}

void ScopeWidget::set_message(const QString& message)
{
    message_ = message;
    update();
}

QRectF ScopeWidget::plot_rect() const
{
    const QFontMetrics metrics(font());
    const double left = 12.0;
    const double bottom = metrics.height() + 10.0;
    return QRectF(left, 10.0, std::max(10.0, width() - left - 12.0), std::max(10.0, height() - 10.0 - bottom));
}

double ScopeWidget::time_at(double x) const
{
    const QRectF plot = plot_rect();
    return start_ + (x - plot.left()) / plot.width() * (end_ - start_);
}

double ScopeWidget::x_at(double time) const
{
    const QRectF plot = plot_rect();
    return plot.left() + (time - start_) / (end_ - start_) * plot.width();
}

void ScopeWidget::paint_trace(QPainter& painter, const ScopeTrace& trace, const QRectF& plot) const
{
    if (trace.samples.empty() || trace.per_division <= 0) {
        return;
    }
    const double units_per_pixel = trace.per_division * vertical_divisions / plot.height();
    auto y_at = [&](double v) {
        const double y = plot.center().y() - (v - trace.offset) / units_per_pixel;
        return std::clamp(y, plot.top() - 2.0, plot.bottom() + 2.0); // clipped at the edges like a scope
    };
    const auto& s = trace.samples;
    auto first =
        std::lower_bound(s.begin(), s.end(), start_, [](const sim::Sample& a, double t) { return a.time < t; });
    if (first != s.begin()) {
        --first;
    }
    QPolygonF line;
    const int columns = static_cast<int>(plot.width());
    const auto count = static_cast<std::ptrdiff_t>(std::distance(first, s.end()));
    if (count > 2 * columns) {
        // Min/max per pixel column keeps narrow spikes visible.
        int column = -1;
        double lo = 0, hi = 0, last = 0;
        for (auto it = first; it != s.end() && it->time <= end_; ++it) {
            const int c = static_cast<int>(x_at(it->time) - plot.left());
            if (c != column) {
                if (column >= 0) {
                    const double x = plot.left() + column;
                    line << QPointF(x, y_at(lo)) << QPointF(x, y_at(hi)) << QPointF(x, y_at(last));
                }
                column = c;
                lo = hi = it->value;
            }
            lo = std::min(lo, it->value);
            hi = std::max(hi, it->value);
            last = it->value;
        }
        if (column >= 0) {
            const double x = plot.left() + column;
            line << QPointF(x, y_at(lo)) << QPointF(x, y_at(hi));
        }
    } else {
        for (auto it = first; it != s.end(); ++it) {
            line << QPointF(x_at(it->time), y_at(it->value));
            if (it->time > end_) {
                break;
            }
        }
    }
    painter.setPen(QPen(trace.color, 1.4));
    painter.drawPolyline(line);
}

void ScopeWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    const QRectF plot = plot_rect();
    painter.fillRect(plot, theme::plot_background);

    QPen grid(theme::plot_grid);
    grid.setCosmetic(true);
    painter.setPen(grid);
    for (int i = 1; i < horizontal_divisions; ++i) {
        const double x = plot.left() + plot.width() * i / horizontal_divisions;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    for (int i = 1; i < vertical_divisions; ++i) {
        const double y = plot.top() + plot.height() * i / vertical_divisions;
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    QPen axis(theme::plot_axis);
    axis.setCosmetic(true);
    painter.setPen(axis);
    painter.drawRect(plot);
    painter.drawLine(QPointF(plot.left(), plot.center().y()), QPointF(plot.right(), plot.center().y()));

    painter.setPen(theme::muted_text);
    const QFontMetrics metrics(font());
    const double div = seconds_per_division();
    for (int i = 0; i <= horizontal_divisions; i += 2) {
        const double x = plot.left() + plot.width() * i / horizontal_divisions;
        const QString label = format_seconds(start_ + div * i);
        const double w = metrics.horizontalAdvance(label);
        const double left = std::clamp(x - w / 2, 0.0, std::max(0.0, width() - w));
        painter.drawText(QPointF(left, plot.bottom() + metrics.ascent() + 4), label);
    }

    painter.save();
    painter.setClipRect(plot);
    painter.setRenderHint(QPainter::Antialiasing, true);
    bool any = false;
    for (const auto& trace : traces_) {
        if (trace.visible) {
            paint_trace(painter, trace, plot);
            any = any || !trace.samples.empty();
        }
    }
    if (trigger_level_) {
        // Trigger level marker at the right edge (y in pixels).
        painter.setPen(QPen(trigger_color_, 1.5));
        painter.drawLine(QPointF(plot.right() - 14, *trigger_level_), QPointF(plot.right(), *trigger_level_));
    }
    if (cursors_) {
        for (int i = 0; i < 2; ++i) {
            const double x = x_at(i == 0 ? cursor_a_ : cursor_b_);
            painter.setPen(QPen(theme::ink, 1, Qt::DashLine));
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            painter.drawText(QPointF(x + 3, plot.bottom() - 4), i == 0 ? QStringLiteral("A") : QStringLiteral("B"));
        }
    }
    painter.restore();

    // Legend: channel names and scales.
    double y = plot.top() + 4;
    for (const auto& trace : traces_) {
        if (!trace.visible) {
            continue;
        }
        const QString text = QStringLiteral("%1  %2%3/div").arg(trace.name,
            QString::fromStdString(model::format_engineering(trace.per_division, 3)), trace.unit);
        painter.fillRect(QRectF(plot.left() + 6, y + 5, 12, 3), trace.color);
        painter.setPen(theme::ink);
        painter.drawText(QPointF(plot.left() + 22, y + metrics.ascent()), text);
        y += metrics.height();
    }
    if (!any) {
        painter.setPen(theme::muted_text);
        painter.drawText(plot.adjusted(0, 0, 0, -plot.height() / 2 + 24), Qt::AlignCenter,
            !message_.isEmpty() ? message_
            : traces_.empty()   ? tr("No channels. Use the Probe tool (P) on the schematic to add one.")
                                : tr("No data yet. Run the simulation (F5)."));
    }
    if (!display_running_) {
        painter.setPen(theme::muted_text);
        painter.drawText(plot.adjusted(8, 6, -8, 0), Qt::AlignTop | Qt::AlignRight, tr("Display stopped"));
    }
}

void ScopeWidget::wheelEvent(QWheelEvent* event)
{
    if (!navigation_) {
        QWidget::wheelEvent(event);
        return;
    }
    const double steps = event->angleDelta().y() / 120.0;
    const double factor = std::pow(0.8, steps);
    const double at = time_at(event->position().x());
    const double start = at - (at - start_) * factor;
    const double end = at + (end_ - at) * factor;
    if (end - start > 1e-12) {
        set_window(start, end);
        emit window_changed(start_, end_);
    }
    event->accept();
}

void ScopeWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const double x = event->position().x();
    if (cursors_) {
        for (int i = 0; i < 2; ++i) {
            if (std::abs(x - x_at(i == 0 ? cursor_a_ : cursor_b_)) < 6) {
                dragging_cursor_ = i;
                return;
            }
        }
    }
    if (navigation_) {
        panning_ = true;
        pan_origin_x_ = x;
        pan_origin_start_ = start_;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ScopeWidget::mouseMoveEvent(QMouseEvent* event)
{
    const double x = event->position().x();
    if (dragging_cursor_ >= 0) {
        set_cursor(dragging_cursor_, std::clamp(time_at(x), start_, end_));
        return;
    }
    if (panning_) {
        const double span = end_ - start_;
        const double shift = (pan_origin_x_ - x) / plot_rect().width() * span;
        set_window(pan_origin_start_ + shift, pan_origin_start_ + shift + span);
        emit window_changed(start_, end_);
        return;
    }
    bool near = false;
    if (cursors_) {
        near = std::abs(x - x_at(cursor_a_)) < 6 || std::abs(x - x_at(cursor_b_)) < 6;
    }
    setCursor(near ? Qt::SizeHorCursor : (navigation_ ? Qt::OpenHandCursor : Qt::ArrowCursor));
}

void ScopeWidget::mouseReleaseEvent(QMouseEvent*)
{
    dragging_cursor_ = -1;
    if (panning_) {
        panning_ = false;
        setCursor(Qt::OpenHandCursor);
    }
}

} // namespace simnodus::ui
