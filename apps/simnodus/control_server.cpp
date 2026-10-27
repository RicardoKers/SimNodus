// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "control_server.hpp"

#include "document.hpp"
#include "main_window.hpp"
#include "run_controller.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"

#include "analyzer_window.hpp"
#include "io/project_file.hpp"
#include "model/engineering.hpp"

#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsItem>
#include <QLabel>
#include <QTabWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStatusBar>
#include <QTimer>
#include <QUndoStack>

#include <algorithm>
#include <array>
#include <optional>
#include <set>

namespace simnodus::ui {

using automation::Error;
using automation::json;

namespace {

std::filesystem::path to_path(const QString& text)
{
    return std::filesystem::path(text.toStdWString());
}

QString from_path(const std::filesystem::path& path)
{
    return QDir::fromNativeSeparators(QString::fromStdWString(path.wstring()));
}

// Four numbers from a request ([a, b, c, d]), or nothing when the key is absent.
std::optional<std::array<double, 4>> four_numbers(const json& args, const char* key)
{
    const auto it = args.find(key);
    if (it == args.end() || it->is_null()) {
        return std::nullopt;
    }
    if (!it->is_array() || it->size() != 4
        || !std::all_of(it->begin(), it->end(), [](const json& v) { return v.is_number(); })) {
        throw Error{"invalid-argument", std::string("Argument \"") + key + "\" must be four numbers."};
    }
    return std::array<double, 4>{(*it)[0].get<double>(), (*it)[1].get<double>(), (*it)[2].get<double>(),
        (*it)[3].get<double>()};
}

// A number from a request, given as a number or as text.
double number(const json& args, const char* key, double fallback)
{
    const auto it = args.find(key);
    if (it == args.end() || it->is_null()) {
        return fallback;
    }
    if (it->is_number()) {
        return it->get<double>();
    }
    if (it->is_string()) {
        bool ok = false;
        const double value = QString::fromStdString(it->get<std::string>()).toDouble(&ok);
        if (ok) {
            return value;
        }
    }
    throw Error{"invalid-argument", std::string("Argument \"") + key + "\" must be a number."};
}

// A quantity from a request: a number, or text with an SI prefix ("2m", "500u").
double quantity(const json& value, const std::string& key, std::string_view unit)
{
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        if (const auto parsed = model::parse_engineering(value.get<std::string>(), unit)) {
            return *parsed;
        }
    }
    throw Error{"invalid-argument", "Argument \"" + key + "\" must be a number, or text such as \"2m\"."};
}

// The position of `text` among `options`.
int choice(const std::string& text, std::initializer_list<const char*> options, const std::string& key)
{
    int index = 0;
    std::string list;
    for (const char* option : options) {
        if (text == option) {
            return index;
        }
        list += (list.empty() ? "" : ", ") + std::string(option);
        ++index;
    }
    throw Error{"invalid-argument", "Argument \"" + key + "\" must be one of: " + list + "."};
}

// Only component parameters change: a running simulation can take them (SN-207).
bool parameters_only(const model::Edit& edit)
{
    if (edit.changes().empty()) {
        return false;
    }
    for (const auto& change : edit.changes()) {
        if (!change.before || !change.after) {
            return false;
        }
        const auto* before = std::get_if<model::Component>(&*change.before);
        const auto* after = std::get_if<model::Component>(&*change.after);
        if (!before || !after) {
            return false;
        }
        auto same = *before;
        same.parameters = after->parameters;
        if (!(same == *after)) {
            return false;
        }
    }
    return true;
}

} // namespace

// The window as an automation workspace: the document with its undo stack,
// the run controller's session, the canvas for highlights.
class ControlWorkspace : public automation::Workspace {
public:
    explicit ControlWorkspace(ControlServer& server) : server_(server), window_(server.window_) {}

    const model::Schematic& schematic() const override { return document().schematic(); }
    model::Schematic& id_source() override { return window_.document()->id_source(); }
    const library::ComponentLibrary& library() const override { return document().library(); }

