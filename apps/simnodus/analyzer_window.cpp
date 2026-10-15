// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "analyzer_window.hpp"

#include "logic_widget.hpp"
#include "uart_terminal.hpp"

#include "app_settings.hpp"
#include "run_controller.hpp"
#include "theme.hpp"

#include "model/engineering.hpp"
#include "sim/measurements.hpp"

#include <QHBoxLayout>
#include <QListWidget>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace simnodus::ui {
namespace {

const double timebase_steps[] = {1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3,
    1e-2, 2e-2, 5e-2, 0.1, 0.2, 0.5, 1.0};
const double scale_steps[] = {1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3, 1e-2, 2e-2,
    5e-2, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100};

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

QString quantity(double value, const QString& unit)
{
    return QString::fromStdString(model::format_engineering(value, 4)) + unit;
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
    auto* auto_action = toolbar->addAction(tr("Autoscale"), this, &AnalyzerWindow::autoscale);
    auto_action->setToolTip(tr("Fit every channel's scale to its signal"));
    cursors_ = toolbar->addAction(tr("Cursors"));
    cursors_->setCheckable(true);
    toolbar->addSeparator();
    toolbar->addAction(tr("Export CSV..."), this, &AnalyzerWindow::export_csv_dialog);
    toolbar->addAction(tr("Export PNG..."), this, &AnalyzerWindow::export_png_dialog);

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
    measurements_ = new QLabel(tr("Add a probe on the schematic and run the simulation."));
    measurements_->setObjectName(QStringLiteral("measurementRow"));
    measurements_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    measurements_->setWordWrap(true);
    scope_layout->addWidget(measurements_);
    tabs_->addTab(scope_page, tr("Scope"));
    tabs_->addTab(build_logic_page(), tr("Logic"));
    terminal_ = new UartTerminal;
    terminal_->setObjectName(QStringLiteral("uartTerminal"));
    tabs_->addTab(terminal_, tr("UART"));
    splitter->addWidget(tabs_);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 720});
    setCentralWidget(splitter);

    connect(display_run_, &QAction::toggled, this, [this](bool stopped) {
        scope_->set_display_running(!stopped);
        display_run_->setText(stopped ? tr("Run display") : tr("Stop display"));
        if (!stopped) {
            single_done_ = false;
        }
    });
    connect(cursors_, &QAction::toggled, scope_, &ScopeWidget::set_cursors_enabled);
    connect(scope_, &ScopeWidget::cursors_changed, this, &AnalyzerWindow::update_measurements);
    connect(scope_, &ScopeWidget::window_changed, this, [this] { update_measurements(); });
    connect(timebase_, &QComboBox::currentIndexChanged, this, [this](int index) {
        scope_->set_seconds_per_division(timebase_->itemData(index).toDouble());
        refresh();
    });
    timer_.setInterval(33); // about 30 Hz (NF-03)
    connect(&timer_, &QTimer::timeout, this, &AnalyzerWindow::refresh);
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
    connect(channels_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item) {
        const int index = channels_->indexOfTopLevelItem(item);
        if (index >= 0 && static_cast<std::size_t>(index) < visible_.size()) {
            visible_[static_cast<std::size_t>(index)] = item->checkState(0) == Qt::Checked;
            QTimer::singleShot(0, this, &AnalyzerWindow::refresh); // refresh may rebuild this list
        }
    });
    connect(channels_, &QTreeWidget::currentItemChanged, this, [this] { update_measurements(); });

    layout->addWidget(theme::make_caption(tr("Timebase")));
    auto* timebase_form = new QFormLayout;
    timebase_form->setContentsMargins(6, 0, 6, 0);
    timebase_ = new QComboBox;
    timebase_->setObjectName(QStringLiteral("timebase"));
    for (double step : timebase_steps) {
        timebase_->addItem(tr("%1/div").arg(format_seconds(step)), step);
    }
    timebase_->setCurrentIndex(15); // 100 ms/div suits interactive runs
    timebase_form->addRow(tr("Time"), timebase_);
    mode_ = new QComboBox;
    mode_->addItems({tr("Roll"), tr("Triggered")});
    mode_->setObjectName(QStringLiteral("scopeMode"));
    timebase_form->addRow(tr("Mode"), mode_);
    layout->addLayout(timebase_form);

    layout->addWidget(theme::make_caption(tr("Trigger")));
    auto* trigger_form = new QFormLayout;
    trigger_form->setContentsMargins(6, 0, 6, 6);
    trigger_source_ = new QComboBox;
    trigger_form->addRow(tr("Source"), trigger_source_);
    trigger_level_ = new QDoubleSpinBox;
    trigger_level_->setRange(-1e6, 1e6);
    trigger_level_->setDecimals(4);
    trigger_level_->setValue(1.0);
    trigger_form->addRow(tr("Level"), trigger_level_);
    trigger_slope_ = new QComboBox;
    trigger_slope_->addItems({tr("Rising"), tr("Falling")});
    trigger_form->addRow(tr("Slope"), trigger_slope_);
    trigger_sweep_ = new QComboBox;
    trigger_sweep_->addItems({tr("Auto"), tr("Normal"), tr("Single")});
    trigger_form->addRow(tr("Sweep"), trigger_sweep_);
    layout->addLayout(trigger_form);
    connect(trigger_sweep_, &QComboBox::currentIndexChanged, this, [this] { single_done_ = false; });

    history_ = new QLabel;
    history_->setWordWrap(true);
    history_->setContentsMargins(6, 0, 6, 6);
    QPalette muted = history_->palette();
    muted.setColor(QPalette::WindowText, theme::muted_text);
    history_->setPalette(muted);
    layout->addWidget(history_);
    return panel;
}

