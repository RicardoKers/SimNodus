// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Signal Analyzer (AN-01..AN-06, AN-09): an independent top-level window
// that can live on another monitor. Closing it only hides it; acquisition
// is not affected. Interactive runs scroll (roll mode) or trigger like a
// bench oscilloscope; transient results open as a complete record with
// zoom and pan.
#pragma once

#include "run_controller.hpp"
#include "scope_widget.hpp"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>

#include <map>
#include <optional>
#include <set>

class QAction;
class QComboBox;
class QListWidget;
class QDoubleSpinBox;
class QLabel;
class QSettings;
class QTabWidget;
class QTreeWidget;

namespace simnodus::ui {

class RunController;
class UartTerminal;
class LogicWidget;

class AnalyzerWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit AnalyzerWindow(QWidget* parent = nullptr);

    void set_controller(RunController* controller);
    ScopeWidget* scope() const { return scope_; }
    QTabWidget* tabs() const { return tabs_; }
    UartTerminal* terminal() const { return terminal_; }
    LogicWidget* logic() const { return logic_; }
    QListWidget* lane_list() const { return lane_list_; }
    QLabel* transitions() const { return transitions_; }
    QComboBox* timebase() const { return timebase_; }
    QLabel* measurement_row() const { return measurements_; }

    void save_settings(QSettings& settings) const;
    void restore_settings(const QSettings& settings);

    // Rebuilds the channel list and, for transient results, the record.
    void reload();
    bool export_csv(const QString& path, QString& error) const;
    // Lanes checked in the Logic tab, over the retained history (AN-10).
    bool export_vcd(const QString& path, QString& error) const;

public slots:
    void autoscale();
    void refresh();

signals:
    void visibility_changed(bool visible);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* build_controls();
    QWidget* build_logic_page();
    void refresh_scope();
    void refresh_logic();
    void update_transitions();
    void rebuild_channel_list();
    // Channels within [from, to]; with `bucket` (seconds), thinned for display.
    std::vector<ScopeTrace> collect(double from, double to, double bucket = 0.0) const;
    // Seconds per pixel column of the plot for a window of `span` seconds.
    double pixel_time(double span) const;
    void update_measurements();
    void export_csv_dialog();
    void export_vcd_dialog();
    void export_png_dialog();

    RunController* controller_ = nullptr;
    ScopeWidget* scope_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    UartTerminal* terminal_ = nullptr;
    LogicWidget* logic_ = nullptr;
    QListWidget* lane_list_ = nullptr;
    QDoubleSpinBox* threshold_ = nullptr;
    QLabel* transitions_ = nullptr;
    std::vector<RunController::LogicLane> logic_lanes_;
    std::map<QString, bool> lane_choice_; // lanes the user checked or unchecked
    std::set<QString> auto_shown_;        // pin lanes shown because they changed
    QComboBox* timebase_ = nullptr;
    QComboBox* mode_ = nullptr;
    QComboBox* trigger_source_ = nullptr;
    QComboBox* trigger_slope_ = nullptr;
    QComboBox* trigger_sweep_ = nullptr;
    QDoubleSpinBox* trigger_level_ = nullptr;
    QTreeWidget* channels_ = nullptr;
    QLabel* measurements_ = nullptr;
    QLabel* history_ = nullptr;
    QAction* display_run_ = nullptr;
    QAction* cursors_ = nullptr;
    QTimer timer_;
    std::vector<double> scales_;  // per channel volts or amperes per division
    std::vector<double> offsets_; // per channel center value
    std::vector<bool> visible_;
    std::size_t channel_count_ = 0;
    bool record_mode_ = false;    // complete transient record shown
    bool single_done_ = false;
    bool autoscale_pending_ = true; // fit scales once the first data arrives
    std::optional<std::pair<double, double>> last_window_;
    sim::TransientResult record_;  // the complete transient record, read once per reload
    QElapsedTimer measured_;       // measurements of full data run a few times a second
    bool measurements_due_ = true;
};

} // namespace simnodus::ui
