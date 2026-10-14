// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "app_settings.hpp"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QWidget>

#include <algorithm>

namespace simnodus::ui {

std::unique_ptr<QSettings> open_settings(const QString& explicit_file)
{
    if (!explicit_file.isEmpty()) {
        return std::make_unique<QSettings>(explicit_file, QSettings::IniFormat);
    }
    return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
        QStringLiteral("SimNodus"), QStringLiteral("SimNodus"));
}

bool is_on_available_screen(const QRect& frame, int min_visible)
{
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QRect visible = screen->availableGeometry().intersected(frame);
        if (visible.width() >= std::min(min_visible, frame.width())
            && visible.height() >= std::min(min_visible, frame.height())) {
            return true;
        }
    }
    return false;
}

bool restore_window_geometry(QWidget& window, const QByteArray& geometry)
{
    const bool restored = !geometry.isEmpty() && window.restoreGeometry(geometry);
    if (restored && is_on_available_screen(window.frameGeometry())) {
        return true;
    }
    // Saved on a monitor that is gone (or nothing saved): center on primary.
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        QSize size = window.size().boundedTo(available.size() * 0.9);
        if (!restored) {
            size = window.sizeHint().expandedTo(window.size()).boundedTo(available.size() * 0.9);
        }
        window.resize(size);
        window.move(available.center() - QPoint(size.width() / 2, size.height() / 2));
    }
    return restored;
}

} // namespace simnodus::ui