void AnalyzerWindow::set_controller(RunController* controller)
{
    controller_ = controller;
    terminal_->set_controller(controller);
    reload();
}

void AnalyzerWindow::rebuild_channel_list()
{
    const QSignalBlocker blocker(channels_);
    channels_->clear();
    trigger_source_->clear();
    const auto& channels = controller_ ? controller_->channels() : std::vector<Channel>{};
    channel_count_ = channels.size();
    autoscale_pending_ = true;
    scales_.assign(channels.size(), 1.0);
    offsets_.assign(channels.size(), 0.0);
    visible_.assign(channels.size(), true);
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto& ch = channels[i];
        auto* item = new QTreeWidgetItem(channels_, {ch.name, QString()});
        item->setCheckState(0, Qt::Checked);
        QPixmap swatch(12, 12);
        swatch.fill(ch.color);
        item->setIcon(0, QIcon(swatch));
        if (!ch.valid) {
            item->setToolTip(0, tr("The probed item no longer exists."));
            item->setForeground(0, QColor(0xb4, 0x23, 0x18));
        }
        auto* scale = new QComboBox;
        for (double s : scale_steps) {
            scale->addItem(quantity(s, ch.unit) + tr("/div"), s);
        }
        scale->setCurrentIndex(scale->findData(1.0));
        connect(scale, &QComboBox::activated, this, [this, i, scale](int) {
            if (i < scales_.size()) {
                scales_[i] = scale->currentData().toDouble();
                QTimer::singleShot(0, this, &AnalyzerWindow::refresh); // refresh may delete this box
            }
        });
        channels_->setItemWidget(item, 1, scale);
        trigger_source_->addItem(ch.name);
    }
    if (channels_->topLevelItemCount() > 0) {
        channels_->setCurrentItem(channels_->topLevelItem(0));
    }
    channels_->resizeColumnToContents(0);
}

