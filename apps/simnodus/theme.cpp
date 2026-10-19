// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "theme.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QWidget>
#include <QLabel>
#include <QPalette>
#include <QStyleFactory>

namespace simnodus::ui::theme {

namespace {

bool dark_now = false;

struct Colors {
    QColor canvas, grid_minor, grid_major, ink, selection, muted, bus, plot, plot_grid, plot_axis, readout, label;
    QColor window, base, alternate, button, tooltip, disabled, splitter;
};

const Colors light_colors{
    {0xfc, 0xfc, 0xfb}, {0xd4, 0xd6, 0xd9}, {0xa9, 0xad, 0xb3}, {0x22, 0x24, 0x28}, {0x1f, 0x6f, 0xd1},
    {0x6b, 0x70, 0x78}, {0x2b, 0x4f, 0x7e}, {0xff, 0xff, 0xff}, {0xe3, 0xe5, 0xe8}, {0xb9, 0xbd, 0xc3},
    {0xee, 0xf4, 0xff}, {0xff, 0xff, 0xff, 230},
    {0xf3, 0xf4, 0xf6}, {0xff, 0xff, 0xff}, {0xf7, 0xf8, 0xf9}, {0xf3, 0xf4, 0xf6}, {0xff, 0xff, 0xf0},
    {0xa0, 0xa4, 0xaa}, {0xe2, 0xe4, 0xe8}};

// Dark: the same meanings on dark grey; lines and text light, accents brighter.
const Colors dark_colors{
    {0x1e, 0x1f, 0x22}, {0x3a, 0x3d, 0x42}, {0x56, 0x5a, 0x61}, {0xdc, 0xde, 0xe2}, {0x4c, 0x9b, 0xff},
    {0x9a, 0xa0, 0xa8}, {0x7f, 0xa6, 0xd8}, {0x17, 0x18, 0x1a}, {0x2e, 0x31, 0x35}, {0x4a, 0x4e, 0x54},
    {0x23, 0x32, 0x4a}, {0x1e, 0x1f, 0x22, 230},
    {0x2b, 0x2d, 0x30}, {0x1f, 0x20, 0x23}, {0x26, 0x28, 0x2b}, {0x32, 0x34, 0x38}, {0x32, 0x34, 0x38},
    {0x6e, 0x72, 0x78}, {0x3a, 0x3c, 0x40}};

} // namespace

Scheme scheme_from(const QString& setting)
{
    if (setting == QStringLiteral("light")) {
        return Scheme::Light;
    }
    if (setting == QStringLiteral("dark")) {
        return Scheme::Dark;
    }
    return Scheme::System;
}

QString scheme_setting(Scheme scheme)
{
    switch (scheme) {
    case Scheme::Light:
        return QStringLiteral("light");
    case Scheme::Dark:
        return QStringLiteral("dark");
    case Scheme::System:
        break;
    }
    return {};
}

bool dark()
{
    return dark_now;
}

void apply(QApplication& application, Scheme scheme)
{
    bool use_dark = scheme == Scheme::Dark;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (scheme == Scheme::System) {
        use_dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif
    dark_now = use_dark;
    const Colors& c = use_dark ? dark_colors : light_colors;
    canvas_background = c.canvas;
    grid_minor = c.grid_minor;
    grid_major = c.grid_major;
    ink = c.ink;
    selection = c.selection;
    muted_text = c.muted;
    bus = c.bus;
    plot_background = c.plot;
    plot_grid = c.plot_grid;
    plot_axis = c.plot_axis;
    readout_background = c.readout;
    label_background = c.label;

    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette;
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, ink);
    palette.setColor(QPalette::Base, c.base);
    palette.setColor(QPalette::AlternateBase, c.alternate);
    palette.setColor(QPalette::Text, ink);
    palette.setColor(QPalette::Button, c.button);
    palette.setColor(QPalette::ButtonText, ink);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, selection);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, c.tooltip);
    palette.setColor(QPalette::ToolTipText, ink);
    palette.setColor(QPalette::PlaceholderText, muted_text);
    palette.setColor(QPalette::Link, selection);
    palette.setColor(QPalette::Disabled, QPalette::Text, c.disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, c.disabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, c.disabled);
    application.setPalette(palette);
    application.setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: %1; }"
        "QToolBar { border: none; spacing: 2px; padding: 2px; }"
        "QStatusBar QLabel { padding: 0 8px; }")
                                  .arg(c.splitter.name()));
    // Widgets that paint with the theme colors repaint now.
    for (auto* widget : QApplication::allWidgets()) {
        widget->update();
    }
}

QLabel* make_caption(const QString& text)
{
    auto* label = new QLabel(text.toUpper());
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 0.85);
    font.setLetterSpacing(QFont::PercentageSpacing, 108);
    label->setFont(font);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, muted_text);
    label->setPalette(palette);
    label->setContentsMargins(6, 6, 6, 2);
    return label;
}

} // namespace simnodus::ui::theme