    void apply(model::Edit edit) override
    {
        auto& run = *window_.run_controller();
        if (run.active() && !parameters_only(edit)) {
            run.stop(); // as the window does before a change of the circuit's structure
        }
        std::set<model::Id> touched;
        for (const auto& change : edit.changes()) {
            if (change.after) {
                touched.insert(change.id);
            }
        }
        window_.document()->apply(std::move(edit));
        if (run.active()) {
            run.apply_live_parameters();
        }
        highlight(touched);
        paced();
    }

    bool undo() override { return step(true); }
    bool redo() override { return step(false); }

    std::filesystem::path file() const override { return to_path(document().file_path()); }
    std::filesystem::path base_dir() const override
    {
        const QString base = document().base_dir();
        return to_path(base.isEmpty() ? QDir::currentPath() : base);
    }
    bool modified() const override { return document().modified(); }

    void new_project(bool discard) override
    {
        refuse_unsaved(discard);
        const bool automatic = window_.auto_discard_;
        window_.auto_discard_ = true;
        window_.new_file();
        window_.auto_discard_ = automatic;
    }

    std::vector<std::string> open_project(const std::filesystem::path& file, bool discard) override
    {
        refuse_unsaved(discard);
        // Read first, so that the agent gets the reasons a file cannot be opened.
        const auto loaded = io::load_project(file);
        if (!loaded.schematic) {
            std::string why;
            for (const auto& e : loaded.errors) {
                why += (why.empty() ? "" : " ") + e;
            }
            const auto text = file.u8string();
            throw Error{"io-error", "Cannot open " + std::string(text.begin(), text.end()) + ": " + why};
        }
        const bool automatic = window_.auto_discard_;
        window_.auto_discard_ = true;
        const bool opened = window_.open_file(from_path(file));
        window_.auto_discard_ = automatic;
        if (!opened) {
            throw Error{"io-error", "The window could not open the file; see its Messages panel."};
        }
        return loaded.warnings;
    }

    void save_project(const std::filesystem::path& file) override
    {
        const QString path = from_path(file);
        QString error;
        if (!window_.document()->save(path, error)) {
            throw Error{"io-error", error.toStdString()};
        }
        window_.add_recent(path);
        QFile::remove(window_.recovery_file());
        window_.statusBar()->showMessage(
            MainWindow::tr("Saved %1").arg(QDir::toNativeSeparators(path)), 4000);
    }

    sim::Session& session() override { return window_.run_controller()->session(); }

    bool start_run(sim::RunRequest request, std::string& reason) override
    {
        server_.starting_ = true;
        QString error;
        const bool started = window_.start_run(&request, error);
        server_.starting_ = false;
        if (!started) {
            reason = error.toStdString();
        }
        return started;
    }

    void stop_run() override { window_.run_controller()->stop(); }

    void wait(std::chrono::milliseconds time) override
    {
        check_cancelled();
        QEventLoop loop;
        QTimer::singleShot(static_cast<int>(time.count()), &loop, &QEventLoop::quit);
        QEventLoop* outer = waiting_;
        waiting_ = &loop;
        loop.exec();
        waiting_ = outer;
        check_cancelled();
    }

    // Ends the current wait at once with error "cancelled".
    void cancel()
    {
        cancelled = true;
        if (waiting_) {
            waiting_->quit();
        }
    }

    bool watched() const override { return true; }

    // Selects the elements on the canvas and scrolls them into view.
    void highlight(const std::set<model::Id>& ids)
    {
        auto* scene = window_.scene();
        scene->select_ids(ids);
        QRectF area;
        for (const auto* item : scene->selectedItems()) {
            area = area.united(item->sceneBoundingRect());
        }
        if (!area.isNull()) {
            window_.schematic()->ensureVisible(area, 60, 60);
        }
    }

    void paced()
    {
        if (server_.pace_ > 0) {
            wait(std::chrono::milliseconds(server_.pace_));
        }
    }

    bool cancelled = false;

private:
    QEventLoop* waiting_ = nullptr;

    const Document& document() const { return *window_.document(); }

    void check_cancelled() const
    {
        if (cancelled) {
            throw Error{"cancelled", "The SimNodus window is closing or stopped external control."};
        }
    }

