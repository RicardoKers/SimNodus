// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "main_window.hpp"

#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "component_panel.hpp"
#include "document.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "run_controller.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"
#include "settings_dialog.hpp"
#include "theme.hpp"
#include "user_models.hpp"

#include "editing/operations.hpp"
#include "io/project_file.hpp"
#include "library/blocks.hpp"
#include "model/engineering.hpp"
#include "sim/engine_check.hpp"
#include "sim/engine_host.hpp"
#include "simnodus/version.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QFormLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QTableWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>

#include <algorithm>
#include <numeric>

namespace simnodus::ui {
namespace {

QString panel_key(Panel panel)
{
    switch (panel) {
    case Panel::Components:
        return QStringLiteral("components");
    case Panel::Properties:
        return QStringLiteral("properties");
    case Panel::Messages:
        break;
    }
    return QStringLiteral("messages");
}

constexpr int center_minimum = 200;
constexpr int max_recent = 8;
const QString project_filter = QStringLiteral("SimNodus projects (*.snx)");

QStringList examples_folders()
{
    const QString app = QCoreApplication::applicationDirPath();
    QStringList folders{app + QStringLiteral("/examples"), app + QStringLiteral("/../share/simnodus/examples")};
#ifdef SIMNODUS_SOURCE_EXAMPLES_DIR
    folders << QStringLiteral(SIMNODUS_SOURCE_EXAMPLES_DIR);
#endif
    return folders;
}

QString speed_text(double speed)
{
    return speed <= 0 ? QCoreApplication::translate("MainWindow", "Maximum") : QStringLiteral("%1x").arg(speed);
}

} // namespace

MainWindow::MainWindow(QSettings& settings, QWidget* parent) : QMainWindow(parent), settings_(settings)
{
    setObjectName(QStringLiteral("mainWindow"));
    resize(1280, 800);
    sim::EngineHost::instance().set_application_dir(QCoreApplication::applicationDirPath().toStdWString());

    messages_ = new MessagesPanel;
    load_library();
    set_user_models_folders(settings_.value(QStringLiteral("models/folders")).toStringList());
    document_ = new Document(library_, this);
    scene_ = new SchematicScene(*document_, this);
    run_ = new RunController(*document_, *scene_, this);

    components_ = new ComponentPanel(library_);
    view_ = new SchematicView;
    view_->set_schematic_scene(scene_);
    view_->set_edit_guard([this] { return allow_structure_edit(); });
    properties_ = new PropertiesPanel(document_);

    columns_ = new QSplitter(Qt::Horizontal);
    columns_->setObjectName(QStringLiteral("columns"));
    columns_->addWidget(components_);
    columns_->addWidget(view_);
    columns_->addWidget(properties_);
    columns_->setStretchFactor(0, 0);
    columns_->setStretchFactor(1, 1);
    columns_->setStretchFactor(2, 0);
    // Panels collapse only through the View actions, so a drag never hides one.
    columns_->setChildrenCollapsible(false);
    components_->setMinimumWidth(160);
    properties_->setMinimumWidth(180);
    view_->setMinimumWidth(center_minimum);

    rows_ = new QSplitter(Qt::Vertical);
    rows_->setObjectName(QStringLiteral("rows"));
    rows_->addWidget(columns_);
    rows_->addWidget(messages_);
    rows_->setStretchFactor(0, 1);
    rows_->setStretchFactor(1, 0);
    rows_->setChildrenCollapsible(false);
    messages_->setMinimumHeight(70);
    setCentralWidget(rows_);

    analyzer_ = std::make_unique<AnalyzerWindow>();
    analyzer_->set_controller(run_);

    build_actions_and_menus();
    build_status_bar();

    connect(components_, &ComponentPanel::placement_requested, this, [this](const QString& type) {
        view_->begin_place(document_->library().find(type.toStdString()));
        view_->setFocus();
    });
    connect(view_, &SchematicView::cursor_moved, this, [this](QPointF p) {
        cursor_label_->setText(tr("x %1  y %2").arg(qRound(p.x())).arg(qRound(p.y())));
    });
    connect(view_, &SchematicView::zoom_changed, this,
        [this](double zoom) { zoom_label_->setText(tr("%1 %").arg(qRound(zoom * 100))); });
    connect(view_, &SchematicView::status_message, this, [this](const QString& text) { statusBar()->showMessage(text, 6000); });
    connect(view_, &SchematicView::edit_value_requested, this, [this](model::Id id) {
        // A double-click on a block opens the block's file (PRJ-06).
        if (const auto* c = document_->schematic().component(id); c && !model::block_name(c->type).empty()) {
            open_block(id);
            return;
        }
        scene_->select_ids({id});
        set_panel_visible(Panel::Properties, true);
        properties_->focus_value();
    });
    connect(view_, &SchematicView::control_changed, run_, &RunController::set_control);
    connect(view_, &SchematicView::mcu_reset_requested, run_, &RunController::mcu_reset);
    connect(view_, &SchematicView::label_name_needed, this, [this](model::Point at) { ask_label_name(at.x, at.y); });
    connect(view_, &SchematicView::note_text_needed, this, &MainWindow::ask_note_text);
    connect(view_, &SchematicView::bus_name_needed, this, &MainWindow::ask_bus_name);
    connect(view_, &SchematicView::splitter_needed, this, &MainWindow::ask_splitter);
    connect(scene_, &QGraphicsScene::selectionChanged, this, [this] {
        properties_->set_selection(scene_->selected_ids());
        properties_->set_live_text(run_->live_text(scene_->selected_ids()));
    });
    connect(properties_, &PropertiesPanel::firmware_reload_requested, this, [this] {
        if (run_->active()) {
            run_->stop();
            run();
            statusBar()->showMessage(tr("Firmware reloaded; the simulation restarted."), 4000);
        } else {
            statusBar()->showMessage(tr("The firmware is read when the simulation starts."), 4000);
        }
    });
    connect(properties_, &PropertiesPanel::message, this, [this](const QString& text) {
        statusBar()->showMessage(text, 6000);
    });
    connect(properties_, &PropertiesPanel::models_read, this, &MainWindow::report_model_issues);
    connect(properties_, &PropertiesPanel::block_open_requested, this, &MainWindow::open_block);
    connect(properties_, &PropertiesPanel::block_update_requested, this, &MainWindow::update_block);
    connect(document_, &Document::library_changed, this, [this] {
        components_->set_library(document_->library());
        for (const auto& problem : document_->block_problems()) {
            messages_->add(Severity::Warning, problem, tr("Blocks"));
        }
    });
    connect(document_, &Document::file_changed, this, &MainWindow::update_title);
    connect(document_, &Document::changed, this, [this] {
        setWindowModified(document_->modified());
        if (run_->active()) {
            statusBar()->showMessage(tr("Changes take effect when the simulation is restarted."), 4000);
        }
    });
    connect(messages_, &MessagesPanel::counts_changed, this, &MainWindow::update_counts);
    connect(messages_, &MessagesPanel::error_added, this, [this] { set_panel_visible(Panel::Messages, true); });
    connect(messages_, &MessagesPanel::items_activated, this, [this](const std::vector<model::Id>& ids) {
        std::set<model::Id> present;
        for (model::Id id : ids) {
            if (document_->schematic().contains(id)) {
                present.insert(id);
            }
        }
        if (!present.empty()) {
            view_->center_on_item(*present.begin());
            scene_->select_ids(present);
        }
    });
    connect(analyzer_.get(), &AnalyzerWindow::visibility_changed, analyzer_action_, &QAction::setChecked);
    connect(run_, &RunController::state_changed, this, &MainWindow::update_run_state);
    connect(run_, &RunController::refreshed, this, [this] {
        update_status();
        properties_->set_live_text(run_->live_text(scene_->selected_ids()));
    });
    connect(run_, &RunController::mcu_message, this, [this](model::Id board, const QString& text) {
        const model::Component* c = document_->schematic().component(board);
        messages_->add(Severity::Warning, text, c ? QString::fromStdString(c->designator) : tr("Microcontroller"), {board});
    });
    connect(run_, &RunController::failed, this, [this](const QString& message, const QStringList& output) {
        for (const auto& line : output) {
            messages_->add(Severity::Info, line, tr("Engine"));
        }
        messages_->add(Severity::Error, message, tr("Simulation"));
    });

    columns_->setSizes({remembered_[0], 1280 - remembered_[0] - remembered_[1], remembered_[1]});
    rows_->setSizes({640, remembered_[2]});
    messages_->hide();
    restore_settings();
    sync_panel_actions();
    rebuild_recent_menu();
    rebuild_examples_menu();
    QTimer::singleShot(0, this, &MainWindow::report_model_issues);
    update_title();
    update_run_state();
    recovery_timer_ = new QTimer(this);
    recovery_timer_->setInterval(60 * 1000);
    connect(recovery_timer_, &QTimer::timeout, this, [this] {
        if (document_->modified()) {
            write_recovery();
        }
    });
    recovery_timer_->start();
    messages_->add(Severity::Info, tr("SimNodus %1 started; %2 component types loaded.")
                                       .arg(QStringLiteral(SIMNODUS_VERSION_FULL))
                                       .arg(static_cast<int>(library_.definitions().size())));
}

MainWindow::~MainWindow()
{
    run_->stop();
    if (engine_thread_) {
        engine_thread_->wait();
    }
}

void MainWindow::load_library()
{
    const std::filesystem::path app = QCoreApplication::applicationDirPath().toStdWString();
    for (const auto& folder : library::library_candidates(app)) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec)) {
            continue;
        }
        for (const auto& m : library_.load_directory(folder)) {
            messages_->add(Severity::Warning,
                tr("Component file %1 skipped: %2")
                    .arg(QString::fromStdWString(m.file.filename().wstring()), QString::fromStdString(m.text)),
                tr("Library"));
        }
        if (!library_.empty()) {
            return;
        }
    }
    messages_->add(Severity::Error, tr("The component library was not found next to the application."), tr("Library"));
}