void AnalyzerWindow::reload()
{
    terminal_->reload();
    rebuild_channel_list();
    record_mode_ = false;
    single_done_ = false;
    scope_->set_navigation_enabled(false);
    if (!controller_) {
        scope_->set_traces({});
        return;
    }
    const auto status = controller_->status();
    history_->setText(tr("Retained history: the last 200 000 samples of each channel (about %1 at the current "
                         "time step).")
                          .arg(format_seconds(200000 * 1e-4)));
    if (controller_->transient_mode() && status.state == sim::RunState::Finished) {
        // Complete record with zoom and pan (AN-06).
        record_mode_ = true;
        scope_->set_navigation_enabled(true);
        const auto result = controller_->transient_result();
        if (!result.time.empty()) {
            scope_->set_window(0.0, result.time.back());
        }
        autoscale_pending_ = false;
        refresh();
        autoscale();
    }
}

std::vector<ScopeTrace> AnalyzerWindow::collect(double from, double to) const
{
    std::vector<ScopeTrace> traces;
    if (!controller_) {
        return traces;
    }
    const auto& channels = controller_->channels();
    sim::TransientResult record;
    if (record_mode_) {
        record = controller_->transient_result();
    }
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto& ch = channels[i];
        ScopeTrace t;
        t.name = ch.name;
        t.unit = ch.unit;
        t.color = ch.color;
        t.per_division = i < scales_.size() ? scales_[i] : 1.0;
        t.offset = i < offsets_.size() ? offsets_[i] : 0.0;
        t.visible = i < visible_.size() ? visible_[i] : true;
        if (record_mode_) {
            const auto v = record.vectors.find(ch.vector);
            const auto r = record.vectors.find(ch.reference);
            if (v != record.vectors.end()) {
                t.samples.reserve(record.time.size());
                for (std::size_t k = 0; k < record.time.size() && k < v->second.size(); ++k) {
                    const double ref = r != record.vectors.end() && k < r->second.size() ? r->second[k] : 0.0;
                    t.samples.push_back({record.time[k], v->second[k] - ref});
                }
            }
        } else {
            t.samples = controller_->samples(ch, from);
            while (!t.samples.empty() && t.samples.back().time > to + 1e-12 && t.samples.size() > 1) {
                t.samples.pop_back();
            }
        }
        traces.push_back(std::move(t));
    }
    return traces;
}

QWidget* AnalyzerWindow::build_logic_page()
{
    auto* page = new QWidget;
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(4, 4, 4, 4);
    auto* side = new QVBoxLayout;
    lane_list_ = new QListWidget;
    lane_list_->setObjectName(QStringLiteral("logicLanes"));
    lane_list_->setToolTip(tr("Signals shown as lanes; microcontroller pins appear by themselves when they change"));
    lane_list_->setMaximumWidth(180);
    side->addWidget(lane_list_, 1);
    auto* threshold_row = new QHBoxLayout;
    threshold_row->addWidget(new QLabel(tr("Probe threshold")));
    threshold_ = new QDoubleSpinBox;
    threshold_->setObjectName(QStringLiteral("logicThreshold"));
    threshold_->setRange(-100, 100);
    threshold_->setDecimals(2);
    threshold_->setSingleStep(0.1);
    threshold_->setValue(1.65);
    threshold_->setSuffix(QStringLiteral(" V"));
    threshold_->setToolTip(tr("Voltage channels read 1 above this level"));
    threshold_row->addWidget(threshold_);
    side->addLayout(threshold_row);
    row->addLayout(side);
    auto* main = new QVBoxLayout;
    logic_ = new LogicWidget;
    logic_->setObjectName(QStringLiteral("logicView"));
    main->addWidget(logic_, 1);
    transitions_ = new QLabel;
    transitions_->setObjectName(QStringLiteral("logicTransitions"));
    transitions_->setWordWrap(true);
    transitions_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    main->addWidget(transitions_);
    row->addLayout(main, 1);
    connect(lane_list_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        lane_choice_[item->text()] = item->checkState() == Qt::Checked;
        QTimer::singleShot(0, this, &AnalyzerWindow::refresh); // refresh may rebuild this list
    });
    connect(logic_, &LogicWidget::lane_selected, this, [this](int) { update_transitions(); });
    return page;
}

void AnalyzerWindow::refresh()
{
    if (!controller_ || !isVisible()) {
        return;
    }
    terminal_->refresh();
    refresh_scope();
    refresh_logic();
}

