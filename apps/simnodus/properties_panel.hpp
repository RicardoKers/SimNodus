// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Right column (UI-10, INT-04): parameters of the selected item and, while
// running, its live values. Editing arrives with SN-114.
#pragma once

#include <QWidget>

class QGroupBox;
class QLabel;
class QTableWidget;

namespace simnodus::ui {

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    // Header shown for a component type chosen in the tree (no instance yet).
    void show_type(const QString& name);
    void clear_selection();
    QString header_text() const;

private:
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QTableWidget* parameters_ = nullptr;
    QLabel* live_ = nullptr;
};

} // namespace simnodus::ui