void MainWindow::build_actions_and_menus()
{
    auto* style = QApplication::style();
    auto planned = [this](QMenu* menu, const QString& text, const QKeySequence& shortcut, const QString& task) {
        auto* action = menu->addAction(text);
        action->setShortcut(shortcut);
        action->setEnabled(false);
        action->setStatusTip(tr("Planned (%1)").arg(task));
        return action;
    };
    auto place = [this](const char* type) {
        return [this, type] { view_->begin_place(library_.find(type)); };
    };

    auto* file = menuBar()->addMenu(tr("&File"));
    auto* new_action = file->addAction(style->standardIcon(QStyle::SP_FileIcon), tr("&New"), this, &MainWindow::new_file);
    new_action->setShortcut(QKeySequence::New);
    auto* open_action =
        file->addAction(style->standardIcon(QStyle::SP_DialogOpenButton), tr("&Open..."), this, &MainWindow::open_dialog);
    open_action->setShortcut(QKeySequence::Open);
    recent_menu_ = file->addMenu(tr("Open &Recent"));
    examples_menu_ = file->addMenu(tr("&Examples"));
    auto* save_action = file->addAction(style->standardIcon(QStyle::SP_DialogSaveButton), tr("&Save"), this, [this] { save(); });
    save_action->setShortcut(QKeySequence::Save);
    auto* save_as_action = file->addAction(tr("Save &As..."), this, [this] { save_as(); });
    save_as_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    file->addSeparator();
    auto* quit = file->addAction(tr("E&xit"), this, &QWidget::close);
    // QKeySequence::Quit has no usable key on Windows.
    quit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    auto* undo = document_->undo_stack()->createUndoAction(this, tr("&Undo"));
    undo->setShortcut(QKeySequence::Undo);
    undo->setIcon(style->standardIcon(QStyle::SP_ArrowBack));
    auto* redo = document_->undo_stack()->createRedoAction(this, tr("&Redo"));
    redo->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    redo->setIcon(style->standardIcon(QStyle::SP_ArrowForward));
    edit->addAction(undo);
    edit->addAction(redo);
    edit->addSeparator();
    edit->addAction(tr("Cu&t"), view_, &SchematicView::cut_selection)->setShortcut(QKeySequence::Cut);
    edit->addAction(tr("&Copy"), view_, &SchematicView::copy_selection)->setShortcut(QKeySequence::Copy);
    edit->addAction(tr("&Paste"), view_, &SchematicView::paste)->setShortcut(QKeySequence::Paste);
    auto* del = edit->addAction(tr("&Delete"), view_, &SchematicView::delete_selection);
    del->setShortcuts({QKeySequence::Delete, QKeySequence(Qt::Key_Backspace)});
    edit->addAction(tr("Select &All"), view_, &SchematicView::select_all)->setShortcut(QKeySequence::SelectAll);
    edit->addSeparator();
    edit->addAction(tr("&Rotate"), view_, &SchematicView::rotate)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    edit->addAction(tr("&Mirror"), view_, &SchematicView::mirror)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    for (auto* action : edit->actions()) {
        action->setShortcutContext(Qt::WindowShortcut);
    }

    auto* view = menuBar()->addMenu(tr("&View"));
    const std::array<QString, 3> names{tr("&Components"), tr("&Properties"), tr("&Messages")};
    const std::array<Qt::Key, 3> keys{Qt::Key_1, Qt::Key_2, Qt::Key_3};
    for (int i = 0; i < 3; ++i) {
        auto* action = view->addAction(names[static_cast<std::size_t>(i)]);
        action->setCheckable(true);
        action->setShortcut(QKeySequence(Qt::CTRL | keys[static_cast<std::size_t>(i)]));
        const Panel panel = static_cast<Panel>(i);
        connect(action, &QAction::triggered, this, [this, panel](bool on) { set_panel_visible(panel, on); });
        panel_actions_[static_cast<std::size_t>(i)] = action;
    }
    focus_action_ = view->addAction(tr("&Focus Canvas"));
    focus_action_->setCheckable(true);
    focus_action_->setShortcut(QKeySequence(Qt::Key_F11));
    connect(focus_action_, &QAction::triggered, this, &MainWindow::set_focus_canvas);
    view->addSeparator();
    view->addAction(tr("Zoom &In"), view_, &SchematicView::zoom_in)->setShortcut(QKeySequence::ZoomIn);
    view->addAction(tr("Zoom &Out"), view_, &SchematicView::zoom_out)->setShortcut(QKeySequence::ZoomOut);
    view->addAction(tr("&Fit Circuit"), view_, &SchematicView::fit_content)
        ->setShortcuts({QKeySequence(Qt::Key_F), QKeySequence(Qt::Key_Home)});
    view->addSeparator();
    // Light or dark colors (SN-162), applied at once and kept for the next start.
    auto* theme_menu = view->addMenu(tr("&Theme"));
    auto* themes = new QActionGroup(theme_menu);
    const auto current = theme::scheme_from(settings_.value(QStringLiteral("theme")).toString());
    for (const auto& [scheme, name] : std::vector<std::pair<theme::Scheme, QString>>{
             {theme::Scheme::System, tr("System")}, {theme::Scheme::Light, tr("Light")},
             {theme::Scheme::Dark, tr("Dark")}}) {
        auto* action = theme_menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(scheme == current);
        action->setData(theme::scheme_setting(scheme));
        action->setObjectName(QStringLiteral("theme_%1").arg(scheme == theme::Scheme::System ? QStringLiteral("system")
                                                                 : theme::scheme_setting(scheme)));
        themes->addAction(action);
        connect(action, &QAction::triggered, this, [this, scheme] {
            if (scheme == theme::Scheme::System) {
                settings_.remove(QStringLiteral("theme"));
            } else {
                settings_.setValue(QStringLiteral("theme"), theme::scheme_setting(scheme));
            }
            theme::apply(*qApp, scheme);
            scene_->update();
        });
    }
    auto* voltages = view->addAction(tr("Show &Voltages"));
    voltages->setCheckable(true);
    voltages->setStatusTip(tr("Color wires by their voltage while the simulation runs"));
    connect(voltages, &QAction::toggled, scene_, &SchematicScene::set_show_voltages);

    auto* placement = menuBar()->addMenu(tr("&Place"));
    auto* wire = placement->addAction(tr("&Wire"), this, [this] { view_->set_tool(Tool::Wire); });
    wire->setShortcut(QKeySequence(Qt::Key_W));
    placement->addAction(tr("&Net Label"), this, [this] { view_->set_tool(Tool::Label); })->setShortcut(QKeySequence(Qt::Key_N));
    placement->addAction(tr("&Ground"), this, place("ground"))->setShortcut(QKeySequence(Qt::Key_G));
    auto* note = placement->addAction(tr("&Text Note"), this, [this] { view_->set_tool(Tool::Note); });
    note->setShortcut(QKeySequence(Qt::Key_T));
    note->setStatusTip(tr("Free text on the schematic, such as instructions for the simulation"));
    auto* bus = placement->addAction(tr("&Bus"), this, [this] { view_->set_tool(Tool::Bus); });
    bus->setShortcut(QKeySequence(Qt::Key_B));
    bus->setStatusTip(tr("A thick line standing for several wires; splitters join its bits to wires"));
    auto* splitter = placement->addAction(tr("&Splitter"), this, [this] { view_->set_tool(Tool::Splitter); });
    splitter->setShortcut(QKeySequence(Qt::Key_S));
    splitter->setStatusTip(tr("Joins a bus to single wires: its bit pins act as labels D0, D1, ..."));
    auto* probe = placement->addAction(tr("P&robe"), this, [this] { view_->set_tool(Tool::Probe); });
    probe->setShortcut(QKeySequence(Qt::Key_P));
    auto* block = placement->addAction(tr("B&lock..."), this, &MainWindow::place_block);
    block->setStatusTip(tr("Place another project as a block; its Block port parts become the block's pins"));
    placement->addSeparator();
    placement->addAction(tr("Resistor"), this, place("resistor"))->setShortcut(QKeySequence(Qt::Key_R));
    placement->addAction(tr("Capacitor"), this, place("capacitor"))->setShortcut(QKeySequence(Qt::Key_C));
    placement->addAction(tr("Inductor"), this, place("inductor"))->setShortcut(QKeySequence(Qt::Key_L));
    placement->addAction(tr("Diode"), this, place("diode"))->setShortcut(QKeySequence(Qt::Key_D));
    wire->setText(tr("&Wire"));

    auto* simulate = menuBar()->addMenu(tr("&Simulate"));
    run_action_ = simulate->addAction(style->standardIcon(QStyle::SP_MediaPlay), tr("&Run"), this, &MainWindow::run);
    run_action_->setShortcut(QKeySequence(Qt::Key_F5));
    pause_action_ =
        simulate->addAction(style->standardIcon(QStyle::SP_MediaPause), tr("&Pause / Resume"), this, &MainWindow::pause_resume);
    pause_action_->setShortcut(QKeySequence(Qt::Key_F6));
    // Pressed (and highlighted in the toolbar) while the simulation is paused.
    pause_action_->setCheckable(true);
    pause_action_->setObjectName(QStringLiteral("pauseAction"));
    stop_action_ = simulate->addAction(style->standardIcon(QStyle::SP_MediaStop), tr("&Stop"), this, &MainWindow::stop);
    stop_action_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
    reset_action_ = simulate->addAction(tr("R&eset"), this, &MainWindow::reset);
    reset_action_->setStatusTip(tr("Restart the simulation from the beginning"));
    simulate->addSeparator();
    simulate->addAction(tr("Simulation &Settings..."), this, &MainWindow::edit_settings);
    debug_action_ = simulate->addAction(tr("&Debug Firmware with GDB (port %1)").arg(default_gdb_port));
    debug_action_->setCheckable(true);
    debug_action_->setStatusTip(tr("The next run waits for STM32CubeIDE or arm-none-eabi-gdb on 127.0.0.1 port %1 "
                                   "(the next board on %2, ...); the circuit stops while the firmware is halted")
                                    .arg(default_gdb_port)
                                    .arg(default_gdb_port + 1));
    // Not remembered between sessions: a forgotten debug mode left every board
    // waiting for a debugger that never came (owner's test, 2026-10-18).
    settings_.remove(QStringLiteral("mcu/debug"));
    connect(debug_action_, &QAction::toggled, this, [this](bool on) { run_->set_gdb_port(on ? default_gdb_port : -1); });
    run_->set_gdb_port(debug_action_->isChecked() ? default_gdb_port : -1);

    auto* tools = menuBar()->addMenu(tr("&Tools"));
    engine_action_ = tools->addAction(tr("Check Simulation &Engine"), this, &MainWindow::check_engine);
    engine_action_->setStatusTip(tr("Load the analog engine and solve a reference circuit"));
    auto* models = tools->addAction(tr("&Model Folders..."), this, &MainWindow::edit_model_folders);
    // Interface language (UI-16); it takes effect at the next start.
    auto* language = tools->addMenu(tr("&Language"));
    auto* languages = new QActionGroup(language);
    const QString chosen = settings_.value(QStringLiteral("language")).toString();
    for (const auto& [code, name] : std::vector<std::pair<QString, QString>>{
             {QString(), tr("System language")}, {QStringLiteral("en"), QStringLiteral("English")},
             {QStringLiteral("pt_BR"), QStringLiteral("Português (Brasil)")}}) {
        auto* action = language->addAction(name);
        action->setCheckable(true);
        action->setChecked(code == chosen);
        action->setData(code);
        languages->addAction(action);
        connect(action, &QAction::triggered, this, [this, code] {
            if (code.isEmpty()) {
                settings_.remove(QStringLiteral("language"));
            } else {
                settings_.setValue(QStringLiteral("language"), code);
            }
            const QString note = tr("The language changes when SimNodus starts again.");
            statusBar()->showMessage(note, 8000);
            messages_->add(Severity::Info, note, tr("Language"));
        });
    }
    models->setStatusTip(tr("SPICE model files (*.lib) for transistors, MOSFETs and diodes; their models appear in Properties"));

    auto* window = menuBar()->addMenu(tr("&Window"));
    analyzer_action_ = window->addAction(tr("Signal &Analyzer"));
    analyzer_action_->setCheckable(true);
    analyzer_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(analyzer_action_, &QAction::triggered, this, [this](bool on) {
        if (on) {
            show_analyzer();
        } else {
            analyzer_->hide();
        }
    });

    auto* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&Keyboard Shortcuts"), this, &MainWindow::show_shortcuts)->setShortcut(QKeySequence::HelpContents);
    help->addAction(tr("&User Guide"), this, &MainWindow::show_user_guide);
    help->addSeparator();
    help->addAction(tr("&About SimNodus"), this, &MainWindow::show_about);
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
    (void)planned;

    auto* toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->addAction(new_action);
    toolbar->addAction(open_action);
    toolbar->addAction(save_action);
    toolbar->addSeparator();
    toolbar->addAction(undo);
    toolbar->addAction(redo);
    toolbar->addSeparator();
    toolbar->addAction(wire);
    toolbar->addAction(probe);
    toolbar->addSeparator();
    toolbar->addAction(run_action_);
    toolbar->addAction(pause_action_);
    toolbar->addAction(stop_action_);
    speed_ = new QComboBox;
    speed_->setObjectName(QStringLiteral("speedFactor"));
    for (double s : {0.001, 0.01, 0.1, 0.5, 1.0, 0.0}) {
        speed_->addItem(speed_text(s), s);
    }
    speed_->setCurrentIndex(4);
    speed_->setToolTip(tr("Simulation speed relative to real time"));
    connect(speed_, &QComboBox::activated, this, [this](int) {
        const double s = speed_->currentData().toDouble();
        run_->set_speed(s);
        auto settings = document_->schematic().settings();
        if (settings.speed != s) {
            settings.speed = s;
            document_->apply(editing::set_settings(document_->schematic(), settings));
        }
    });
    toolbar->addWidget(speed_);
    toolbar->addSeparator();
    toolbar->addAction(analyzer_action_);
}

