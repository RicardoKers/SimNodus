// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Light technical theme (UI-03): near-white canvas, thin dark lines, color
// used only for meaning. A dark theme is a later option (SN-162).
#pragma once

#include <QColor>

class QApplication;
class QLabel;

namespace simnodus::ui::theme {

inline const QColor canvas_background{0xfc, 0xfc, 0xfb};
inline const QColor grid_minor{0xd4, 0xd6, 0xd9};
inline const QColor grid_major{0xa9, 0xad, 0xb3};
inline const QColor ink{0x22, 0x24, 0x28};
inline const QColor selection{0x1f, 0x6f, 0xd1};
inline const QColor muted_text{0x6b, 0x70, 0x78};
// Microcontroller pin levels on the board symbol (INT-06).
inline const QColor pin_high{0xe0, 0x3a, 0x2f};
inline const QColor pin_low{0x2f, 0x5f, 0xd0};
inline const QColor plot_background{0xff, 0xff, 0xff};
inline const QColor plot_grid{0xe3, 0xe5, 0xe8};
inline const QColor plot_axis{0xb9, 0xbd, 0xc3};

// Fusion style with a fixed light palette, independent of the OS color scheme.
void apply(QApplication& application);

// Small upper-case caption used for panel headers ("COMPONENTS").
QLabel* make_caption(const QString& text);

} // namespace simnodus::ui::theme
