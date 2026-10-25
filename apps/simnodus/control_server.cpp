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

#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsItem>
#include <QLabel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStatusBar>
#include <QTimer>
#include <QUndoStack>

#include <algorithm>
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
    a->extend({"view.fit", "Zooms the canvas to show the whole circuit.", json::object(), [this](const json&) {
                   window_.schematic()->fit_content();
                   return json::object();
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
    return a;
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
