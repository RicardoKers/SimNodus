// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "scope_widget.hpp"

#include "theme.hpp"

#include <QPainter>

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
            return QStringLiteral("%1 %2").arg(QString::number(seconds / unit.scale, 'g', 4), QString::fromUtf8(unit.suffix));
        }
    }
    return QStringLiteral("%1 ns").arg(QString::number(seconds / 1e-9, 'g', 4));
}

ScopeWidget::ScopeWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("scopeWidget"));
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void ScopeWidget::set_seconds_per_division(double value)
{
    if (value > 0.0 && value != seconds_per_division_) {
        seconds_per_division_ = value;
        update();
    }
}

void ScopeWidget::set_display_running(bool running)
{
    display_running_ = running;
    update();
}

QRectF ScopeWidget::plot_rect() const
{
    const QFontMetrics metrics(font());
    const double left = 12.0;
    const double bottom = metrics.height() + 10.0;
    return QRectF(left, 10.0, std::max(10.0, width() - left - 12.0), std::max(10.0, height() - 10.0 - bottom));
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
    const double center_y = plot.center().y();
    painter.drawLine(QPointF(plot.left(), center_y), QPointF(plot.right(), center_y));

    painter.setPen(theme::muted_text);
    const QFontMetrics metrics(font());
    for (int i = 0; i <= horizontal_divisions; i += 2) {
        const double x = plot.left() + plot.width() * i / horizontal_divisions;
        const QString label = format_seconds(seconds_per_division_ * i);
        const double w = metrics.horizontalAdvance(label);
        const double left = std::clamp(x - w / 2, 0.0, width() - w);
        painter.drawText(QPointF(left, plot.bottom() + metrics.ascent() + 4), label);
    }
    painter.drawText(plot.adjusted(0, 0, 0, -plot.height() / 2 + 24), Qt::AlignCenter,
        tr("No channels. Use the Probe tool (P) on the schematic to add one."));
    if (!display_running_) {
        painter.drawText(plot.adjusted(8, 6, -8, 0), Qt::AlignTop | Qt::AlignRight, tr("Display stopped"));
    }
}

} // namespace simnodus::ui