    void refuse_unsaved(bool discard) const
    {
        if (document().modified() && !discard) {
            throw Error{"blocked",
                "The window's project has unsaved changes; save them with project.save, or pass \"discard\": true."};
        }
    }

    bool step(bool back)
    {
        auto* stack = window_.document()->undo_stack();
        if (back ? !stack->canUndo() : !stack->canRedo()) {
            return false;
        }
        window_.run_controller()->stop();
        back ? stack->undo() : stack->redo();
        paced();
        return true;
    }

    ControlServer& server_;
    MainWindow& window_;
};

ControlServer::ControlServer(MainWindow& window)
    : QObject(&window), window_(window), workspace_(std::make_unique<ControlWorkspace>(*this))
{
    // A run the user starts replaces the agents' runs.
    connect(window_.run_controller(), &RunController::started, this, [this] {
        if (!starting_) {
            forget_runs();
        }
    });
}

ControlServer::~ControlServer()
{
    blockSignals(true); // the window is being destroyed
    close();
    drop_gone();
}

QString ControlServer::default_name()
{
    const QString given = qEnvironmentVariable("SIMNODUS_CONTROL_NAME");
    if (!given.isEmpty()) {
        return given;
    }
#ifdef Q_OS_WIN
    const QString user = qEnvironmentVariable("USERNAME");
#else
    const QString user = qEnvironmentVariable("USER");
#endif
    // As simnodus-cli builds it: letters, digits, '-' and '_' only.
    QString name = QStringLiteral("simnodus-control-");
    for (const QChar c : user) {
        const bool plain = c.unicode() < 128 && (c.isLetterOrNumber() || c == u'-' || c == u'_');
        name += plain ? c : u'_';
    }
    return name;
}

bool ControlServer::listen(QString& error, const QString& name)
{
    if (listening()) {
        return true;
    }
    // Another window may own the endpoint; a socket file left by a crash may not.
    {
        QLocalSocket probe;
        probe.connectToServer(name);
        if (probe.waitForConnected(300)) {
            error = tr("Another SimNodus window already accepts external control.");
            return false;
        }
    }
    QLocalServer::removeServer(name);
    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(name)) {
        error = tr("External control could not start: %1").arg(server_->errorString());
        delete server_;
        server_ = nullptr;
        return false;
    }
    connect(server_, &QLocalServer::newConnection, this, &ControlServer::accept);
    emit status_changed();
    return true;
}

void ControlServer::close()
{
    if (server_) {
        server_->close();
        delete server_;
        server_ = nullptr;
    }
    for (auto& client : clients_) {
        client->gone = true;
        if (client->socket) {
            client->socket->disconnect(this);
            client->socket->disconnectFromServer();
        }
    }
    queue_.clear();
    if (busy_) {
        cancel(); // the running command ends; process() then drops the clients
    } else {
        drop_gone();
    }
    emit status_changed();
}

bool ControlServer::listening() const
{
    return server_ && server_->isListening();
}

QString ControlServer::endpoint() const
{
    return server_ ? server_->fullServerName() : QString();
}

int ControlServer::clients() const
{
    return static_cast<int>(std::count_if(clients_.begin(), clients_.end(), [](const auto& c) { return !c->gone; }));
}

void ControlServer::cancel()
{
    if (busy_) {
        workspace_->cancel();
    }
}

json ControlServer::execute(const json& request)
{
    if (!local_) {
        local_ = make_automation();
    }
    return local_->execute(request);
}

void ControlServer::accept()
{
    while (server_ && server_->hasPendingConnections()) {
        auto client = std::make_unique<Client>();
        client->socket = server_->nextPendingConnection();
        client->automation = make_automation();
        Client* raw = client.get();
        connect(raw->socket, &QLocalSocket::readyRead, this, [this, raw] { read(raw); });
        connect(raw->socket, &QLocalSocket::disconnected, this, [this, raw] {
            raw->gone = true;
            if (!busy_) {
                drop_gone();
            }
            emit status_changed();
        });
        clients_.push_back(std::move(client));
        emit status_changed();
    }
}

