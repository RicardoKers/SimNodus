// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "theme.hpp"

#include <QApplication>
#include <QLabel>
#include <QPalette>
#include <QStyleFactory>

namespace simnodus::ui::theme {

void apply(QApplication& application)
{
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0xf3, 0xf4, 0xf6));
    palette.setColor(QPalette::WindowText, ink);
    palette.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf8, 0xf9));
    palette.setColor(QPalette::Text, ink);
    palette.setColor(QPalette::Button, QColor(0xf3, 0xf4, 0xf6));
    palette.setColor(QPalette::ButtonText, ink);
    palette.setColor(QPalette::Highlight, selection);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xf0));
    palette.setColor(QPalette::ToolTipText, ink);
    palette.setColor(QPalette::PlaceholderText, muted_text);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0xa0, 0xa4, 0xaa));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0xa0, 0xa4, 0xaa));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0xa0, 0xa4, 0xaa));
    application.setPalette(palette);
    application.setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #e2e4e8; }"
        "QToolBar { border: none; spacing: 2px; padding: 2px; }"
        "QStatusBar QLabel { padding: 0 8px; }"));
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