void AnalyzerWindow::refresh_logic()
{
    const auto lanes = controller_->logic_lanes();
    if (lanes.size() != logic_lanes_.size()
        || !std::equal(lanes.begin(), lanes.end(), logic_lanes_.begin(),
            [](const auto& a, const auto& b) { return a.title == b.title; })) {
        logic_lanes_ = lanes;
        const QSignalBlocker blocker(lane_list_);
        lane_list_->clear();
        for (const auto& lane : logic_lanes_) {
            auto* item = new QListWidgetItem(lane.title, lane_list_);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            const auto choice = lane_choice_.find(lane.title);
            item->setCheckState(choice != lane_choice_.end() && choice->second ? Qt::Checked : Qt::Unchecked);
        }
    }
    const double from = scope_->window_start();
    std::vector<LogicLaneView> views;
    for (int i = 0; i < static_cast<int>(logic_lanes_.size()); ++i) {
        const auto& lane = logic_lanes_[static_cast<std::size_t>(i)];
        auto* item = lane_list_->item(i);
        const bool chosen = lane_choice_.count(lane.title) > 0;
        if (!item || (item->checkState() != Qt::Checked && chosen)) {
            continue;
        }
        auto lane_data = controller_->lane_data(lane, from, threshold_->value());
        if (item->checkState() != Qt::Checked) {
            // Not chosen by the user yet: board pins appear once they change.
            if (lane.channel >= 0 || (lane_data.changes.empty() && !auto_shown_.count(lane.title))) {
                continue;
            }
            auto_shown_.insert(lane.title);
            const QSignalBlocker blocker(lane_list_);
            item->setCheckState(Qt::Checked);
        }
        views.push_back({lane.title, lane.color, lane_data.initial, std::move(lane_data.changes)});
    }
    logic_->set_lanes(std::move(views));
    logic_->set_window(scope_->window_start(), scope_->window_end());
    logic_->set_cursors(scope_->cursors_enabled(), scope_->cursor(0), scope_->cursor(1));
    update_transitions();
}

void AnalyzerWindow::update_transitions()
{
    const int lane = logic_->selected();
    if (lane < 0) {
        transitions_->setText(tr("Click a lane to list its transitions in the window."));
        return;
    }
    const auto changes = logic_->visible_transitions(lane);
    QStringList parts;
    const std::size_t first = changes.size() > 12 ? changes.size() - 12 : 0;
    for (std::size_t i = first; i < changes.size(); ++i) {
        parts << QStringLiteral("%1 %2s").arg(QChar(changes[i].second ? 0x2191 : 0x2193), // up / down arrow
            QString::fromStdString(model::format_engineering(changes[i].first, 7)));
    }
    transitions_->setText(tr("%1: %2 transition(s) in the window%3")
                              .arg(logic_->lanes()[static_cast<std::size_t>(lane)].title)
                              .arg(changes.size())
                              .arg(parts.isEmpty() ? QString() : QStringLiteral(": ") + parts.join(QStringLiteral("  "))));
}