void MainWindow::build_status_bar()
{
    state_label_ = new QLabel;
    time_label_ = new QLabel;
    speed_label_ = new QLabel;
    mode_label_ = new QLabel;
    warnings_label_ = new QLabel;
    cursor_label_ = new QLabel;
    zoom_label_ = new QLabel(tr("%1 %").arg(qRound(view_->zoom() * 100)));
    for (auto* label : {state_label_, time_label_, speed_label_, mode_label_, warnings_label_}) {
        statusBar()->addWidget(label);
    }
    statusBar()->addPermanentWidget(cursor_label_);
    statusBar()->addPermanentWidget(zoom_label_);
    update_counts(0, 0);
    update_status();
}

QWidget* MainWindow::panel_widget(Panel panel) const
{
    switch (panel) {
    case Panel::Components:
        return components_;
    case Panel::Properties:
        return properties_;
    case Panel::Messages:
        break;
    }
    return messages_;
}

QSplitter* MainWindow::panel_splitter(Panel panel) const
{
    return panel == Panel::Messages ? rows_ : columns_;
}

bool MainWindow::panel_visible(Panel panel) const
{
    return !panel_widget(panel)->isHidden();
}

void MainWindow::set_panel_visible(Panel panel, bool visible)
{
    QWidget* widget = panel_widget(panel);
    QSplitter* splitter = panel_splitter(panel);
    const int index = splitter->indexOf(widget);
    const int center = panel == Panel::Messages ? 0 : 1;
    auto& remembered = remembered_[static_cast<std::size_t>(panel)];
    if (visible == panel_visible(panel)) {
        sync_panel_actions();
        return;
    }
    if (!visible) {
        const int size = splitter->sizes().value(index);
        if (size > 0) {
            remembered = size;
        }
        widget->hide();
    } else {
        widget->show();
        QList<int> sizes = splitter->sizes();
        const int total = std::accumulate(sizes.begin(), sizes.end(), 0);
        const int others = total - sizes[center] - sizes[index];
        const int wanted = std::max(remembered, 1);
        if (total > 0) {
            sizes[index] = std::min(wanted, std::max(1, total - others - center_minimum));
            sizes[center] = std::max(0, total - others - sizes[index]);
            splitter->setSizes(sizes);
        }
    }
    if (focus_mode_ && visible) {
        focus_mode_ = false;
    }
    sync_panel_actions();
}

