// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "main_window.hpp"

#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "component_panel.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "schematic_view.hpp"

#include "sim/engine_check.hpp"
#include "simnodus/version.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QToolBar>
#include <QUndoStack>

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

} // namespace

MainWindow::MainWindow(QSettings& settings, QWidget* parent) : QMainWindow(parent), settings_(settings)
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(tr("Untitled - SimNodus"));
    resize(1280, 800);
    undo_stack_ = new QUndoStack(this);

    components_ = new ComponentPanel;
    view_ = new SchematicView;
    properties_ = new PropertiesPanel;
    messages_ = new MessagesPanel;

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

    build_actions_and_menus();
    build_status_bar();

    connect(components_, &ComponentPanel::placement_requested, this, [this](const QString&, const QString& name) {
        properties_->show_type(name);
        statusBar()->showMessage(tr("Placing %1 arrives with the schematic editor (next build).").arg(name), 4000);
    });
    connect(view_, &SchematicView::cursor_moved, this, [this](QPointF p) {
        cursor_label_->setText(tr("x %1  y %2").arg(qRound(p.x())).arg(qRound(p.y())));
    });
    connect(view_, &SchematicView::zoom_changed, this,
        [this](double zoom) { zoom_label_->setText(tr("%1 %").arg(qRound(zoom * 100))); });
    connect(messages_, &MessagesPanel::counts_changed, this, &MainWindow::update_counts);
    connect(messages_, &MessagesPanel::error_added, this, [this] { set_panel_visible(Panel::Messages, true); });
    connect(analyzer_.get(), &AnalyzerWindow::visibility_changed, analyzer_action_, &QAction::setChecked);

    columns_->setSizes({remembered_[0], 1280 - remembered_[0] - remembered_[1], remembered_[1]});
    rows_->setSizes({640, remembered_[2]});
    messages_->hide();
    restore_settings();
    sync_panel_actions();
    messages_->add(Severity::Info, tr("SimNodus %1 started.").arg(QStringLiteral(SIMNODUS_VERSION_FULL)));
}

MainWindow::~MainWindow()
{
    if (engine_thread_) {
        engine_thread_->wait();
    }
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

    auto* file = menuBar()->addMenu(tr("&File"));
    auto* new_action = planned(file, tr("&New"), QKeySequence::New, QStringLiteral("SN-115"));
    new_action->setIcon(style->standardIcon(QStyle::SP_FileIcon));
    auto* open_action = planned(file, tr("&Open..."), QKeySequence::Open, QStringLiteral("SN-115"));
    open_action->setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    auto* save_action = planned(file, tr("&Save"), QKeySequence::Save, QStringLiteral("SN-115"));
    save_action->setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    planned(file, tr("Save &As..."), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), QStringLiteral("SN-115"));
    file->addMenu(tr("&Examples"))->setEnabled(false);
    file->addSeparator();
    auto* quit = file->addAction(tr("E&xit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    auto* edit = menuBar()->addMenu(tr("&Edit"));
    auto* undo = undo_stack_->createUndoAction(this, tr("&Undo"));
    undo->setShortcut(QKeySequence::Undo);
    undo->setIcon(style->standardIcon(QStyle::SP_ArrowBack));
    auto* redo = undo_stack_->createRedoAction(this, tr("&Redo"));
    redo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    redo->setIcon(style->standardIcon(QStyle::SP_ArrowForward));
    edit->addAction(undo);
    edit->addAction(redo);
    edit->addSeparator();
    planned(edit, tr("Cu&t"), QKeySequence::Cut, QStringLiteral("SN-112"));
    planned(edit, tr("&Copy"), QKeySequence::Copy, QStringLiteral("SN-112"));
    planned(edit, tr("&Paste"), QKeySequence::Paste, QStringLiteral("SN-112"));
    planned(edit, tr("&Delete"), QKeySequence::Delete, QStringLiteral("SN-112"));
    edit->addSeparator();
    planned(edit, tr("&Rotate"), QKeySequence(Qt::CTRL | Qt::Key_R), QStringLiteral("SN-112"));
    planned(edit, tr("&Mirror"), QKeySequence(Qt::CTRL | Qt::Key_E), QStringLiteral("SN-112"));

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
    auto* zoom_in = view->addAction(tr("Zoom &In"), view_, &SchematicView::zoom_in);
    zoom_in->setShortcut(QKeySequence::ZoomIn);
    auto* zoom_out = view->addAction(tr("Zoom &Out"), view_, &SchematicView::zoom_out);
    zoom_out->setShortcut(QKeySequence::ZoomOut);
    auto* fit = view->addAction(tr("&Fit Circuit"), view_, &SchematicView::fit_content);
    fit->setShortcuts({QKeySequence(Qt::Key_F), QKeySequence(Qt::Key_Home)});
    view->addSeparator();
    planned(view, tr("Show &Voltages"), QKeySequence(), QStringLiteral("SN-119"));

    auto* place = menuBar()->addMenu(tr("&Place"));
    auto* wire = planned(place, tr("&Wire"), QKeySequence(Qt::Key_W), QStringLiteral("SN-113"));
    planned(place, tr("&Net Label"), QKeySequence(Qt::Key_N), QStringLiteral("SN-113"));
    planned(place, tr("&Ground"), QKeySequence(Qt::Key_G), QStringLiteral("SN-113"));
    auto* probe = planned(place, tr("P&robe"), QKeySequence(Qt::Key_P), QStringLiteral("SN-120"));
    place->addSeparator();
    planned(place, tr("Resistor"), QKeySequence(Qt::Key_R), QStringLiteral("SN-112"));
    planned(place, tr("Capacitor"), QKeySequence(Qt::Key_C), QStringLiteral("SN-112"));
    planned(place, tr("Inductor"), QKeySequence(Qt::Key_L), QStringLiteral("SN-112"));
    planned(place, tr("Diode"), QKeySequence(Qt::Key_D), QStringLiteral("SN-112"));

    auto* simulate = menuBar()->addMenu(tr("&Simulate"));
    auto* run = planned(simulate, tr("&Run"), QKeySequence(Qt::Key_F5), QStringLiteral("SN-118"));
    run->setIcon(style->standardIcon(QStyle::SP_MediaPlay));
    auto* pause = planned(simulate, tr("&Pause / Resume"), QKeySequence(Qt::Key_F6), QStringLiteral("SN-118"));
    pause->setIcon(style->standardIcon(QStyle::SP_MediaPause));
    auto* stop = planned(simulate, tr("&Stop"), QKeySequence(Qt::SHIFT | Qt::Key_F5), QStringLiteral("SN-118"));
    stop->setIcon(style->standardIcon(QStyle::SP_MediaStop));
    planned(simulate, tr("R&eset"), QKeySequence(), QStringLiteral("SN-118"));
    simulate->addSeparator();
    planned(simulate, tr("&Transient Settings..."), QKeySequence(), QStringLiteral("SN-121"));

    auto* tools = menuBar()->addMenu(tr("&Tools"));
    engine_action_ = tools->addAction(tr("Check Simulation &Engine"), this, &MainWindow::check_engine);
    engine_action_->setStatusTip(tr("Load the analog engine and solve a reference circuit"));

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
    help->addAction(tr("&About SimNodus"), this, &MainWindow::show_about);
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);

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
    toolbar->addAction(run);
    toolbar->addAction(pause);
    toolbar->addAction(stop);
    auto* speed = new QComboBox;
    speed->setObjectName(QStringLiteral("speedFactor"));
    speed->addItems({QStringLiteral("0.001x"), QStringLiteral("0.01x"), QStringLiteral("0.1x"),
        QStringLiteral("1.00x"), tr("Maximum")});
    speed->setCurrentIndex(3);
    speed->setEnabled(false);
    speed->setToolTip(tr("Simulation speed relative to real time (SN-118)"));
    toolbar->addWidget(speed);
    toolbar->addSeparator();
    toolbar->addAction(analyzer_action_);
}

