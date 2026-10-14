// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Left column (UI-04): searchable component tree and a preview pane. The
// phase 0 catalogue is a fixed list of component set A; SN-111 replaces it
// with the data-driven library.
#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace simnodus::ui {

class ComponentPanel : public QWidget {
    Q_OBJECT
public:
    explicit ComponentPanel(QWidget* parent = nullptr);

    QLineEdit* search_field() const { return search_; }
    QTreeWidget* tree() const { return tree_; }
    // Number of component entries that match the current filter.
    int visible_component_count() const;
    QString selected_type() const;

signals:
    void placement_requested(const QString& type, const QString& name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void populate();
    void apply_filter(const QString& text);
    void update_preview();

    QLineEdit* search_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* preview_name_ = nullptr;
    QLabel* preview_details_ = nullptr;
};

} // namespace simnodus::ui