void AnalyzerWindow::refresh_scope()
{
    if (controller_->channels().size() != channel_count_) {
        rebuild_channel_list();
    }
    if (record_mode_) {
        scope_->set_traces(collect(scope_->window_start(), scope_->window_end()));
        update_measurements();
        return;
    }
    if (display_run_->isChecked()) {
        return; // display stopped; acquisition continues
    }
    const auto status = controller_->status();
    const double now = status.sim_time;
    const double div = timebase_->currentData().toDouble();
    const double span = div * ScopeWidget::horizontal_divisions;
    auto traces = collect(now - 2.5 * span, now);
    double start = std::max(0.0, now - span);
    bool show = true;
    if (mode_->currentIndex() == 1 && !traces.empty()) {
        // Triggered: newest crossing that still has the full post-trigger part.
        const int source = std::clamp(trigger_source_->currentIndex(), 0, static_cast<int>(traces.size()) - 1);
        const auto found = sim::find_trigger(traces[static_cast<std::size_t>(source)].samples, trigger_level_->value(),
            trigger_slope_->currentIndex() == 0, now - 2.4 * span, now - 0.9 * span);
        const int sweep = trigger_sweep_->currentIndex();
        if (found) {
            start = *found - div;
            if (sweep == 2) {
                if (single_done_) {
                    show = false;
                } else {
                    single_done_ = true;
                    display_run_->setChecked(true); // freeze after one acquisition
                }
            }
        } else if (sweep != 0) {
            show = false; // normal/single: keep the last triggered frame
        }
        const double units_per_pixel = scales_.empty()
            ? 1.0
            : scales_[static_cast<std::size_t>(source)] * ScopeWidget::vertical_divisions / scope_->plot_rect().height();
        const double offset = offsets_.empty() ? 0.0 : offsets_[static_cast<std::size_t>(source)];
        scope_->set_trigger_marker(scope_->plot_rect().center().y() - (trigger_level_->value() - offset) / units_per_pixel,
            controller_->channels()[static_cast<std::size_t>(source)].color);
    } else {
        scope_->set_trigger_marker(std::nullopt, {});
    }
    if (show) {
        scope_->set_window(start, start + span);
        const bool has_data = std::any_of(traces.begin(), traces.end(), [](const ScopeTrace& t) { return t.samples.size() > 10; });
        scope_->set_traces(std::move(traces));
        if (autoscale_pending_ && has_data && now > span * 0.5) {
            autoscale_pending_ = false;
            autoscale();
            return;
        }
    }
    update_measurements();
}

void AnalyzerWindow::autoscale()
{
    const auto traces = collect(scope_->window_start(), scope_->window_end());
    for (std::size_t i = 0; i < traces.size() && i < scales_.size(); ++i) {
        const auto m = sim::measure(traces[i].samples, scope_->window_start(), scope_->window_end());
        if (!m) {
            continue;
        }
        const double range = std::max(m->peak_to_peak, std::max(std::abs(m->max), 1e-6) * 0.2);
        scales_[i] = nice_division(range / 6.0);
        offsets_[i] = std::round(0.5 * (m->max + m->min) / scales_[i]) * scales_[i];
        if (auto* box = qobject_cast<QComboBox*>(channels_->itemWidget(channels_->topLevelItem(static_cast<int>(i)), 1))) {
            const int index = box->findData(scales_[i]);
            if (index >= 0) {
                box->setCurrentIndex(index);
            }
        }
    }
    refresh();
}

void AnalyzerWindow::update_measurements()
{
    if (!controller_ || scope_->traces().empty()) {
        measurements_->setText(controller_ && !controller_->channels().empty()
                ? tr("Waiting for data.")
                : tr("Add a probe on the schematic and run the simulation."));
        return;
    }
    const int index = std::max(0, channels_->indexOfTopLevelItem(channels_->currentItem()));
    const auto& traces = scope_->traces();
    const auto& t = traces[std::min(static_cast<std::size_t>(index), traces.size() - 1)];
    QStringList parts;
    if (const auto m = sim::measure(t.samples, scope_->window_start(), scope_->window_end())) {
        parts << QStringLiteral("<b>%1</b>").arg(t.name.toHtmlEscaped())
              << tr("min %1").arg(quantity(m->min, t.unit)) << tr("max %1").arg(quantity(m->max, t.unit))
              << tr("mean %1").arg(quantity(m->mean, t.unit)) << tr("RMS %1").arg(quantity(m->rms, t.unit))
              << tr("p-p %1").arg(quantity(m->peak_to_peak, t.unit));
        if (m->frequency) {
            parts << tr("f %1").arg(quantity(*m->frequency, QStringLiteral("Hz")));
        }
        if (m->duty) {
            parts << tr("duty %1 %").arg(QString::number(*m->duty * 100, 'f', 1));
        }
    }
    QString text = parts.join(QStringLiteral("&nbsp;&nbsp;"));
    if (scope_->cursors_enabled()) {
        const double a = scope_->cursor(0);
        const double b = scope_->cursor(1);
        const double dt = b - a;
        QString cursor = tr("A %1  B %2  Δt %3").arg(format_seconds(a), format_seconds(b), format_seconds(dt));
        if (std::abs(dt) > 0) {
            cursor += tr("  1/Δt %1").arg(quantity(1.0 / std::abs(dt), QStringLiteral("Hz")));
        }
        const auto va = sim::value_at(t.samples, a);
        const auto vb = sim::value_at(t.samples, b);
        if (va && vb) {
            cursor += tr("  %1: %2 / %3  Δ %4")
                          .arg(t.name, quantity(*va, t.unit), quantity(*vb, t.unit), quantity(*vb - *va, t.unit));
        }
        text += QStringLiteral("<br>") + cursor.toHtmlEscaped();
    }
    measurements_->setText(text);
}