void MainWindow::toggle_panel(Panel panel)
{
    set_panel_visible(panel, !panel_visible(panel));
}

void MainWindow::set_focus_canvas(bool enabled)
{
    if (enabled == focus_mode_) {
        sync_panel_actions();
        return;
    }
    if (enabled) {
        for (int i = 0; i < 3; ++i) {
            before_focus_[static_cast<std::size_t>(i)] = panel_visible(static_cast<Panel>(i));
        }
        for (int i = 0; i < 3; ++i) {
            set_panel_visible(static_cast<Panel>(i), false);
        }
        focus_mode_ = true;
    } else {
        focus_mode_ = false;
        for (int i = 0; i < 3; ++i) {
            set_panel_visible(static_cast<Panel>(i), before_focus_[static_cast<std::size_t>(i)]);
        }
    }
    sync_panel_actions();
}

void MainWindow::sync_panel_actions()
{
    for (int i = 0; i < 3; ++i) {
        panel_actions_[static_cast<std::size_t>(i)]->setChecked(panel_visible(static_cast<Panel>(i)));
    }
    focus_action_->setChecked(focus_mode_);
}

void MainWindow::show_analyzer()
{
    analyzer_->show();
    analyzer_->raise();
    analyzer_->activateWindow();
}

void MainWindow::update_counts(int warnings, int errors)
{
    warnings_label_->setText(errors > 0 ? tr("%n warning(s)", nullptr, warnings) + QStringLiteral(", ")
                                              + tr("%n error(s)", nullptr, errors)
                                        : tr("%n warning(s)", nullptr, warnings));
}

void MainWindow::update_title()
{
    setWindowTitle(QStringLiteral("%1[*] - SimNodus").arg(document_->display_name()));
    setWindowModified(document_->modified());
}

void MainWindow::update_status()
{
    const auto status = run_->status();
    const bool interactive = document_->schematic().settings().mode == model::SimulationMode::Interactive;
    QString state;
    switch (status.state) {
    case sim::RunState::Running:
        if (status.debugger_waiting) {
            state = tr("Waiting for the debugger (GDB port %1)").arg(status.gdb_port);
        } else if (status.debugger_halted) {
            state = tr("Halted by the debugger");
        } else {
            state = status.starting_mcu ? tr("Starting the microcontroller...") : tr("Running");
        }
        break;
    case sim::RunState::Paused:
        state = tr("Paused");
        break;
    case sim::RunState::Finished:
        state = tr("Finished");
        break;
    case sim::RunState::Failed:
        state = tr("Failed");
        break;
    default:
        state = tr("Stopped");
    }
    state_label_->setText(state);
    const bool any = status.state != sim::RunState::Idle;
    time_label_->setText(any ? tr("t = %1s").arg(QString::fromStdString(model::format_engineering(status.sim_time, 4)))
                             : tr("t = -"));
    if (status.state == sim::RunState::Running && status.mode == model::SimulationMode::Interactive) {
        speed_label_->setText(tr("%1x real time").arg(QString::number(status.achieved_factor, 'f', status.achieved_factor < 0.1 ? 4 : 2)));
    } else if (status.state == sim::RunState::Running) {
        speed_label_->setText(tr("%1 %").arg(qRound(status.progress * 100)));
    } else {
        speed_label_->setText(tr("- x real time"));
    }
    mode_label_->setToolTip(run_->mode_description());
    mode_label_->setText(interactive ? tr("Interactive, approximate timing (step %1s)")
                                           .arg(QString::fromStdString(
                                               model::format_engineering(document_->schematic().settings().max_step)))
                                     : tr("Transient"));
}

void MainWindow::update_run_state()
{
    const auto state = run_->state();
    const bool active = state == sim::RunState::Running || state == sim::RunState::Paused;
    run_action_->setEnabled(!active);
    pause_action_->setEnabled(active && document_->schematic().settings().mode == model::SimulationMode::Interactive);
    pause_action_->setChecked(state == sim::RunState::Paused);
    stop_action_->setEnabled(active);
    reset_action_->setEnabled(active);
    engine_action_->setEnabled(!active && !engine_check_running());
    view_->set_running(active);
    speed_->setEnabled(document_->schematic().settings().mode == model::SimulationMode::Interactive);
    update_status();
    if (state == sim::RunState::Finished && run_->transient_mode()) {
        analyzer_->reload();
        show_analyzer();
        messages_->add(Severity::Info,
            tr("Transient analysis finished (%1s simulated).")
                .arg(QString::fromStdString(model::format_engineering(run_->status().sim_time))),
            tr("Simulation"));
    }
}

