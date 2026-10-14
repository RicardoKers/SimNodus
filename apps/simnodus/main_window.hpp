// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Circuit Editor main window (REQUIREMENTS section 2, DESKTOP_UX): component
// tree | schematic | properties in a splitter, a collapsible messages panel,
// a status bar and the separate Signal Analyzer window.
#pragma once

#include <QMainWindow>
#include <QPointer>

#include <array>
#include <memory>

class QLabel;
class QSettings;
class QSplitter;
class QThread;
class QUndoStack;

namespace simnodus::ui {

class AnalyzerWindow;
class ComponentPanel;
class MessagesPanel;
class PropertiesPanel;
class SchematicView;

enum class Panel { Components = 0, Properties = 1, Messages = 2 };

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QSettings& settings, QWidget* parent = nullptr);
    ~MainWindow() override;

    ComponentPanel* components() const { return components_; }
    PropertiesPanel* properties() const { return properties_; }
    MessagesPanel* messages() const { return messages_; }
    SchematicView* schematic() const { return view_; }
    AnalyzerWindow* analyzer() const { return analyzer_.get(); }
    QSplitter* columns() const { return columns_; }
    QSplitter* rows() const { return rows_; }

    bool panel_visible(Panel panel) const;
    void set_panel_visible(Panel panel, bool visible);
    // Width (side panels) or height (messages) the panel had when last visible.
    int remembered_size(Panel panel) const { return remembered_[static_cast<int>(panel)]; }
    bool focus_canvas_active() const { return focus_mode_; }

    void save_settings();
    void restore_settings();
    // Reopens secondary windows that were open at the end of the last session.
    void show_restored_windows();
    bool engine_check_running() const;

public slots:
    void toggle_panel(Panel panel);
    void set_focus_canvas(bool enabled);
    void show_analyzer();
    void check_engine();
    void show_about();

signals:
    void engine_check_finished(bool ok);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void build_actions_and_menus();
    void build_status_bar();
    QWidget* panel_widget(Panel panel) const;
    QSplitter* panel_splitter(Panel panel) const;
    void sync_panel_actions();
    void update_counts(int warnings, int errors);

    QSettings& settings_;
    SchematicView* view_ = nullptr;
    ComponentPanel* components_ = nullptr;
    PropertiesPanel* properties_ = nullptr;
    MessagesPanel* messages_ = nullptr;
    QSplitter* columns_ = nullptr;
    QSplitter* rows_ = nullptr;
    std::unique_ptr<AnalyzerWindow> analyzer_;
    QUndoStack* undo_stack_ = nullptr;

    std::array<QAction*, 3> panel_actions_{};
    QAction* focus_action_ = nullptr;
    QAction* analyzer_action_ = nullptr;
    QAction* engine_action_ = nullptr;
    std::array<int, 3> remembered_{260, 280, 160};
    bool focus_mode_ = false;
    std::array<bool, 3> before_focus_{true, true, false};

    QLabel* state_label_ = nullptr;
    QLabel* time_label_ = nullptr;
    QLabel* speed_label_ = nullptr;
    QLabel* mode_label_ = nullptr;
    QLabel* warnings_label_ = nullptr;
    QLabel* cursor_label_ = nullptr;
    QLabel* zoom_label_ = nullptr;

    QPointer<QThread> engine_thread_;
};

} // namespace simnodus::ui
