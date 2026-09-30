#include "boundary_fixture.hpp"

#include <QApplication>
#include <QCryptographicHash>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QProcess>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {
QByteArray bytes(std::string_view value)
{
    return QByteArray(value.data(), static_cast<qsizetype>(value.size()));
}

QString digest(const QByteArray& value)
{
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}

class Experiment final : public QObject {
public:
    Experiment(QString worker, QString report)
        : worker_(std::move(worker)), report_(std::move(report))
    {
        editor_.setWindowTitle("SimNodus Circuit Editor - SN-022 fixture");
        analyzer_.setWindowTitle("SimNodus Signal Analyzer - SN-022 fixture");
        editor_.resize(860, 500);
        analyzer_.resize(640, 300);
        editor_.setCentralWidget(new QLabel("Circuit canvas placeholder: no editing or simulation"));
        analyzer_.setCentralWidget(new QLabel("Committed fixture snapshot: unavailable"));
        components_ = new QDockWidget("Components / Preview", &editor_);
        components_->setObjectName("components-preview");
        splitter_ = new QSplitter(Qt::Vertical);
        splitter_->addWidget(new QLabel("Component browser placeholder"));
        splitter_->addWidget(new QLabel("Symbol preview placeholder: no resource execution"));
        components_->setWidget(splitter_);
        editor_.addDockWidget(Qt::LeftDockWidgetArea, components_);
        properties_ = new QDockWidget("Instance properties", &editor_);
        properties_->setObjectName("instance-properties");
        properties_->setWidget(new QLabel("No selected instance"));
        editor_.addDockWidget(Qt::RightDockWidgetArea, properties_);
        auto* view = editor_.menuBar()->addMenu("View");
        view->addAction(components_->toggleViewAction());
        view->addAction(properties_->toggleViewAction());
        editor_.show();
        analyzer_.show();
        elapsed_.start();
        heartbeat_.setInterval(10);
        connect(&heartbeat_, &QTimer::timeout, this, [this] {
            const auto now = elapsed_.elapsed();
            max_gap_ = std::max(max_gap_, now - last_tick_);
            last_tick_ = now;
            ++ticks_;
        });
        heartbeat_.start();
        deadline_.setSingleShot(true);
        deadline_.setTimerType(Qt::PreciseTimer);
        connect(&deadline_, &QTimer::timeout, this, [this] {
            rejection_ = "timeout";
            process_->kill(); // Asynchronous: finished() is still the terminal gate.
        });
        QTimer::singleShot(15000, this, [this] {
            watchdog_ = true;
            if (process_ && process_->state() != QProcess::NotRunning) {
                rejection_ = "watchdog";
                process_->kill();
            } else {
                finish();
            }
            QTimer::singleShot(1000, this, [] { std::_Exit(3); });
        });
        QTimer::singleShot(150, this, [this] { beginLayoutCheck(); });
    }

private:
    void beginLayoutCheck()
    {
        layout_["independent_windows"] = editor_.isWindow() && analyzer_.isWindow() &&
            editor_.isVisible() && analyzer_.isVisible();
        editor_.resizeDocks({components_, properties_}, {220, 180}, Qt::Horizontal);
        splitter_->setSizes({240, 140});
        QTimer::singleShot(100, this, [this] {
            dock_state_ = editor_.saveState(1);
            geometry_ = editor_.saveGeometry();
            splitter_state_ = splitter_->saveState();
            panel_widths_ = {components_->width(), properties_->width()};
            splitter_sizes_ = splitter_->sizes();
            editor_size_ = editor_.size();
            components_->hide();
            properties_->hide();
            layout_["panels_hidden"] = !components_->isVisible() && !properties_->isVisible();
            editor_.resize(920, 540);
            splitter_->setSizes({10, 10});
            layout_["state_restore_returned_true"] = editor_.restoreState(dock_state_, 1) &&
                editor_.restoreGeometry(geometry_) && splitter_->restoreState(splitter_state_);
            QTimer::singleShot(100, this, [this] {
                layout_["panels_reopened"] = components_->isVisible() && properties_->isVisible();
                layout_["panel_widths_restored"] = panel_widths_ ==
                    QList<int>{components_->width(), properties_->width()};
                layout_["splitter_sizes_restored"] = splitter_sizes_ == splitter_->sizes();
                layout_["editor_size_restored"] = editor_size_ == editor_.size();
                next();
            });
        });
    }