bool MainWindow::allow_structure_edit()
{
    if (!run_->active()) {
        return true;
    }
    if (auto_discard_) {
        run_->stop();
        return true;
    }
    const auto answer = QMessageBox::question(this, tr("Simulation running"),
        tr("The circuit cannot be changed while the simulation runs. Stop the simulation?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        run_->stop();
        return true;
    }
    return false;
}

void MainWindow::report_diagnostics()
{
    messages_->clear_source(tr("Check"));
    for (const auto& d : run_->diagnostics()) {
        const Severity severity = d.severity == netlist::Severity::Error ? Severity::Error
            : d.severity == netlist::Severity::Warning                  ? Severity::Warning
                                                                         : Severity::Info;
        messages_->add(severity, QString::fromStdString(d.message), tr("Check"), d.items);
    }
}

void MainWindow::run()
{
    if (run_->active()) {
        return;
    }
    QString error;
    const bool started = run_->start(error);
    report_diagnostics();
    if (!started) {
        messages_->add(Severity::Error, error, tr("Simulation"));
        return;
    }
    run_->set_speed(document_->schematic().settings().speed);
    analyzer_->reload();
    const auto& components = document_->schematic().components();
    const bool board = std::any_of(components.begin(), components.end(), [this](const model::Component& c) {
        const auto* d = document_->library().find(c.type);
        return d && d->mcu;
    });
    if (board && debug_action_->isChecked()) {
        const QString text = tr("Waiting for the debugger: connect STM32CubeIDE or arm-none-eabi-gdb to localhost:%1. "
                                "The circuit starts when the firmware runs. To run without a debugger, turn off "
                                "Simulate > Debug Firmware with GDB.")
                                 .arg(default_gdb_port);
        statusBar()->showMessage(text, 15000);
        messages_->add(Severity::Info, text, tr("Simulation"));
        return;
    }
    statusBar()->showMessage(tr("Simulation started. Click switches and buttons to operate them."), 4000);
}

void MainWindow::pause_resume()
{
    run_->pause_or_resume();
}

void MainWindow::stop()
{
    run_->stop();
}

void MainWindow::reset()
{
    run_->stop();
    run();
}

void MainWindow::edit_settings()
{
    SimulationSettingsDialog dialog(document_->schematic().settings(), this);
    if (dialog.exec() == QDialog::Accepted) {
        document_->apply(editing::set_settings(document_->schematic(), dialog.settings()));
        update_run_state();
    }
}

void MainWindow::ask_label_name(int x, int y)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Net label"),
        tr("Name (points with the same name are connected; GND or 0 is ground):"), QLineEdit::Normal, last_label_, &ok);
    if (!ok || name.trimmed().isEmpty() || !allow_structure_edit()) {
        return;
    }
    last_label_ = name.trimmed();
    document_->apply(editing::add_label(document_->id_source(), {x, y}, last_label_.toStdString()));
}

void MainWindow::ask_note_text(model::Point at, model::Id note)
{
    const auto* existing = document_->schematic().note(note);
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(this, existing ? tr("Edit text note") : tr("Text note"),
        tr("Text (an empty text deletes the note):"),
        existing ? QString::fromStdString(existing->text) : QString(), &ok);
    if (!ok || !allow_structure_edit()) {
        return;
    }
    const std::string value = text.trimmed().isEmpty() ? std::string() : text.toStdString();
    if (existing) {
        document_->apply(editing::set_note_text(document_->schematic(), note, value));
    } else if (!value.empty()) {
        model::Id created = model::no_id;
        document_->apply(editing::add_note(document_->id_source(), at, value, &created));
        scene_->select_ids({created});
        view_->set_tool(Tool::Select);
    }
}

void MainWindow::edit_model_folders()
{
    // The folders SPICE model files are read from (PRJ-07): any number, anywhere.
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Model folders"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* intro = new QLabel(tr("SimNodus reads the .lib and .mod files of these folders for the Model lists in "
                                "Properties. A chosen model is saved inside the project, so a folder is only "
                                "needed while choosing."));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* list = new QListWidget;
    list->setObjectName(QStringLiteral("modelFolders"));
    list->addItems(user_models_folders());
    layout->addWidget(list, 1);
    auto* buttons_row = new QHBoxLayout;
    auto* add = new QPushButton(tr("Add..."));
    auto* remove = new QPushButton(tr("Remove"));
    auto* open = new QPushButton(tr("Open"));
    auto* defaults = new QPushButton(tr("Default"));
    open->setToolTip(tr("Show the selected folder in the file manager (it is created if missing)"));
    defaults->setToolTip(tr("Only %1").arg(default_models_folder()));
    for (auto* b : {add, remove, open, defaults}) {
        buttons_row->addWidget(b);
    }
    buttons_row->addStretch(1);
    layout->addLayout(buttons_row);
    connect(add, &QPushButton::clicked, &dialog, [this, list] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Add model folder"),
            list->currentItem() ? list->currentItem()->text() : default_models_folder());
        if (!folder.isEmpty() && list->findItems(folder, Qt::MatchExactly).isEmpty()) {
            list->addItem(folder);
        }
    });
    connect(remove, &QPushButton::clicked, &dialog, [list] { delete list->currentItem(); });
    connect(defaults, &QPushButton::clicked, &dialog, [list] {
        list->clear();
        list->addItem(default_models_folder());
    });
    connect(open, &QPushButton::clicked, &dialog, [this, list] {
        const QString folder = list->currentItem() ? list->currentItem()->text() : default_models_folder();
        if (!ensure_models_folder(folder)) {
            QMessageBox::warning(this, tr("Model folders"), tr("The folder %1 could not be created.").arg(folder));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);
    dialog.resize(560, 320);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QStringList folders;
    for (int i = 0; i < list->count(); ++i) {
        folders << list->item(i)->text();
    }
    set_model_folders(folders);
}

void MainWindow::set_model_folders(const QStringList& folders)
{
    const bool only_default = folders.isEmpty() || folders == QStringList{default_models_folder()};
    if (only_default) {
        settings_.remove(QStringLiteral("models/folders"));
    } else {
        settings_.setValue(QStringLiteral("models/folders"), folders);
    }
    set_user_models_folders(only_default ? QStringList{} : folders);
    statusBar()->showMessage(tr("Model folders: %1").arg(user_models_folders().join(QStringLiteral("; "))), 6000);
}

void MainWindow::report_model_issues()
{
    const int generation = user_models_generation();
    if (generation == reported_models_generation_) {
        return;
    }
    reported_models_generation_ = generation;
    const QString source = tr("Models");
    messages_->clear_source(source);
    for (const auto& issue : user_model_issues()) {
        messages_->add(Severity::Warning, tr("Models folder: %1").arg(issue), source);
    }
}

QString MainWindow::bus_name_at(const model::Point& point) const
{
    for (const auto& b : document_->schematic().buses()) {
        for (std::size_t i = 0; i + 1 < b.points.size(); ++i) {
            if (model::on_segment(point, b.points[i], b.points[i + 1])) {
                return QString::fromStdString(b.name);
            }
        }
    }
    return {};
}

void MainWindow::ask_bus_name(std::vector<model::Point> points)
{
    // A bus that starts or ends on another bus continues it: same name.
    QString name = bus_name_at(points.front());
    if (name.isEmpty()) {
        name = bus_name_at(points.back());
    }
    bool ok = false;
    name = QInputDialog::getText(this, tr("Bus"),
        tr("Name (bit k of bus D is the net Dk, joined by splitters):"), QLineEdit::Normal,
        name.isEmpty() ? last_bus_ : name, &ok).trimmed();
    if (!ok || name.isEmpty() || !allow_structure_edit()) {
        return;
    }
    last_bus_ = name;
    document_->apply(editing::add_bus(document_->id_source(), std::move(points), name.toStdString()));
}

void MainWindow::ask_splitter(model::Point at)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Splitter"));
    auto* form = new QFormLayout(&dialog);
    // Like hneemann's Digital: widths on each side; bit k on the left is bit k on the right.
    auto* help = new QLabel(tr("Pins on the left and on the right, from the top: 1 is a wire, a larger width "
                               "a bus. 8 / 8*1 splits a bus into wires; 1,1,1,1 / 4 joins wires; 4 / 2,2 splits "
                               "a bus in two."));
    help->setWordWrap(true);
    form->addRow(help);
    const bool on_bus = !bus_name_at(at).isEmpty();
    auto* inputs = new QLineEdit(on_bus ? QStringLiteral("8") : QStringLiteral("8*1"));
    inputs->setObjectName(QStringLiteral("splitterInputs"));
    auto* outputs = new QLineEdit(on_bus ? QStringLiteral("8*1") : QStringLiteral("8"));
    outputs->setObjectName(QStringLiteral("splitterOutputs"));
    auto* first = new QSpinBox;
    first->setRange(0, 63);
    first->setToolTip(tr("Bus bit of the first left bit: 4 takes D4, D5, ... from bus D"));
    form->addRow(tr("Left widths"), inputs);
    form->addRow(tr("Right widths"), outputs);
    form->addRow(tr("First bit"), first);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted || !allow_structure_edit()) {
        return;
    }
    const auto in = model::parse_widths(inputs->text().toStdString());
    const auto out = model::parse_widths(outputs->text().toStdString());
    if (!in || !out) {
        statusBar()->showMessage(tr("Widths such as 8, 8*1, 1,1,1,1 or 2,2 (64 bits at most)."), 6000);
        return;
    }
    document_->apply(editing::add_splitter(
        document_->id_source(), at, bus_name_at(at).toStdString(), first->value(), *in, *out));
}

