// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Bottom panel (UI-12): diagnostics and engine messages; double-clicking a
// message selects and centers the related items.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "model/schematic.hpp"
#endif

#include <QWidget>

#include <vector>

class QTreeWidget;

namespace simnodus::ui {

enum class Severity { Info, Warning, Error };

class MessagesPanel : public QWidget {
    Q_OBJECT
public:
    explicit MessagesPanel(QWidget* parent = nullptr);

    // `items` are selected and centered when the user double-clicks the message.
    void add(Severity severity, const QString& text, const QString& source = {}, std::vector<model::Id> items = {});
    // Removes the messages of one source (for example the last pre-run check).
    void clear_source(const QString& source);
    void clear();
    int count() const;
    // The messages as text lines ("12:00:01 Warning: text (source)"), all or
    // only the selected ones; what Copy puts on the clipboard.
    QString text(bool selected_only) const;
    int warning_count() const { return warnings_; }
    int error_count() const { return errors_; }

signals:
    void counts_changed(int warnings, int errors);
    void error_added();
    void items_activated(const std::vector<model::Id>& items);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTreeWidget* list_ = nullptr;
    int warnings_ = 0;
    int errors_ = 0;
};

} // namespace simnodus::ui