void ControlServer::read(Client* client)
{
    if (client->gone || !client->socket) {
        return;
    }
    client->buffer += client->socket->readAll();
    for (qsizetype end = client->buffer.indexOf('\n'); end >= 0; end = client->buffer.indexOf('\n')) {
        QByteArray line = client->buffer.left(end).trimmed();
        client->buffer.remove(0, end + 1);
        if (!line.isEmpty() && !line.startsWith('#')) {
            queue_.emplace_back(client, std::move(line));
        }
    }
    if (client->buffer.size() > 16 * 1024 * 1024) {
        client->buffer.clear(); // not a client of this protocol
        client->socket->disconnectFromServer();
    }
    // Later, from the event loop: a command may wait, and requests arriving
    // meanwhile only join the queue.
    QTimer::singleShot(0, this, &ControlServer::process);
}

void ControlServer::process()
{
    if (busy_) {
        return;
    }
    busy_ = true;
    while (!queue_.empty()) {
        auto [client, line] = std::move(queue_.front());
        queue_.pop_front();
        if (client->gone) {
            continue;
        }
        const auto reply = client->automation->execute_line(line.toStdString());
        if (!client->gone && client->socket) {
            const auto text = reply.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
            client->socket->write(text.data(), static_cast<qint64>(text.size()));
            client->socket->flush();
        }
    }
    busy_ = false;
    workspace_->cancelled = false; // a cancellation ends only the command that was waiting
    drop_gone();
}