void MainWindow::build_status_bar()
{
    state_label_ = new QLabel(tr("Stopped"));
    time_label_ = new QLabel(tr("t = -"));
    speed_label_ = new QLabel(tr("- x real time"));
    mode_label_ = new QLabel(tr("Interactive"));
    warnings_label_ = new QLabel;
    cursor_label_ = new QLabel;
    zoom_label_ = new QLabel(tr("%1 %").arg(qRound(view_->zoom() * 100)));
    for (auto* label : {state_label_, time_label_, speed_label_, mode_label_, warnings_label_}) {
        statusBar()->addWidget(label);
    }
    statusBar()->addPermanentWidget(cursor_label_);
    statusBar()->addPermanentWidget(zoom_label_);
    update_counts(0, 0);
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

bool MainWindow::engine_check_running() const
{
    return engine_thread_ && engine_thread_->isRunning();
}

void MainWindow::check_engine()
{
    if (engine_check_running()) {
        return;
    }
    engine_action_->setEnabled(false);
    messages_->add(Severity::Info, tr("Checking the analog engine..."), tr("Engine"));
    auto result = std::make_shared<sim::EngineCheckResult>();
    const std::filesystem::path app_dir = QCoreApplication::applicationDirPath().toStdWString();
    // The check loads a native library and runs a solve: never on the GUI thread.
    engine_thread_ = QThread::create([result, app_dir] { *result = sim::check_analog_engine(app_dir); });
    connect(engine_thread_, &QThread::finished, this, [this, result] {
        engine_action_->setEnabled(true);
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

void MainWindow::show_about()
{
    QMessageBox::about(this, tr("About SimNodus"),
        tr("<h3>SimNodus %1</h3>"
           "<p>Interactive circuit and STM32 simulator for teaching.</p>"
           "<p>This is a development build of the rebuilt application: the editor, the continuous "
           "simulation and the Signal Analyzer arrive in release 0.1.</p>"
           "<p>MIT License. Analog engine: ngspice (BSD-3-Clause), loaded at run time. "
           "Qt %2 (LGPL-3.0).</p>")
            .arg(QStringLiteral(SIMNODUS_VERSION_FULL), QString::fromLatin1(qVersion())));
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
        const int size = settings_.value(QStringLiteral("panels/%1/size").arg(panel_key(panel)), remembered_[static_cast<std::size_t>(i)]).toInt();
        remembered_[static_cast<std::size_t>(i)] = std::clamp(size, 60, 2000);
    }
    columns_->setSizes({remembered_[0], std::max(center_minimum, width() - remembered_[0] - remembered_[1]), remembered_[1]});
    rows_->setSizes({std::max(200, height() - remembered_[2]), remembered_[2]});
    for (int i = 0; i < 3; ++i) {
        const Panel panel = static_cast<Panel>(i);
        const bool fallback = panel != Panel::Messages;
        panel_widget(panel)->setVisible(settings_.value(QStringLiteral("panels/%1/visible").arg(panel_key(panel)), fallback).toBool());
    }
    analyzer_->restore_settings(settings_);
}

void MainWindow::show_restored_windows()
{
    if (settings_.value(QStringLiteral("analyzer/visible"), false).toBool()) {
        analyzer_->show();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    save_settings();
    analyzer_->hide();
    QMainWindow::closeEvent(event);
}

} // namespace simnodus::ui
