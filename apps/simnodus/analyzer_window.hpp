// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Signal Analyzer (AN-01..AN-06, AN-09): an independent top-level window
// that can live on another monitor. Closing it only hides it; acquisition
// is not affected. Interactive runs scroll (roll mode) or trigger like a
// bench oscilloscope; transient results open as a complete record with
// zoom and pan, and so does the retained history of an interactive run
// while it is paused or the display is stopped. External control
// (analyzer.set) reaches every setting through the public functions below.
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

    // Settings for external control (analyzer.set); channels by index in the
    // run controller's channels(), lanes by title ("U1.PA0", "V(OUT)").
    int channel_index(const QString& name) const;
    void set_tab(int index);
    void set_timebase(double seconds_per_division); // the nearest step
    double timebase_value() const;
    void set_triggered(bool triggered);
    bool triggered() const;
    // source: channel index or -1 to keep; sweep 0 auto, 1 normal, 2 single, -1 to keep.
    void set_trigger(int source, std::optional<double> level, std::optional<bool> rising, int sweep);
    struct TriggerView {
        int source = 0;
        double level = 0.0;
        bool rising = true;
        int sweep = 0;
    };
    TriggerView trigger() const;
    void set_cursor_positions(bool enabled, std::optional<double> a, std::optional<double> b);
    void select_channel(int index);
    int selected_channel() const;
    void set_channel_visible(int index, bool visible);
    // nullopt: automatic (Autoscale, then followed when the signal leaves the screen).
    void set_channel_scale(int index, std::optional<double> per_division);
    void set_channel_offset(int index, double offset); // value at the center line
    struct ChannelView {
        QString name;
        QString unit;
        bool visible = true;
        bool automatic = true;
        double per_division = 1.0;
        double offset = 0.0;
    };
    std::vector<ChannelView> channel_views() const;
    // Exactly these lanes are shown in the Logic tab (also before a run starts):
    // board pins no longer appear by themselves until set_automatic_lanes().
    void set_lanes(const QStringList& titles);
    void set_automatic_lanes();
    QStringList shown_lanes() const;
    QStringList available_lanes() const;
    void set_threshold(double volts);
    double threshold() const;
    void set_controls_visible(bool visible); // the side panel with channels, timebase and trigger
    bool controls_visible() const;
    void set_display_stopped(bool stopped);
    bool display_stopped() const;
    // The time axis can be zoomed and panned: a complete record, a paused run or a stopped display.
    bool navigable() const;
    void set_time_window(double start, double end);

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
    // Fits the scales and offsets of the automatic channels to the window's signals.
    void fit_scales();
    // An automatic channel's signal on the screen goes beyond the top or the bottom.
    bool automatic_off_screen() const;
    void show_scale(std::size_t index); // the scale box of a channel shows its scale or Auto
    bool frozen() const; // a paused run or a stopped display: the window stays and can be zoomed
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
    bool lanes_fixed_ = false;            // set_lanes(): no lane appears by itself
    QComboBox* timebase_ = nullptr;
    QComboBox* mode_ = nullptr;
    QComboBox* trigger_source_ = nullptr;
    QComboBox* trigger_slope_ = nullptr;
    QComboBox* trigger_sweep_ = nullptr;
    QDoubleSpinBox* trigger_level_ = nullptr;
    QTreeWidget* channels_ = nullptr;
    QWidget* controls_ = nullptr;
    QLabel* measurements_ = nullptr;
    QLabel* history_ = nullptr;
    QAction* display_run_ = nullptr;
    QAction* cursors_ = nullptr;
    QTimer timer_;
    std::vector<double> scales_;  // per channel volts or amperes per division
    std::vector<double> offsets_; // per channel center value
    std::vector<bool> visible_;
    std::vector<bool> automatic_; // scale fitted by Autoscale and followed while running
    std::vector<std::pair<QString, QString>> channel_names_; // name and unit of each channel listed
    std::size_t channel_count_ = 0;
    bool record_mode_ = false;    // complete transient record shown
    bool single_done_ = false;
    bool autoscale_pending_ = true; // fit scales once the first data arrives
    std::optional<std::pair<double, double>> last_window_;
    std::optional<double> last_trigger_; // newest trigger crossing found
    double trigger_seen_at_ = 0.0;       // simulated time when it was found
    sim::TransientResult record_;  // the complete transient record, read once per reload
    QElapsedTimer measured_;       // measurements of full data run a few times a second
    bool measurements_due_ = true;
};

} // namespace simnodus::ui
