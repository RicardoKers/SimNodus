// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "analyzer_window.hpp"

#include "app_settings.hpp"
#include "scope_widget.hpp"
#include "theme.hpp"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace simnodus::ui {
namespace {

const double timebase_steps[] = {1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3,
    1e-2, 2e-2, 5e-2, 0.1, 0.2, 0.5, 1.0};

QWidget* placeholder(const QString& text)
{
    auto* label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, theme::muted_text);
    label->setPalette(palette);
    return label;
}

} // namespace

AnalyzerWindow::AnalyzerWindow(QWidget* parent) : QMainWindow(parent)
{
    setObjectName(QStringLiteral("analyzerWindow"));
    setWindowTitle(tr("Signal Analyzer - SimNodus"));
    // Closing the main window ends the application even if this one is open.
    setAttribute(Qt::WA_QuitOnClose, false);
    resize(960, 600);

    auto* toolbar = addToolBar(tr("Analyzer"));
    toolbar->setObjectName(QStringLiteral("analyzerToolbar"));
    toolbar->setMovable(false);
    display_run_ = toolbar->addAction(tr("Stop display"));
    display_run_->setCheckable(true);
    display_run_->setToolTip(tr("Freeze the display without stopping the simulation"));

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName(QStringLiteral("analyzerSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(build_controls());

    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("analyzerTabs"));
    tabs_->setDocumentMode(true);
    auto* scope_page = new QWidget;
    auto* scope_layout = new QVBoxLayout(scope_page);
    scope_layout->setContentsMargins(4, 4, 4, 4);
    scope_ = new ScopeWidget;
    scope_layout->addWidget(scope_, 1);
    auto* measurements = new QLabel(tr("Cursors and measurements appear here once a channel is added."));
    measurements->setObjectName(QStringLiteral("measurementRow"));
    QPalette muted = measurements->palette();
    muted.setColor(QPalette::WindowText, theme::muted_text);
    measurements->setPalette(muted);
    scope_layout->addWidget(measurements);
    tabs_->addTab(scope_page, tr("Scope"));
    tabs_->addTab(placeholder(tr("Logic lanes for digital signals and MCU pins arrive with the microcontroller "
                                 "in release 0.2.")),
        tr("Logic"));
    tabs_->addTab(placeholder(tr("The UART terminal arrives with the microcontroller in release 0.2.")), tr("UART"));
    splitter->addWidget(tabs_);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 740});
    setCentralWidget(splitter);

    connect(display_run_, &QAction::toggled, this, [this](bool stopped) {
        scope_->set_display_running(!stopped);
        display_run_->setText(stopped ? tr("Run display") : tr("Stop display"));
    });
    connect(timebase_, &QComboBox::currentIndexChanged, this,
        [this](int index) { scope_->set_seconds_per_division(timebase_->itemData(index).toDouble()); });
}

QWidget* AnalyzerWindow::build_controls()
{
    auto* panel = new QWidget;
    panel->setObjectName(QStringLiteral("analyzerControls"));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    layout->addWidget(theme::make_caption(tr("Channels")));
    channels_ = new QTreeWidget;
    channels_->setHeaderLabels({tr("Channel"), tr("Scale")});
    channels_->setRootIsDecorated(false);
    channels_->setFrameShape(QFrame::NoFrame);
    channels_->setObjectName(QStringLiteral("channelList"));
    layout->addWidget(channels_, 1);

    layout->addWidget(theme::make_caption(tr("Timebase")));
    auto* timebase_form = new QFormLayout;
    timebase_form->setContentsMargins(6, 0, 6, 0);
    timebase_ = new QComboBox;
    timebase_->setObjectName(QStringLiteral("timebase"));
    for (double step : timebase_steps) {
        timebase_->addItem(tr("%1/div").arg(format_seconds(step)), step);
    }
    timebase_->setCurrentIndex(9); // 1 ms/div
    timebase_form->addRow(tr("Time"), timebase_);
    auto* mode = new QComboBox;
    mode->addItems({tr("Roll"), tr("Triggered")});
    mode->setEnabled(false);
    mode->setToolTip(tr("Available with the scope (release 0.1)"));
    timebase_form->addRow(tr("Mode"), mode);
    layout->addLayout(timebase_form);

    layout->addWidget(theme::make_caption(tr("Trigger")));
    auto* trigger_form = new QFormLayout;
    trigger_form->setContentsMargins(6, 0, 6, 6);
    auto* trigger = new QComboBox;
    trigger->addItems({tr("Auto"), tr("Normal"), tr("Single")});
    trigger->setEnabled(false);
    trigger->setToolTip(tr("Available with the scope (release 0.1)"));
    trigger_form->addRow(tr("Sweep"), trigger);
    layout->addLayout(trigger_form);
    return panel;
}

void AnalyzerWindow::save_settings(QSettings& settings) const
{
    settings.setValue(QStringLiteral("analyzer/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("analyzer/state"), saveState());
    settings.setValue(QStringLiteral("analyzer/visible"), isVisible());
    settings.setValue(QStringLiteral("analyzer/timebase"), timebase_->currentData());
    settings.setValue(QStringLiteral("analyzer/tab"), tabs_->currentIndex());
}

void AnalyzerWindow::restore_settings(const QSettings& settings)
{
    restore_window_geometry(*this, settings.value(QStringLiteral("analyzer/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("analyzer/state")).toByteArray());
    const double step = settings.value(QStringLiteral("analyzer/timebase"), 1e-3).toDouble();
    const int index = timebase_->findData(step);
    if (index >= 0) {
        timebase_->setCurrentIndex(index);
    }
    tabs_->setCurrentIndex(std::clamp(settings.value(QStringLiteral("analyzer/tab"), 0).toInt(), 0, tabs_->count() - 1));
}

void AnalyzerWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    emit visibility_changed(true);
}

void AnalyzerWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    emit visibility_changed(false);
}

} // namespace simnodus::ui
