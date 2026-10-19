// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Technical theme (UI-03): a plain canvas, thin lines, color used only for
// meaning. Light or dark (SN-162): apply() sets these colors, which the
// painting code reads every time it draws.
#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QLabel;

namespace simnodus::ui::theme {

inline QColor canvas_background{0xfc, 0xfc, 0xfb};
inline QColor grid_minor{0xd4, 0xd6, 0xd9};
inline QColor grid_major{0xa9, 0xad, 0xb3};
inline QColor ink{0x22, 0x24, 0x28};
inline QColor selection{0x1f, 0x6f, 0xd1};
inline QColor muted_text{0x6b, 0x70, 0x78};
inline QColor bus{0x2b, 0x4f, 0x7e}; // buses and splitters (UI-19)
// Microcontroller pin levels on the board symbol (INT-06).
inline QColor pin_high{0xe0, 0x3a, 0x2f};
inline QColor pin_low{0x2f, 0x5f, 0xd0};
inline QColor plot_background{0xff, 0xff, 0xff};
inline QColor plot_grid{0xe3, 0xe5, 0xe8};
inline QColor plot_axis{0xb9, 0xbd, 0xc3};
inline QColor readout_background{0xee, 0xf4, 0xff}; // live values on the schematic
inline QColor label_background{0xff, 0xff, 0xff, 230}; // net label boxes

// "system" follows the operating system's light or dark setting (Qt 6.5 and
// later; light before), "light" and "dark" fix it.
enum class Scheme { System, Light, Dark };
Scheme scheme_from(const QString& setting);
QString scheme_setting(Scheme scheme);

// Fusion style with the scheme's palette and colors. It can run again while
// the application shows windows; they repaint with the new colors.
void apply(QApplication& application, Scheme scheme = Scheme::Light);
bool dark();

// Small upper-case caption used for panel headers ("COMPONENTS").
QLabel* make_caption(const QString& text);

} // namespace simnodus::ui::theme