bool MainWindow::engine_check_running() const
{
    return engine_thread_ && engine_thread_->isRunning();
}

void MainWindow::check_engine()
{
    if (engine_check_running() || run_->active()) {
        return;
    }
    engine_action_->setEnabled(false);
    messages_->add(Severity::Info, tr("Checking the analog engine..."), tr("Engine"));
    auto result = std::make_shared<sim::EngineCheckResult>();
    const std::filesystem::path app_dir = QCoreApplication::applicationDirPath().toStdWString();
    // The check loads a native library and runs a solve: never on the GUI thread.
    engine_thread_ = QThread::create([result, app_dir] { *result = sim::check_analog_engine(app_dir); });
    connect(engine_thread_, &QThread::finished, this, [this, result] {
        engine_action_->setEnabled(!run_->active());
        if (result->ok) {
            messages_->add(Severity::Info,
                tr("Analog engine ready: ngspice-%1, reference divider %2 V (%3).")
                    .arg(result->version.value_or(0))
                    .arg(result->divider_volts, 0, 'f', 4)
                    .arg(QString::fromStdWString(result->library.wstring())),
                tr("Engine"));
            statusBar()->showMessage(tr("Analog engine ready (ngspice-%1).").arg(result->version.value_or(0)), 5000);
        } else {
            for (const auto& line : result->messages) {
                messages_->add(Severity::Warning, QString::fromStdString(line), tr("Engine"));
            }
            messages_->add(Severity::Error, tr("The analog engine is not available."), tr("Engine"));
        }
        emit engine_check_finished(result->ok);
    });
    connect(engine_thread_, &QThread::finished, engine_thread_, &QObject::deleteLater);
    engine_thread_->start();
}

void MainWindow::show_user_guide()
{
    // The packages put the guide next to the executable; a build tree reads it on GitHub.
    const QString local = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("USER-GUIDE.md"));
    const QUrl url = QFileInfo::exists(local)
        ? QUrl::fromLocalFile(local)
        : QUrl(QStringLiteral("https://github.com/RicardoKers/SimNodus/blob/main/docs/user/USER_GUIDE.md"));
    if (!QDesktopServices::openUrl(url)) {
        messages_->add(Severity::Warning, tr("Could not open the user guide: %1").arg(url.toDisplayString()), tr("Help"));
    }
}

void MainWindow::show_about()
{
    QMessageBox::about(this, tr("About SimNodus"),
        tr("<h3>SimNodus %1</h3>"
           "<p>Interactive circuit and STM32 simulator for teaching.</p>"
           "<p>MIT License. Analog engine: ngspice (BSD-3-Clause), loaded at run time. "
           "Qt %2 (LGPL-3.0).</p>")
            .arg(QStringLiteral(SIMNODUS_VERSION_FULL), QString::fromLatin1(qVersion())));
}

QList<ShortcutRow> MainWindow::shortcut_rows() const
{
    QList<ShortcutRow> rows;
    for (const QAction* menu_action : menuBar()->actions()) {
        const QMenu* menu = menu_action->menu();
        if (!menu) {
            continue;
        }
        const QString group = QString(menu_action->text()).remove(QLatin1Char('&'));
        for (const QAction* action : menu->actions()) {
            if (action->isSeparator() || action->menu() || action->shortcuts().isEmpty()) {
                continue;
            }
            QStringList keys;
            for (const QKeySequence& key : action->shortcuts()) {
                keys << key.toString(QKeySequence::NativeText);
            }
            QString text = QString(action->text()).remove(QLatin1Char('&'));
            if (text.endsWith(QStringLiteral("..."))) {
                text.chop(3);
            }
            rows.push_back({group, keys.join(tr(" or ")), text});
        }
    }
    const QString canvas = tr("Schematic");
    const QString running = tr("While running");
    rows.push_back({canvas, tr("Esc"), tr("Cancel the wire or tool, then clear the selection")});
    rows.push_back({canvas, tr("Space"), tr("Wire tool: change the corner; otherwise hold and drag to pan")});
    rows.push_back({canvas, tr("Arrow keys"), tr("Move the selection one grid step (Shift: ten steps)")});
    rows.push_back({canvas, tr("Ctrl+click"), tr("Add to or remove from the selection")});
    rows.push_back({canvas, tr("Double-click"), tr("Edit a part's value; ends a wire at an open point")});
    rows.push_back({canvas, tr("Wheel / middle drag"), tr("Zoom / pan")});
    rows.push_back({canvas, tr("Shift+click"), tr("Probe tool: voltage of a pin instead of its current")});
    rows.push_back({canvas, tr("Alt+click"), tr("Probe tool: measure the last voltage probe relative to this net")});
    rows.push_back({running, tr("Click"), tr("Toggle a switch; hold a push button")});
    rows.push_back({running, tr("Drag or wheel"), tr("Turn a potentiometer")});
    return rows;
}

