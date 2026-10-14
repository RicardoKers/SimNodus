// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Bottom panel (UI-12): diagnostics and engine messages; double-clicking a
// message selects and centers the related items.
#pragma once

#include "model/schematic.hpp"

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
    int warning_count() const { return warnings_; }
    int error_count() const { return errors_; }

signals:
    void counts_changed(int warnings, int errors);
    void error_added();
    void items_activated(const std::vector<model::Id>& items);

private:
    QTreeWidget* list_ = nullptr;
    int warnings_ = 0;
    int errors_ = 0;
};

} // namespace simnodus::ui
