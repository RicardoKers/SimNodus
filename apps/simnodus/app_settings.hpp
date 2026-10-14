// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Persistent settings (UI-02): an INI file in the user's configuration
// folder, or an explicit file given on the command line (tests).
#pragma once

#include <QByteArray>
#include <QRect>
#include <QString>

#include <memory>

class QSettings;
class QWidget;

namespace simnodus::ui {

std::unique_ptr<QSettings> open_settings(const QString& explicit_file = {});

// Restores a saved geometry and makes sure the window ends up on a monitor
// that exists now; otherwise it is centered on the primary screen.
// Returns false when nothing usable was saved.
bool restore_window_geometry(QWidget& window, const QByteArray& geometry);

// True if at least `min_visible` pixels of each side of `frame` lie on one
// of the available screens.
bool is_on_available_screen(const QRect& frame, int min_visible = 80);

} // namespace simnodus::ui