void MainWindow::show_shortcuts()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Keyboard Shortcuts"));
    auto* table = new QTableWidget(0, 2, &dialog);
    table->setObjectName(QStringLiteral("shortcutTable"));
    table->setHorizontalHeaderLabels({tr("Keys"), tr("Action")});
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setWordWrap(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    QString group;
    for (const ShortcutRow& row : shortcut_rows()) {
        if (row.group != group) {
            group = row.group;
            const int heading = table->rowCount();
            table->insertRow(heading);
            auto* item = new QTableWidgetItem(group);
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            table->setItem(heading, 0, item);
            table->setSpan(heading, 0, 1, 2);
        }
        const int r = table->rowCount();
        table->insertRow(r);
        table->setItem(r, 0, new QTableWidgetItem(row.keys));
        table->setItem(r, 1, new QTableWidgetItem(row.action));
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(table);
    layout->addWidget(buttons);
    dialog.resize(620, 560);
    dialog.exec();
}

bool MainWindow::confirm_discard()
{
    if (!document_->modified() || auto_discard_) {
        return true;
    }
    const auto answer = QMessageBox::warning(this, tr("Unsaved changes"),
        tr("%1 has unsaved changes. Save them?").arg(document_->display_name()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) {
        return save();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::new_file()
{
    if (!confirm_discard()) {
        return;
    }
    run_->stop();
    document_->new_document();
    scene_->clearSelection();
}

void MainWindow::open_dialog()
{
    if (!confirm_discard()) {
        return;
    }
    const QString start = document_->file_path().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : QFileInfo(document_->file_path()).path();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open project"), start, project_filter);
    if (!path.isEmpty()) {
        auto_discard_ = true; // already confirmed
        open_file(path);
        auto_discard_ = false;
    }
}

bool MainWindow::open_file(const QString& path)
{
    if (!confirm_discard()) {
        return false;
    }
    run_->stop();
    QStringList errors, warnings;
    if (!document_->open(path, errors, warnings)) {
        for (const auto& e : errors) {
            messages_->add(Severity::Error, tr("%1: %2").arg(QFileInfo(path).fileName(), e), tr("File"));
        }
        return false;
    }
    for (const auto& w : warnings) {
        messages_->add(Severity::Warning, w, tr("File"));
    }
    add_recent(path);
    speed_->setCurrentIndex(std::max(0, speed_->findData(document_->schematic().settings().speed)));
    view_->fit_content();
    update_run_state();
    for (const auto& problem : document_->block_problems()) {
        messages_->add(Severity::Warning, problem, tr("Blocks"));
    }
    offer_block_updates();
    return true;
}

// ---- Blocks (PRJ-06, ADR 0098) -----------------------------------------------

QString MainWindow::block_path(const model::Block& block) const
{
    const QString source = QString::fromStdString(block.source);
    if (source.isEmpty() || QDir::isAbsolutePath(source)) {
        return source;
    }
    const QString base = document_->base_dir();
    return base.isEmpty() ? QString() : QDir(base).absoluteFilePath(source);
}

std::optional<model::Block> MainWindow::read_block(const QString& path, QString& error) const
{
    const QFileInfo info(path);
    if (!document_->file_path().isEmpty() && info == QFileInfo(document_->file_path())) {
        error = tr("A project cannot be a block of itself.");
        return std::nullopt;
    }
    // Reading only parses JSON: no code runs and nothing is loaded (PRJ-03).
    auto loaded = io::load_project(path.toStdWString());
    if (!loaded.schematic) {
        error = loaded.errors.empty() ? tr("The file cannot be read.") : QString::fromStdString(loaded.errors.front());
        return std::nullopt;
    }
    model::Block block;
    block.name = info.completeBaseName().toStdString();
    // Relative to the project's folder when it has one, so that both can move together.
    const QString base = document_->base_dir();
    QString source = base.isEmpty() ? info.absoluteFilePath() : QDir(base).relativeFilePath(info.absoluteFilePath());
    if (QDir::isAbsolutePath(source) || source.startsWith(QStringLiteral("../../.."))) {
        source = info.absoluteFilePath();
    }
    block.source = source.toStdString();
    block.schematic = std::make_shared<const model::Schematic>(std::move(*loaded.schematic));
    std::vector<std::string> problems;
    if (!library::block_definition(block, document_->library(), problems)) {
        QStringList text;
        for (const auto& p : problems) {
            text << QString::fromStdString(p);
        }
        error = tr("%1 cannot be a block: %2.").arg(info.fileName(), text.join(QStringLiteral("; ")));
        return std::nullopt;
    }
    return block;
}

QString MainWindow::add_block_file(const QString& path, QString& error)
{
    const auto block = read_block(path, error);
    if (!block) {
        return {};
    }
    if (const auto* existing = document_->schematic().block(block->name); existing && !(*existing == *block)) {
        const bool used = std::any_of(document_->schematic().components().begin(),
            document_->schematic().components().end(),
            [&](const model::Component& c) { return model::block_name(c.type) == block->name; });
        if (used && !auto_discard_
            && QMessageBox::question(this, tr("Replace block"),
                   tr("The project already has a block named %1. Replace it with this file? Every %1 in the "
                      "project changes.")
                       .arg(QString::fromStdString(block->name)))
                != QMessageBox::Yes) {
            error.clear();
            return {};
        }
    }
    document_->apply(editing::set_blocks(document_->schematic(), {*block}, "Add block " + block->name));
    return QString::fromStdString(model::block_type(block->name));
}

void MainWindow::place_block()
{
    const QString start = document_->base_dir().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : document_->base_dir();
    const QString path = QFileDialog::getOpenFileName(this, tr("Place block"), start, project_filter);
    if (path.isEmpty()) {
        return;
    }
    QString error;
    const QString type = add_block_file(path, error);
    if (type.isEmpty()) {
        if (!error.isEmpty()) {
            messages_->add(Severity::Error, error, tr("Blocks"));
        }
        return;
    }
    view_->begin_place(document_->library().find(type.toStdString()));
    view_->setFocus();
}

void MainWindow::open_block(model::Id component)
{
    const auto* c = document_->schematic().component(component);
    const auto* block = c ? document_->schematic().block(model::block_name(c->type)) : nullptr;
    if (!block) {
        return;
    }
    QString path = block_path(*block);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        // The project's copy is all there is: write it to a file to edit it.
        const auto answer = QMessageBox::question(this, tr("Block file not found"),
            tr("The file of block %1 (%2) was not found. Save the project's copy as a file to edit it?")
                .arg(QString::fromStdString(block->name), QString::fromStdString(block->source)));
        if (answer != QMessageBox::Yes) {
            return;
        }
        const QString start = QDir(document_->base_dir().isEmpty()
                                       ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                       : document_->base_dir())
                                  .filePath(QString::fromStdString(block->name) + QStringLiteral(".snx"));
        path = QFileDialog::getSaveFileName(this, tr("Save block"), start, project_filter);
        if (path.isEmpty()) {
            return;
        }
        const auto saved = io::save_project(*block->schematic, path.toStdWString());
        if (!saved.ok) {
            messages_->add(Severity::Error, QString::fromStdString(saved.error), tr("Blocks"));
            return;
        }
        QString error;
        if (const auto updated = read_block(path, error)) {
            model::Block moved = *block;
            moved.source = updated->source;
            document_->apply(editing::set_blocks(document_->schematic(), {moved}, "Save block " + block->name));
        }
    }
    const QString project = document_->file_path();
    if (!open_file(path)) {
        return;
    }
    const QString text = project.isEmpty()
        ? tr("Editing block %1. Save it; placed blocks use it after Update.").arg(QFileInfo(path).fileName())
        : tr("Editing block %1. Save it, then open %2 again (File > Recent) to update the project's copy.")
              .arg(QFileInfo(path).fileName(), QFileInfo(project).fileName());
    messages_->add(Severity::Info, text, tr("Blocks"));
    statusBar()->showMessage(text, 10000);
}

void MainWindow::update_block(model::Id component)
{
    const auto* c = document_->schematic().component(component);
    const auto* block = c ? document_->schematic().block(model::block_name(c->type)) : nullptr;
    if (!block) {
        return;
    }
    const QString path = block_path(*block);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        messages_->add(Severity::Error,
            tr("The file of block %1 (%2) was not found; the project keeps its copy.")
                .arg(QString::fromStdString(block->name), QString::fromStdString(block->source)),
            tr("Blocks"));
        return;
    }
    QString error;
    auto updated = read_block(path, error);
    if (!updated) {
        messages_->add(Severity::Error, error, tr("Blocks"));
        return;
    }
    updated->name = block->name; // the file may have been renamed
    updated->source = block->source;
    if (*updated == *block) {
        statusBar()->showMessage(tr("Block %1 is up to date.").arg(QString::fromStdString(block->name)), 4000);
        return;
    }
    const QString name = QString::fromStdString(block->name); // the edit replaces *block
    document_->apply(editing::set_blocks(document_->schematic(), {*updated}, "Update block " + updated->name));
    statusBar()->showMessage(tr("Block %1 updated from its file.").arg(name), 4000);
}

QStringList MainWindow::changed_block_files() const
{
    QStringList names;
    for (const auto& [name, block] : document_->schematic().blocks()) {
        const QString path = block_path(block);
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            continue;
        }
        const auto loaded = io::load_project(path.toStdWString());
        if (loaded.schematic && !(*loaded.schematic == *block.schematic)) {
            names << QString::fromStdString(name);
        }
    }
    return names;
}

void MainWindow::offer_block_updates()
{
    const QStringList names = changed_block_files();
    if (names.isEmpty()) {
        return;
    }
    const QString list = names.join(QStringLiteral(", "));
    if (auto_discard_
        || QMessageBox::question(this, tr("Blocks changed"),
               tr("The files of these blocks changed since the project copied them: %1. Update the project's "
                  "copies?")
                   .arg(list))
            != QMessageBox::Yes) {
        messages_->add(Severity::Info,
            tr("Block files changed: %1. Select a block and choose Update in Properties to use the new version.")
                .arg(list),
            tr("Blocks"));
        return;
    }
    std::vector<model::Block> updated;
    for (const auto& name : names) {
        const auto* block = document_->schematic().block(name.toStdString());
        QString error;
        auto fresh = read_block(block_path(*block), error);
        if (!fresh) {
            messages_->add(Severity::Error, error, tr("Blocks"));
            continue;
        }
        fresh->name = block->name;
        fresh->source = block->source;
        updated.push_back(*fresh);
    }
    if (!updated.empty()) {
        document_->apply(editing::set_blocks(document_->schematic(), updated, "Update blocks"));
        messages_->add(Severity::Info, tr("Blocks updated from their files: %1.").arg(list), tr("Blocks"));
    }
}

bool MainWindow::save()
{
    if (document_->file_path().isEmpty()) {
        return save_as();
    }
    return save_to(document_->file_path());
}

bool MainWindow::save_as()
{
    QString start = document_->file_path();
    if (start.isEmpty()) {
        start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/circuit.snx");
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Save project"), start, project_filter);
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".snx"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".snx");
    }
    return save_to(path);
}