bool AnalyzerWindow::export_csv(const QString& path, QString& error) const
{
    const auto& traces = scope_->traces();
    std::vector<const ScopeTrace*> visible;
    for (const auto& t : traces) {
        if (t.visible && !t.samples.empty()) {
            visible.push_back(&t);
        }
    }
    if (visible.empty()) {
        error = tr("There is no visible channel with data.");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << "# SimNodus " << QCoreApplication::applicationVersion() << "; "
        << (controller_ ? controller_->mode_description() : QString()) << "\n";
    out << "time_s";
    for (const auto* t : visible) {
        out << "," << t->name << "_" << t->unit;
    }
    out << "\n";
    out.setRealNumberPrecision(12);
    // Rows at the first channel's sample times; all channels share the
    // engine's accepted time points, others are interpolated if needed.
    for (const auto& s : visible.front()->samples) {
        if (s.time < scope_->window_start() || s.time > scope_->window_end()) {
            continue;
        }
        out << QString::number(s.time, 'g', 12);
        for (const auto* t : visible) {
            const auto v = sim::value_at(t->samples, s.time);
            out << "," << (v ? QString::number(*v, 'g', 12) : QString());
        }
        out << "\n";
    }
    return true;
}

void AnalyzerWindow::export_csv_dialog()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export channels"), tr("signals.csv"), tr("CSV files (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!export_csv(path, error)) {
        QMessageBox::warning(this, tr("Export channels"), error);
    }
}

void AnalyzerWindow::export_png_dialog()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export plot"), tr("scope.png"), tr("PNG images (*.png)"));
    if (!path.isEmpty() && !scope_->grab().save(path)) {
        QMessageBox::warning(this, tr("Export plot"), tr("The image could not be written."));
    }
}

void AnalyzerWindow::save_settings(QSettings& settings) const
{
    settings.setValue(QStringLiteral("analyzer/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("analyzer/state"), saveState());
    settings.setValue(QStringLiteral("analyzer/visible"), isVisible());
    settings.setValue(QStringLiteral("analyzer/timebase"), timebase_->currentData());
    settings.setValue(QStringLiteral("analyzer/tab"), tabs_->currentIndex());
    settings.setValue(QStringLiteral("analyzer/mode"), mode_->currentIndex());
}

void AnalyzerWindow::restore_settings(const QSettings& settings)
{
    restore_window_geometry(*this, settings.value(QStringLiteral("analyzer/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("analyzer/state")).toByteArray());
    const double step = settings.value(QStringLiteral("analyzer/timebase"), 0.1).toDouble();
    const int index = timebase_->findData(step);
    if (index >= 0) {
        timebase_->setCurrentIndex(index);
    }
    tabs_->setCurrentIndex(std::clamp(settings.value(QStringLiteral("analyzer/tab"), 0).toInt(), 0, tabs_->count() - 1));
    mode_->setCurrentIndex(std::clamp(settings.value(QStringLiteral("analyzer/mode"), 0).toInt(), 0, 1));
}

void AnalyzerWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    timer_.start();
    refresh();
    emit visibility_changed(true);
}

void AnalyzerWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    timer_.stop();
    emit visibility_changed(false);
}

} // namespace simnodus::ui
