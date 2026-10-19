// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Circuit Editor main window (REQUIREMENTS section 2, DESKTOP_UX): component
// tree | schematic | properties in a splitter, a collapsible messages panel,
// a status bar and the separate Signal Analyzer window. It owns the
// document, the component library and the run controller.
#pragma once

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/component_library.hpp"

#include <optional>
#endif

#include <QMainWindow>
#include <QPointer>
#include <QStringList>

#include <array>
#include <memory>

class QComboBox;
class QLabel;
class QMenu;
class QSettings;
class QSplitter;
class QThread;
class QTimer;

namespace simnodus::ui {

class AnalyzerWindow;
class ComponentPanel;
class Document;
class MessagesPanel;
class PropertiesPanel;
class RunController;
class SchematicScene;
class SchematicView;

enum class Panel { Components = 0, Properties = 1, Messages = 2 };

// One line of Help > Keyboard Shortcuts.
struct ShortcutRow {
    QString group;
    QString keys;
    QString action;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QSettings& settings, QWidget* parent = nullptr);
    ~MainWindow() override;

    ComponentPanel* components() const { return components_; }
    PropertiesPanel* properties() const { return properties_; }
    MessagesPanel* messages() const { return messages_; }
    SchematicView* schematic() const { return view_; }
    SchematicScene* scene() const { return scene_; }
    Document* document() const { return document_; }
    RunController* run_controller() const { return run_; }
    AnalyzerWindow* analyzer() const { return analyzer_.get(); }
    const library::ComponentLibrary& library() const { return library_; }
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
    // Menu shortcuts (read from the actions, so the list cannot go stale)
    // followed by the canvas keys and mouse gestures.
    QList<ShortcutRow> shortcut_rows() const;

    // File operations; return false if cancelled or failed.
    bool open_file(const QString& path);
    bool save_to(const QString& path);
    // Blocks (PRJ-06): copies the block file into the project (replacing a copy
    // of the same name) and returns the block's type, or empty with `error`.
    QString add_block_file(const QString& path, QString& error);
    // The block files that differ from the project's copies, by block name.
    QStringList changed_block_files() const;
    // Model folders (PRJ-07): saved in the settings and used from now on.
    void set_model_folders(const QStringList& folders);
    // For tests: the unsaved-changes prompt answers "discard" automatically.
    void set_confirm_discard(bool automatic) { auto_discard_ = automatic; }

    // Crash recovery (NF-04): the modified document is written to a recovery
    // file every minute; a clean exit removes it.
    QString recovery_file() const;
    bool write_recovery();
    // Offers to restore a recovery file left by a session that did not exit
    // cleanly; `automatic` restores without asking (tests). Returns true if restored.
    bool check_recovery(bool automatic = false);

public slots:
    void toggle_panel(Panel panel);
    void set_focus_canvas(bool enabled);
    void show_analyzer();
    void check_engine();
    void show_about();
    void show_shortcuts();
    void show_user_guide();
    void new_file();
    void open_dialog();
    bool save();
    bool save_as();
    void run();
    void pause_resume();
    void stop();
    void reset();
    void edit_settings();

signals:
    void engine_check_finished(bool ok);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void load_library();
    void build_actions_and_menus();
    void build_status_bar();
    QWidget* panel_widget(Panel panel) const;
    QSplitter* panel_splitter(Panel panel) const;
    void sync_panel_actions();
    void update_counts(int warnings, int errors);
    void update_title();
    void update_run_state();
    void update_status();
    bool confirm_discard();
    bool allow_structure_edit();
    void add_recent(const QString& path);
    void rebuild_recent_menu();
    void rebuild_examples_menu();
    void ask_label_name(int x, int y);
    void ask_note_text(model::Point at, model::Id note);
    void ask_bus_name(std::vector<model::Point> points);
    void ask_splitter(model::Point at);
    QString bus_name_at(const model::Point& point) const;
    // Problems of the user's models folder in Messages, once per read (PRJ-07).
    void report_model_issues();
    void edit_model_folders();
    void report_diagnostics();
    void place_block();
    void open_block(model::Id component);
    void update_block(model::Id component);
    void offer_block_updates();
    // A block's file on disk: its source resolved against the project's folder.
    QString block_path(const model::Block& block) const;
    std::optional<model::Block> read_block(const QString& path, QString& error) const;

    QSettings& settings_;
    library::ComponentLibrary library_;
    Document* document_ = nullptr;
    SchematicScene* scene_ = nullptr;
    SchematicView* view_ = nullptr;
    ComponentPanel* components_ = nullptr;
    PropertiesPanel* properties_ = nullptr;
    MessagesPanel* messages_ = nullptr;
    QSplitter* columns_ = nullptr;
    QSplitter* rows_ = nullptr;
    std::unique_ptr<AnalyzerWindow> analyzer_;
    RunController* run_ = nullptr;

    std::array<QAction*, 3> panel_actions_{};
    QAction* focus_action_ = nullptr;
    QAction* analyzer_action_ = nullptr;
    QAction* engine_action_ = nullptr;
    QAction* run_action_ = nullptr;
    QAction* pause_action_ = nullptr;
    QAction* stop_action_ = nullptr;
    QAction* reset_action_ = nullptr;
    QAction* debug_action_ = nullptr;
    static constexpr int default_gdb_port = 3333; // OpenOCD's default, which CubeIDE's dialog offers
    QComboBox* speed_ = nullptr;
    QMenu* recent_menu_ = nullptr;
    QMenu* examples_menu_ = nullptr;
    std::array<int, 3> remembered_{260, 280, 160};
    bool focus_mode_ = false;
    std::array<bool, 3> before_focus_{true, true, false};
    bool auto_discard_ = false;
    QString last_label_;
    QString last_bus_ = QStringLiteral("D");
    int reported_models_generation_ = -1;
    QTimer* recovery_timer_ = nullptr;

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
