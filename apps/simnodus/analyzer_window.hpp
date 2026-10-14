// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Signal Analyzer (AN-01): an independent top-level window that can live on
// another monitor. Closing it only hides it; acquisition is not affected.
#pragma once

#include <QMainWindow>

class QAction;
class QComboBox;
class QSettings;
class QTabWidget;
class QTreeWidget;

namespace simnodus::ui {

class ScopeWidget;

class AnalyzerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit AnalyzerWindow(QWidget* parent = nullptr);

    ScopeWidget* scope() const { return scope_; }
    QTabWidget* tabs() const { return tabs_; }
    QComboBox* timebase() const { return timebase_; }

    void save_settings(QSettings& settings) const;
    void restore_settings(const QSettings& settings);

signals:
    void visibility_changed(bool visible);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* build_controls();

    ScopeWidget* scope_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QComboBox* timebase_ = nullptr;
    QTreeWidget* channels_ = nullptr;
    QAction* display_run_ = nullptr;
};

} // namespace simnodus::ui