void ControlServer::drop_gone()
{
    for (auto it = clients_.begin(); it != clients_.end();) {
        if ((*it)->gone) {
            if ((*it)->socket) {
                (*it)->socket->disconnect(this);
                (*it)->socket->deleteLater();
            }
            const Client* raw = it->get();
            std::erase_if(queue_, [raw](const auto& entry) { return entry.first == raw; });
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

void ControlServer::forget_runs()
{
    for (auto& client : clients_) {
        client->automation->forget_run();
    }
    if (local_) {
        local_->forget_run();
    }
}

std::unique_ptr<automation::Automation> ControlServer::make_automation()
{
    auto a = std::make_unique<automation::Automation>(*workspace_);
    a->extend({"view.fit",
        "Zooms the canvas to show the whole circuit, or a grid area (to frame a circuit before it is built), "
        "optionally in a part of the canvas only (to keep room for the Signal Analyzer in a recording).",
        json{{"area", "[x1, y1, x2, y2] grid points, optional"},
            {"region", "[left, top, width, height] fractions of the canvas, optional: [0, 0, 0.55, 1] = left 55%"}},
        [this](const json& args) {
            const auto area = four_numbers(args, "area");
            const auto region = four_numbers(args, "region");
            auto* view = window_.schematic();
            if (!area && !region) {
                view->fit_content();
                return json::object();
            }
            QRectF target = view->content_rect();
            if (area) {
                const auto& [x1, y1, x2, y2] = *area;
                const double g = SchematicView::grid;
                target = QRectF(QPointF(std::min(x1, x2) * g, std::min(y1, y2) * g),
                    QPointF(std::max(x1, x2) * g, std::max(y1, y2) * g));
                if (target.width() <= 0 || target.height() <= 0) {
                    throw Error{"invalid-argument", "Argument \"area\" must have a width and a height."};
                }
            }
            QRectF fraction(0.0, 0.0, 1.0, 1.0);
            if (region) {
                const auto& [left, top, width, height] = *region;
                fraction = QRectF(left, top, width, height);
                if (left < 0 || top < 0 || width < 0.1 || height < 0.1 || left + width > 1.0001
                    || top + height > 1.0001) {
                    throw Error{"invalid-argument",
                        "Argument \"region\" must lie inside the canvas: fractions from 0 to 1, at least 0.1 wide and high."};
                }
            }
            view->fit_area(target, fraction);
            return json{{"zoom", view->zoom()}};
        }});
    a->extend({"view.show", "Selects components (by designator) or other elements (by ID) and scrolls them into view.",
        json{{"items", "[\"R1\", 12]"}}, [this](const json& args) {
            const auto it = args.find("items");
            if (it == args.end() || !it->is_array()) {
                throw Error{"invalid-argument", "Argument \"items\" must be a list of designators or IDs."};
            }
            const auto& schematic = window_.document()->schematic();
            std::set<model::Id> ids;
            for (const auto& ref : *it) {
                if (ref.is_number_integer() && schematic.contains(ref.get<model::Id>())) {
                    ids.insert(ref.get<model::Id>());
                } else if (const auto* c = ref.is_string() ? schematic.component_by_designator(ref.get<std::string>())
                                                           : nullptr) {
                    ids.insert(c->id);
                } else {
                    throw Error{"not-found", "No element " + ref.dump() + "."};
                }
            }
            workspace_->highlight(ids);
            return json{{"selected", ids.size()}};
        }});
    a->extend({"view.pace", "Milliseconds the window waits after each edit, so that a recording can follow; 0 for none.",
        json{{"ms", "600"}}, [this](const json& args) {
            pace_ = static_cast<int>(std::clamp(number(args, "ms", 0.0), 0.0, 10000.0));
            return json{{"ms", pace_}};
        }});
    a->extend({"view.wait", "Waits while the window stays live (narration, recordings).", json{{"ms", "1000"}},
        [this](const json& args) {
            const auto ms = static_cast<int>(std::clamp(number(args, "ms", 0.0), 0.0, 60000.0));
            workspace_->wait(std::chrono::milliseconds(ms));
            return json{{"ms", ms}};
        }});
    a->extend({"view.caption", "Shows a caption at the bottom of the canvas; empty text hides it.",
        json{{"text", "Now we connect the capacitor."}, {"seconds", "0 = until replaced"}},
        [this](const json& args) {
            const auto it = args.find("text");
            if (it != args.end() && !it->is_string() && !it->is_null()) {
                throw Error{"invalid-argument", "Argument \"text\" must be text."};
            }
            const QString text = it == args.end() || it->is_null() ? QString() : QString::fromStdString(it->get<std::string>());
            const double seconds = std::clamp(number(args, "seconds", 0.0), 0.0, 3600.0);
            show_caption(text, static_cast<int>(seconds * 1000));
            return json{{"shown", !text.isEmpty()}};
        }});
    a->extend({"view.analyzer", "Shows or hides the Signal Analyzer window.", json{{"show", "true"}},
        [this](const json& args) {
            if (args.value("show", true)) {
                window_.show_analyzer();
            } else {
                window_.analyzer()->hide();
            }
            return json{{"shown", window_.analyzer()->isVisible()}};
        }});
    a->extend({"analyzer.set",
        "Changes the Signal Analyzer's settings (all optional) and answers with all of them: the tab, the timebase, "
        "roll or triggered mode, the trigger, cursors, the selected channel, per channel visibility, scale and "
        "offset, the Logic tab's lanes and threshold, the side panel, a stopped display and the time window of a "
        "paused run.",
        json{{"tab", "scope | logic | uart"}, {"timebase", "\"1m\" (seconds per division)"},
            {"mode", "roll | triggered"},
            {"trigger", "{\"source\": \"V(OUT)\", \"level\": 2.5, \"slope\": \"rising\", \"sweep\": \"auto | normal | single\"}"},
            {"cursors", "false, true or [a, b] in seconds"}, {"select", "\"V(OUT)\""},
            {"channels", "{\"V(OUT)\": {\"visible\": true, \"scale\": \"2 or auto\", \"offset\": 0}}"},
            {"lanes", "[\"U1.PA5\", \"V(OUT)\"] or \"auto\""}, {"threshold", "1.65"}, {"panel", "true"},
            {"display", "run | stop"}, {"window", "[from, to] seconds, when paused or stopped"}},
        [this](const json& args) { return analyzer_set(args); }});
    a->extend({"analyzer.autoscale", "Fits every channel's scale to its signal (the Autoscale button); the scales then "
                                     "follow a signal that leaves the screen.",
        json::object(), [this](const json&) {
            window_.analyzer()->autoscale();
            return analyzer_state();
        }});
    return a;
}

json ControlServer::analyzer_state() const
{
    const auto* analyzer = window_.analyzer();
    static const char* const tabs[] = {"scope", "logic", "uart"};
    static const char* const sweeps[] = {"auto", "normal", "single"};
    const auto views = analyzer->channel_views();
    json channels = json::array();
    for (const auto& view : views) {
        channels.push_back({{"name", view.name.toStdString()}, {"unit", view.unit.toStdString()},
            {"visible", view.visible}, {"scale", view.per_division}, {"auto", view.automatic},
            {"offset", view.offset}});
    }
    const auto trigger = analyzer->trigger();
    const int selected = analyzer->selected_channel();
    const auto names = [](const QStringList& list) {
        json out = json::array();
        for (const auto& item : list) {
            out.push_back(item.toStdString());
        }
        return out;
    };
    const auto* scope = analyzer->scope();
    return json{{"shown", analyzer->isVisible()}, {"tab", tabs[std::clamp(analyzer->tabs()->currentIndex(), 0, 2)]},
        {"timebase", analyzer->timebase_value()}, {"mode", analyzer->triggered() ? "triggered" : "roll"},
        {"trigger",
            {{"source", trigger.source >= 0 && static_cast<std::size_t>(trigger.source) < views.size()
                        ? json(views[static_cast<std::size_t>(trigger.source)].name.toStdString())
                        : json()},
                {"level", trigger.level}, {"slope", trigger.rising ? "rising" : "falling"},
                {"sweep", sweeps[std::clamp(trigger.sweep, 0, 2)]}}},
        {"cursors", {{"on", scope->cursors_enabled()}, {"a", scope->cursor(0)}, {"b", scope->cursor(1)}}},
        {"select", selected >= 0 && static_cast<std::size_t>(selected) < views.size()
                ? json(views[static_cast<std::size_t>(selected)].name.toStdString())
                : json()},
        {"channels", channels}, {"lanes", names(analyzer->shown_lanes())},
        {"available_lanes", names(analyzer->available_lanes())}, {"threshold", analyzer->threshold()},
        {"panel", analyzer->controls_visible()}, {"display", analyzer->display_stopped() ? "stop" : "run"},
        {"window", {scope->window_start(), scope->window_end()}}, {"navigable", analyzer->navigable()}};
}

json ControlServer::analyzer_set(const json& args)
{
    auto* analyzer = window_.analyzer();
    // Read every argument first: a wrong one changes nothing.
    const auto text = [&args](const char* key) -> std::optional<std::string> {
        const auto it = args.find(key);
        if (it == args.end() || it->is_null()) {
            return std::nullopt;
        }
        if (!it->is_string()) {
            throw Error{"invalid-argument", std::string("Argument \"") + key + "\" must be text."};
        }
        return it->get<std::string>();
    };
    const auto flag = [&args](const char* key) -> std::optional<bool> {
        const auto it = args.find(key);
        if (it == args.end() || it->is_null()) {
            return std::nullopt;
        }
        if (!it->is_boolean()) {
            throw Error{"invalid-argument", std::string("Argument \"") + key + "\" must be true or false."};
        }
        return it->get<bool>();
    };
    const auto channel = [analyzer](const json& name, const std::string& key) {
        const int index = name.is_string() ? analyzer->channel_index(QString::fromStdString(name.get<std::string>())) : -1;
        if (index < 0) {
            std::string known;
            for (const auto& view : analyzer->channel_views()) {
                known += (known.empty() ? "" : ", ") + view.name.toStdString();
            }
            throw Error{"not-found", "No channel " + name.dump() + " in \"" + key + "\"; "
                    + (known.empty() ? "the channels appear when a run with probes starts." : "channels: " + known + ".")};
        }
        return index;
    };

    std::optional<int> tab;
    if (const auto t = text("tab")) {
        tab = choice(*t, {"scope", "logic", "uart"}, "tab");
    }
    std::optional<double> timebase;
    if (const auto it = args.find("timebase"); it != args.end() && !it->is_null()) {
        timebase = quantity(*it, "timebase", "s");
        if (*timebase <= 0) {
            throw Error{"invalid-argument", "Argument \"timebase\" must be more than 0."};
        }
    }
    std::optional<bool> triggered;
    if (const auto m = text("mode")) {
        triggered = choice(*m, {"roll", "triggered"}, "mode") == 1;
    }
    int source = -1;
    int sweep = -1;
    std::optional<double> level;
    std::optional<bool> rising;
    if (const auto it = args.find("trigger"); it != args.end() && !it->is_null()) {
        if (!it->is_object()) {
            throw Error{"invalid-argument", "Argument \"trigger\" must be an object."};
        }
        if (it->contains("source")) {
            source = channel((*it)["source"], "trigger");
        }
        if (it->contains("level")) {
            level = quantity((*it)["level"], "trigger.level", "V");
        }
        if (it->contains("slope")) {
            rising = choice((*it)["slope"].is_string() ? (*it)["slope"].get<std::string>() : std::string(),
                         {"rising", "falling"}, "trigger.slope")
                == 0;
        }
        if (it->contains("sweep")) {
            sweep = choice((*it)["sweep"].is_string() ? (*it)["sweep"].get<std::string>() : std::string(),
                {"auto", "normal", "single"}, "trigger.sweep");
        }
    }
    std::optional<bool> cursors;
    std::optional<double> cursor_a;
    std::optional<double> cursor_b;
    if (const auto it = args.find("cursors"); it != args.end() && !it->is_null()) {
        if (it->is_boolean()) {
            cursors = it->get<bool>();
        } else if (it->is_array() && it->size() == 2) {
            cursors = true;
            cursor_a = quantity((*it)[0], "cursors", "s");
            cursor_b = quantity((*it)[1], "cursors", "s");
        } else {
            throw Error{"invalid-argument", "Argument \"cursors\" must be true, false or [a, b] in seconds."};
        }
    }
    std::optional<int> select;
    if (const auto it = args.find("select"); it != args.end() && !it->is_null()) {
        select = channel(*it, "select");
    }
    struct ChannelChange {
        int index = -1;
        std::optional<bool> visible;
        bool scale_given = false;
        std::optional<double> scale; // nullopt with scale_given: automatic
        std::optional<double> offset;
    };
    std::vector<ChannelChange> changes;
    if (const auto it = args.find("channels"); it != args.end() && !it->is_null()) {
        if (!it->is_object()) {
            throw Error{"invalid-argument", "Argument \"channels\" must map channel names to settings."};
        }
        for (const auto& [name, settings] : it->items()) {
            ChannelChange change;
            change.index = channel(json(name), "channels");
            if (!settings.is_object()) {
                throw Error{"invalid-argument", "The settings of channel \"" + name + "\" must be an object."};
            }
            if (settings.contains("visible")) {
                if (!settings["visible"].is_boolean()) {
                    throw Error{"invalid-argument", "\"visible\" of channel \"" + name + "\" must be true or false."};
                }
                change.visible = settings["visible"].get<bool>();
            }
            if (settings.contains("scale")) {
                change.scale_given = true;
                const auto& scale = settings["scale"];
                if (!(scale.is_string() && scale.get<std::string>() == "auto")) {
                    change.scale = quantity(scale, "scale", "");
                    if (*change.scale <= 0) {
                        throw Error{"invalid-argument", "\"scale\" of channel \"" + name + "\" must be more than 0."};
                    }
                }
            }
            if (settings.contains("offset")) {
                change.offset = quantity(settings["offset"], "offset", "");
            }
            changes.push_back(change);
        }
    }
    std::optional<QStringList> lanes;
    bool automatic_lanes = false;
    if (const auto it = args.find("lanes"); it != args.end() && !it->is_null()) {
        if (it->is_string() && it->get<std::string>() == "auto") {
            automatic_lanes = true;
        } else if (it->is_array() && std::all_of(it->begin(), it->end(), [](const json& v) { return v.is_string(); })) {
            lanes = QStringList();
            for (const auto& lane : *it) {
                lanes->append(QString::fromStdString(lane.get<std::string>()));
            }
        } else {
            throw Error{"invalid-argument", "Argument \"lanes\" must be a list of lane names or \"auto\"."};
        }
    }
    std::optional<double> threshold;
    if (const auto it = args.find("threshold"); it != args.end() && !it->is_null()) {
        threshold = quantity(*it, "threshold", "V");
    }
    const auto panel = flag("panel");
    std::optional<bool> stopped;
    if (const auto d = text("display")) {
        stopped = choice(*d, {"run", "stop"}, "display") == 1;
    }
    const auto window = args.find("window");
    std::optional<std::pair<double, double>> span;
    if (window != args.end() && !window->is_null()) {
        if (!window->is_array() || window->size() != 2) {
            throw Error{"invalid-argument", "Argument \"window\" must be [from, to] in seconds."};
        }
        span = std::make_pair(quantity((*window)[0], "window", "s"), quantity((*window)[1], "window", "s"));
        if (!(span->second > span->first)) {
            throw Error{"invalid-argument", "Argument \"window\" must end after it starts."};
        }
    }

    if (tab) {
        analyzer->set_tab(*tab);
    }
    if (timebase) {
        analyzer->set_timebase(*timebase);
    }
    if (triggered) {
        analyzer->set_triggered(*triggered);
    }
    analyzer->set_trigger(source, level, rising, sweep);
    for (const auto& change : changes) {
        if (change.visible) {
            analyzer->set_channel_visible(change.index, *change.visible);
        }
        if (change.scale_given) {
            analyzer->set_channel_scale(change.index, change.scale);
        }
        if (change.offset) {
            analyzer->set_channel_offset(change.index, *change.offset);
        }
    }
    if (select) {
        analyzer->select_channel(*select);
    }
    if (threshold) {
        analyzer->set_threshold(*threshold);
    }
    if (automatic_lanes) {
        analyzer->set_automatic_lanes();
    } else if (lanes) {
        analyzer->set_lanes(*lanes);
    }
    if (panel) {
        analyzer->set_controls_visible(*panel);
    }
    if (stopped) {
        analyzer->set_display_stopped(*stopped);
    }
    if (span) {
        if (!analyzer->navigable()) {
            throw Error{"blocked", "The time window can change only while the run is paused, the display is stopped "
                                   "or a transient record is shown."};
        }
        analyzer->set_time_window(span->first, span->second);
    }
    if (cursors) {
        analyzer->set_cursor_positions(*cursors, cursor_a, cursor_b); // after the window they may lie in
    }
    analyzer->refresh();
    return analyzer_state();
}

void ControlServer::show_caption(const QString& text, int ms)
{
    auto* viewport = window_.schematic()->viewport();
    if (!caption_) {
        caption_ = new QLabel(viewport);
        caption_->setObjectName(QStringLiteral("controlCaption"));
        caption_->setWordWrap(true);
        caption_->setAlignment(Qt::AlignCenter);
        caption_->setAttribute(Qt::WA_TransparentForMouseEvents);
        caption_->setStyleSheet(QStringLiteral(
            "QLabel { background: rgba(20, 20, 20, 200); color: white; padding: 10px 18px; border-radius: 8px; }"));
        QFont font = caption_->font();
        font.setPointSizeF(font.pointSizeF() * 1.5);
        caption_->setFont(font);
        viewport->installEventFilter(this);
    }
    ++caption_serial_;
    if (text.isEmpty()) {
        caption_->hide();
        return;
    }
    caption_->setText(text);
    place_caption();
    caption_->show();
    caption_->raise();
    if (ms > 0) {
        const int serial = caption_serial_;
        QTimer::singleShot(ms, caption_, [this, serial] {
            if (serial == caption_serial_) {
                caption_->hide();
            }
        });
    }
}

void ControlServer::place_caption()
{
    if (!caption_) {
        return;
    }
    const QSize area = caption_->parentWidget()->size();
    const int widest = std::max(160, area.width() * 4 / 5);
    const int text = caption_->fontMetrics().horizontalAdvance(caption_->text()) + 40; // and the padding
    caption_->setFixedWidth(std::min(widest, text));
    caption_->setFixedHeight(caption_->heightForWidth(caption_->width()));
    caption_->move((area.width() - caption_->width()) / 2, area.height() - caption_->height() - 24);
}

bool ControlServer::eventFilter(QObject* watched, QEvent* event)
{
    if (caption_ && watched == caption_->parentWidget() && event->type() == QEvent::Resize) {
        place_caption();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace simnodus::ui