    void next()
    {
        if (watchdog_ || index_ == steps_.size()) {
            finish();
            return;
        }
        const auto& step = steps_[index_];
        process_ = std::make_unique<QProcess>();
        output_.clear();
        errors_.clear();
        rejection_.clear();
        terminal_ = false;
        ticks_before_ = ticks_;
        start_ms_ = elapsed_.elapsed();
        snapshot_before_ = committed_;
        pid_ = 0;
        editor_.statusBar()->showMessage("Explicit fixture action: " + step.name);
        connect(process_.get(), &QProcess::started, this, [this] { pid_ = process_->processId(); });
        connect(process_.get(), &QProcess::readyReadStandardOutput, this, [this] { drain(); });
        connect(process_.get(), &QProcess::readyReadStandardError, this, [this] { drain(); });
        connect(process_.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) complete("failed-start", -1, "not-started");
        });
        connect(process_.get(), &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus status) {
                drain();
                const auto outcome = !rejection_.isEmpty() ? rejection_ :
                    status == QProcess::CrashExit ? QString("crash") :
                    code == 0 && output_ == bytes(sn022::complete_record) && errors_.isEmpty() ?
                    QString("success") : QString("rejected-output");
                complete(outcome, code, status == QProcess::CrashExit ? "crash" : "normal");
            });
        process_->setProgram(step.mode == "missing" ? worker_ + ".missing-sn022" : worker_);
        process_->setArguments({step.mode});
        deadline_.start(1200);
        process_->start();
    }

    void drain()
    {
        // Bound accepted records and retained buffers, not QProcess's internal pipe
        // allocation. Production streaming/backpressure remains pending.
        const auto append = [this](QByteArray& target, const QByteArray& incoming) {
            const auto room = static_cast<qsizetype>(sn022::output_limit) - target.size();
            target.append(incoming.first(std::min(room, incoming.size())));
            if (incoming.size() > room) {
                rejection_ = "rejected-output";
                process_->kill();
            }
        };
        append(output_, process_->readAllStandardOutput());
        append(errors_, process_->readAllStandardError());
    }

    void complete(const QString& outcome, int exit_code, const QString& exit_status)
    {
        if (terminal_) return;
        terminal_ = true;
        deadline_.stop();
        const auto& step = steps_[index_];
        if (outcome == "success") {
            committed_ = std::make_shared<const sn022::Snapshot>();
            static_cast<QLabel*>(analyzer_.centralWidget())->setText(
                "Fixture snapshot 41: 1.234567 V (canned data, no backend)");
            editor_.statusBar()->showMessage("Fixture worker completed successfully");
        } else {
            editor_.statusBar()->showMessage("Worker unavailable: " + outcome + "; explicit retry required");
            static_cast<QLabel*>(analyzer_.centralWidget())->setText(committed_ ?
                "Historical fixture snapshot 41: 1.234567 V; worker unavailable" :
                "No committed fixture snapshot; worker unavailable");
        }
        const bool preserved = outcome == "success" ?
            committed_ && *committed_ == sn022::Snapshot{} :
            committed_ && committed_ == snapshot_before_;
        const bool unchanged = document_ == sn022::document;
        const bool responsive = step.mode == "missing" || ticks_ - ticks_before_ >= 2;
        const bool passed = outcome == step.expected && preserved && unchanged && responsive;
        results_.append(QJsonObject{
            {"case", step.name}, {"mode", step.mode}, {"expected", step.expected},
            {"outcome", outcome}, {"exit_code", exit_code}, {"exit_status", exit_status},
            {"process_id", pid_},
            {"elapsed_ms", elapsed_.elapsed() - start_ms_}, {"heartbeat_ticks", ticks_ - ticks_before_},
            {"responsive", responsive}, {"document_unchanged", unchanged},
            {"committed_snapshot_preserved_or_valid", preserved},
            {"stdout_retained_bytes", output_.size()}, {"stderr_retained_bytes", errors_.size()},
            {"stdout_sha256", digest(output_)}, {"passed", passed}});
        passed_ = passed_ && passed;
        ++index_;
        if (index_ == 1) {
            const auto retained = committed_;
            analyzer_.close();
            layout_["analyzer_close_retains_snapshot"] = !analyzer_.isVisible() &&
                committed_ && committed_ == retained;
            analyzer_.show();
            layout_["analyzer_reopened"] = analyzer_.isVisible();
        }
        // Fresh process per case; retry is an explicit test action, never automatic
        // recovery. Destroy the old QProcess after its signal handler returns.
        QTimer::singleShot(30, this, [this] { next(); });
    }

    void finish()
    {
        heartbeat_.stop();
        for (const auto& value : layout_) passed_ = passed_ && value.toBool();
        passed_ = passed_ && !watchdog_ && index_ == steps_.size();
        const QJsonObject report{
            {"experiment", "SN-022 Qt Widgets / worker fixture"},
            {"qt_version", qVersion()}, {"platform", QGuiApplication::platformName()},
            {"style", "Fusion"}, {"worker", QFileInfo(worker_).absoluteFilePath()},
            {"record_limit_bytes", static_cast<qint64>(sn022::output_limit)},
            {"process_deadline_ms", 1200}, {"heartbeat_interval_ms", 10},
            {"heartbeat_max_gap_ms", max_gap_}, {"watchdog_fired", watchdog_},
            {"document_sha256", digest(bytes(document_))},
            {"layout", layout_}, {"cases", results_}, {"passed", passed_}};
        const auto encoded = QJsonDocument(report).toJson(QJsonDocument::Indented);
        bool written = true;
        if (!report_.isEmpty()) {
            QFile file(report_);
            written = file.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
                file.write(encoded) == encoded.size() && file.flush();
            if (!written) std::fprintf(stderr, "Could not create new evidence file.\n");
        }
        std::fwrite(encoded.constData(), 1, static_cast<std::size_t>(encoded.size()), stdout);
        QApplication::exit(passed_ && written ? 0 : 1);
    }

    struct Step { QString name; QString mode; QString expected; };
    const std::vector<Step> steps_{
        {"initial-explicit-run", "success", "success"},
        {"native-crash-after-partial-record", "crash", "crash"},
        {"explicit-fresh-retry-after-crash", "success", "success"},
        {"deadline-after-partial-record", "timeout", "timeout"},
        {"missing-worker", "missing", "failed-start"},
        {"incomplete-clean-exit", "incomplete", "rejected-output"},
        {"trailing-output", "malformed", "rejected-output"},
        {"output-over-limit", "oversized", "rejected-output"},
        {"explicit-fresh-retry-after-failures", "success", "success"}};
    QString worker_, report_, rejection_;
    QMainWindow editor_, analyzer_;
    QDockWidget *components_ = nullptr, *properties_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QTimer heartbeat_, deadline_;
    QElapsedTimer elapsed_;
    std::unique_ptr<QProcess> process_;
    const std::string document_{sn022::document};
    std::shared_ptr<const sn022::Snapshot> committed_, snapshot_before_;
    QByteArray output_, errors_, dock_state_, geometry_, splitter_state_;
    QList<int> panel_widths_, splitter_sizes_;
    QSize editor_size_;
    QJsonObject layout_;
    QJsonArray results_;
    std::size_t index_ = 0;
    qint64 ticks_ = 0, ticks_before_ = 0, start_ms_ = 0, last_tick_ = 0, max_gap_ = 0, pid_ = 0;
    bool terminal_ = false, passed_ = true, watchdog_ = false;
};
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    QApplication::setQuitOnLastWindowClosed(false);
    const auto arguments = app.arguments();
    if (arguments.size() != 5 || arguments[1] != "--worker" || arguments[3] != "--report") {
        std::fprintf(stderr, "Usage: sn022_presentation --worker PATH --report NEW_JSON_PATH\n");
        return 2;
    }
    Experiment experiment(arguments[2], arguments[4]);
    return app.exec();
}
