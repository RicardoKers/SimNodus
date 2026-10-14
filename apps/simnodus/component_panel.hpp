// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Left column (UI-04): searchable component tree built from the component
// library, with a preview of the selected symbol. Enter or double-click
// starts placement; dragging onto the canvas places directly.
#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace simnodus::library {
class ComponentLibrary;
}

namespace simnodus::ui {

class SymbolPreview;

class ComponentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ComponentPanel(const library::ComponentLibrary& library, QWidget* parent = nullptr);

    QLineEdit* search_field() const { return search_; }
    QTreeWidget* tree() const { return tree_; }
    // Number of component entries that match the current filter.
    int visible_component_count() const;
    QString selected_type() const;

signals:
    void placement_requested(const QString& type);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void populate();
    void apply_filter(const QString& text);
    void update_preview();

    const library::ComponentLibrary& library_;
    QLineEdit* search_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    SymbolPreview* preview_symbol_ = nullptr;
    QLabel* preview_name_ = nullptr;
    QLabel* preview_details_ = nullptr;
};

} // namespace simnodus::ui
