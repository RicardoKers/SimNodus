// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// What a viewer sees of the hand in recorded tutorials (SN-238): a drawn
// pointer that glides to a target and shows clicks and drags, a key badge, an
// outline around a widget or a part of the canvas, and the screen position of
// targets. Targets are grid points, pins and parts of the canvas, and the
// window's widgets by name (a toolbar button, a Components entry, a
// Properties field, a menu, an analyzer tab). The pointer and the outline are
// small windows of their own, above every window and transparent for input,
// so that a screen recording shows them over menus and the Signal Analyzer
// too. The ui.* commands of external control use them while they act through
// the real widgets (ControlServer).
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "automation/automation.hpp"
#endif

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>

class QVariantAnimation;
class QWidget;

namespace simnodus::ui {

class MainWindow;
class PointerOverlay;
class HighlightOverlay;

class Presenter : public QObject {
    Q_OBJECT
public:
    explicit Presenter(MainWindow& window);
    ~Presenter() override;

    // A place a command names: the widget that holds it, its rectangle and the
    // point a click goes to, in that widget's coordinates.
    struct Target {
        QPointer<QWidget> widget;
        QRect rect;
        QPoint point;
        bool canvas = false; // on the schematic (grid point, pin, part, area)
    };
    // The target of a request (see AUTOMATION.md); throws automation::Error.
    // `scroll`: a canvas target off the screen is scrolled into view first.
    Target resolve(const automation::json& target, bool scroll = false) const;
    static QRect global_rect(const Target& target);
    static QPoint global_point(const Target& target);
    // Physical pixels of the screen (Windows scales logical pixels, 125 % on the owner's laptop).
    static QRect physical(const QRect& global);

    // The pointer glides to `global` in `ms` milliseconds; while it moves,
    // `drive` (if any) receives mouse moves with `held` buttons, as from a hand.
    void glide(QPoint global, int ms, QWidget* drive = nullptr, Qt::MouseButtons held = Qt::NoButton);
    void show_click(int count);  // ripples at the pointer
    void set_pressed(bool pressed); // a drag holds the button
    void hide_pointer();
    bool pointer_visible() const;
    QPoint pointer_position() const;

    // An outline around `global`, pulsing; `ms` 0 keeps it until replaced or hidden.
    void highlight(const QRect& global, int ms);
    void hide_highlight();
    bool highlight_visible() const;

private:
    QPoint start_point() const;

    MainWindow& window_;
    PointerOverlay* pointer_ = nullptr;
    HighlightOverlay* outline_ = nullptr;
    QVariantAnimation* motion_ = nullptr;
};

} // namespace simnodus::ui
