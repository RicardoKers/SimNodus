// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "logic_widget.hpp"

#include "theme.hpp"

#ifndef Q_MOC_RUN
#include "model/engineering.hpp"
#endif

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace simnodus::ui {

namespace {

constexpr double label_width = 96.0;
constexpr double lane_height = 26.0;
constexpr double axis_height = 20.0;
constexpr int divisions = 10;

QString seconds_text(double t)
{
    return QString::fromStdString(model::format_engineering(t, 6)) + QStringLiteral("s");
}

} // namespace

LogicWidget::LogicWidget(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAutoFillBackground(true);
}

void LogicWidget::set_lanes(std::vector<LogicLaneView> lanes)
{
    lanes_ = std::move(lanes);
    if (selected_ >= static_cast<int>(lanes_.size())) {
        selected_ = -1;
    }
    setMinimumHeight(static_cast<int>(axis_height + lane_height * static_cast<double>(std::max<std::size_t>(lanes_.size(), 1)) + 8));
    update();
}

void LogicWidget::set_window(double start, double end)
{
    if (end <= start) {
        end = start + 1e-6;
    }
    start_ = start;
    end_ = end;
    update();
}

void LogicWidget::set_cursors(bool enabled, double a, double b)
{
    cursors_ = enabled;
    cursor_a_ = a;
    cursor_b_ = b;
    update();
}

void LogicWidget::set_selected(int lane)
{
    selected_ = lane;
    update();
    emit lane_selected(lane);
}

std::vector<std::pair<double, bool>> LogicWidget::visible_transitions(int lane) const
{
    std::vector<std::pair<double, bool>> result;
    if (lane < 0 || lane >= static_cast<int>(lanes_.size())) {
        return result;
    }
    for (const auto& change : lanes_[static_cast<std::size_t>(lane)].changes) {
        if (change.first >= start_ && change.first <= end_) {
            result.push_back(change);
        }
    }
    return result;
}

QRectF LogicWidget::plot_rect() const
{
    return QRectF(label_width, 4, std::max(10.0, width() - label_width - 8), std::max(10.0, height() - axis_height - 8));
}

double LogicWidget::x_at(double time) const
{
    const QRectF plot = plot_rect();
    return plot.left() + (time - start_) / (end_ - start_) * plot.width();
}

double LogicWidget::time_at(double x) const
{
    const QRectF plot = plot_rect();
    return start_ + (x - plot.left()) / plot.width() * (end_ - start_);
}

int LogicWidget::lane_at(double y) const
{
    const int lane = static_cast<int>(std::floor((y - plot_rect().top()) / lane_height));
    return lane >= 0 && lane < static_cast<int>(lanes_.size()) ? lane : -1;
}

void LogicWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), theme::plot_background);
    const QRectF plot = plot_rect();
    painter.setPen(QPen(theme::plot_grid, 1));
    for (int i = 0; i <= divisions; ++i) {
        const double x = plot.left() + plot.width() * i / divisions;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    if (lanes_.empty()) {
        painter.setPen(theme::muted_text);
        painter.drawText(plot, Qt::AlignCenter,
            tr("Run a circuit with a microcontroller or voltage probes; choose the signals on the left."));
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (std::size_t i = 0; i < lanes_.size(); ++i) {
        const auto& lane = lanes_[i];
        const double top = plot.top() + lane_height * static_cast<double>(i);
        const QRectF row(0, top, width(), lane_height);
        if (static_cast<int>(i) == selected_) {
            painter.fillRect(row, QColor(0xee, 0xf4, 0xff));
        }
        painter.setPen(theme::ink);
        painter.drawText(QRectF(4, top, label_width - 8, lane_height), Qt::AlignVCenter | Qt::AlignLeft, lane.title);
        painter.setPen(QPen(theme::plot_grid, 1));
        painter.drawLine(QPointF(plot.left(), top + lane_height), QPointF(plot.right(), top + lane_height));
        const double high = top + 5;
        const double low = top + lane_height - 5;
        QPolygonF wave;
        bool level = lane.initial;
        double x = plot.left();
        wave << QPointF(x, level ? high : low);
        for (const auto& [t, next] : lane.changes) {
            if (t < start_) {
                level = next;
                wave.back().setY(level ? high : low);
                continue;
            }
            if (t > end_) {
                break;
            }
            x = std::clamp(x_at(t), plot.left(), plot.right());
            wave << QPointF(x, level ? high : low) << QPointF(x, next ? high : low);
            level = next;
        }
        wave << QPointF(plot.right(), level ? high : low);
        painter.setPen(QPen(lane.color, 1.6));
        painter.drawPolyline(wave);
    }
    painter.setPen(theme::muted_text);
    for (int i = 0; i <= divisions; i += 2) {
        const double t = start_ + (end_ - start_) * i / divisions;
        const double x = plot.left() + plot.width() * i / divisions;
        painter.drawText(QRectF(x - 50, plot.bottom() + 2, 100, axis_height), Qt::AlignHCenter | Qt::AlignTop,
            QString::fromStdString(model::format_engineering(t, 4)) + QStringLiteral("s"));
    }
    if (cursors_) {
        for (const double c : {cursor_a_, cursor_b_}) {
            if (c >= start_ && c <= end_) {
                painter.setPen(QPen(theme::selection, 1, Qt::DashLine));
                painter.drawLine(QPointF(x_at(c), plot.top()), QPointF(x_at(c), plot.bottom()));
            }
        }
    }
}

void LogicWidget::mousePressEvent(QMouseEvent* event)
{
    set_selected(lane_at(event->position().y()));
    QWidget::mousePressEvent(event);
}

bool LogicWidget::event(QEvent* event)
{
    if (event->type() != QEvent::ToolTip) {
        return QWidget::event(event);
    }
    auto* help = static_cast<QHelpEvent*>(event);
    const int lane = lane_at(help->pos().y());
    if (lane < 0 || help->pos().x() < plot_rect().left()) {
        QToolTip::hideText();
        return true;
    }
    // Nearest transition within 6 pixels.
    const auto& view = lanes_[static_cast<std::size_t>(lane)];
    const double mouse = help->pos().x();
    std::optional<std::pair<double, bool>> best;
    for (const auto& change : view.changes) {
        if (change.first < start_ || change.first > end_) {
            continue;
        }
        if (std::abs(x_at(change.first) - mouse) <= 6
            && (!best || std::abs(x_at(change.first) - mouse) < std::abs(x_at(best->first) - mouse))) {
            best = change;
        }
    }
    const QString text = best ? tr("%1 %2 at %3").arg(view.title, best->second ? tr("rises") : tr("falls"),
                                    seconds_text(best->first))
                              : tr("%1 at %2").arg(view.title, seconds_text(time_at(mouse)));
    QToolTip::showText(help->globalPos(), text, this);
    return true;
}

} // namespace simnodus::ui