bool MainWindow::save_to(const QString& path)
{
    QString error;
    if (!document_->save(path, error)) {
        messages_->add(Severity::Error, tr("Saving failed: %1").arg(error), tr("File"));
        QMessageBox::warning(this, tr("Save project"), tr("The project could not be saved:\n%1").arg(error));
        return false;
    }
    add_recent(path);
    QFile::remove(recovery_file()); // the saved file is now the safe copy
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)), 4000);
    return true;
}

void MainWindow::add_recent(const QString& path)
{
    QStringList recent = settings_.value(QStringLiteral("files/recent")).toStringList();
    const QString absolute = QFileInfo(path).absoluteFilePath();
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while (recent.size() > max_recent) {
        recent.removeLast();
    }
    settings_.setValue(QStringLiteral("files/recent"), recent);
    // Often called from one of the recent-file actions: rebuild after it returns.
    QTimer::singleShot(0, this, &MainWindow::rebuild_recent_menu);
}

void MainWindow::rebuild_recent_menu()
{
    recent_menu_->clear();
    const QStringList recent = settings_.value(QStringLiteral("files/recent")).toStringList();
    for (const auto& path : recent) {
        auto* action = recent_menu_->addAction(QDir::toNativeSeparators(path));
        action->setEnabled(QFileInfo::exists(path));
        connect(action, &QAction::triggered, this, [this, path] { open_file(path); });
    }
    recent_menu_->setEnabled(!recent.isEmpty());
}

void MainWindow::rebuild_examples_menu()
{
    examples_menu_->clear();
    for (const auto& folder : examples_folders()) {
        const QDir dir(folder);
        const auto files = dir.entryInfoList({QStringLiteral("*.snx")}, QDir::Files, QDir::Name);
        if (files.isEmpty()) {
            continue;
        }
        for (const auto& info : files) {
            auto* action = examples_menu_->addAction(info.completeBaseName().replace(QLatin1Char('_'), QLatin1Char(' ')));
            const QString path = info.absoluteFilePath();
            connect(action, &QAction::triggered, this, [this, path] {
                if (!confirm_discard()) {
                    return;
                }
                run_->stop();
                QStringList errors, warnings;
                // Examples open as untitled copies (UI-15).
                if (!document_->open_copy(path, errors, warnings)) {
                    for (const auto& e : errors) {
                        messages_->add(Severity::Error, e, tr("File"));
                    }
                    return;
                }
                speed_->setCurrentIndex(std::max(0, speed_->findData(document_->schematic().settings().speed)));
                view_->fit_content();
                update_run_state();
            });
        }
        break;
    }
    examples_menu_->setEnabled(!examples_menu_->isEmpty());
}

void MainWindow::save_settings()
{
    settings_.setValue(QStringLiteral("mainWindow/geometry"), saveGeometry());
    settings_.setValue(QStringLiteral("mainWindow/state"), saveState());
    for (int i = 0; i < 3; ++i) {
        const Panel panel = static_cast<Panel>(i);
        const bool visible = focus_mode_ ? before_focus_[static_cast<std::size_t>(i)] : panel_visible(panel);
        if (panel_visible(panel)) {
            const int size = panel_splitter(panel)->sizes().value(panel_splitter(panel)->indexOf(panel_widget(panel)));
            if (size > 0) {
                remembered_[static_cast<std::size_t>(i)] = size;
            }
        }
        settings_.setValue(QStringLiteral("panels/%1/visible").arg(panel_key(panel)), visible);
        settings_.setValue(QStringLiteral("panels/%1/size").arg(panel_key(panel)), remembered_[static_cast<std::size_t>(i)]);
    }
    analyzer_->save_settings(settings_);
    settings_.sync();
}

void MainWindow::restore_settings()
{
    restore_window_geometry(*this, settings_.value(QStringLiteral("mainWindow/geometry")).toByteArray());
    restoreState(settings_.value(QStringLiteral("mainWindow/state")).toByteArray());
    for (int i = 0; i < 3; ++i) {
        const Panel panel = static_cast<Panel>(i);
        const int size = settings_.value(QStringLiteral("panels/%1/size").arg(panel_key(panel)),
            remembered_[static_cast<std::size_t>(i)]).toInt();
        remembered_[static_cast<std::size_t>(i)] = std::clamp(size, 60, 2000);
    }
    columns_->setSizes(
        {remembered_[0], std::max(center_minimum, width() - remembered_[0] - remembered_[1]), remembered_[1]});
    rows_->setSizes({std::max(200, height() - remembered_[2]), remembered_[2]});
    for (int i = 0; i < 3; ++i) {
        const Panel panel = static_cast<Panel>(i);
        const bool fallback = panel != Panel::Messages;
        panel_widget(panel)->setVisible(
            settings_.value(QStringLiteral("panels/%1/visible").arg(panel_key(panel)), fallback).toBool());
    }
    analyzer_->restore_settings(settings_);
}

void MainWindow::show_restored_windows()
{
    if (settings_.value(QStringLiteral("analyzer/visible"), false).toBool()) {
        analyzer_->show();
    }
}

QString MainWindow::recovery_file() const
{
    // Next to the settings file, so that tests with their own settings stay apart.
    return QFileInfo(settings_.fileName()).absolutePath() + QStringLiteral("/recovery.snx");
}

bool MainWindow::write_recovery()
{
    QDir().mkpath(QFileInfo(recovery_file()).absolutePath());
    const auto text = io::to_json_text(document_->schematic());
    QFile file(recovery_file() + QStringLiteral(".tmp"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QByteArray::fromStdString(text));
    file.close();
    QFile::remove(recovery_file());
    const bool ok = QFile::rename(file.fileName(), recovery_file());
    settings_.setValue(QStringLiteral("recovery/original"), document_->file_path());
    settings_.sync();
    return ok;
}

bool MainWindow::check_recovery(bool automatic)
{
    const QString file = recovery_file();
    if (!QFileInfo::exists(file)) {
        return false;
    }
    const QString original = settings_.value(QStringLiteral("recovery/original")).toString();
    const QString when = QFileInfo(file).lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    bool restore = automatic;
    if (!automatic) {
        restore = QMessageBox::question(this, tr("Restore unsaved work"),
                      tr("SimNodus did not close normally. Restore the circuit saved automatically at %1%2?")
                          .arg(when, original.isEmpty() ? QString() : tr(" (from %1)").arg(QDir::toNativeSeparators(original))),
                      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
            == QMessageBox::Yes;
    }
    if (!restore) {
        QFile::remove(file);
        return false;
    }
    QStringList errors, warnings;
    if (!document_->open_copy(file, errors, warnings)) {
        for (const auto& e : errors) {
            messages_->add(Severity::Error, tr("Recovery: %1").arg(e), tr("File"));
        }
        return false;
    }
    messages_->add(Severity::Warning,
        tr("Restored unsaved work from %1. Save it with File > Save As.").arg(when), tr("File"));
    view_->fit_content();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirm_discard()) {
        event->ignore();
        return;
    }
    QFile::remove(recovery_file()); // clean exit
    run_->stop();
    save_settings();
    analyzer_->hide();
    QMainWindow::closeEvent(event);
}

} // namespace simnodus::ui
