// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "control_server.hpp"

#include "document.hpp"
#include "main_window.hpp"
#include "run_controller.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"

#include "analyzer_window.hpp"
#include "presenter.hpp"
#include "io/project_file.hpp"
#include "model/engineering.hpp"

#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsItem>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScreen>
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

// A label of the canvas placed at `position` ("top-left", "bottom", "center"...).
void place_label(QLabel* label, const std::string& position)
{
    const QSize area = label->parentWidget()->size();
    const int widest = std::max(160, area.width() * 4 / 5);
    const int text = label->fontMetrics().horizontalAdvance(label->text()) + 2 * label->margin() + 40;
    label->setFixedWidth(std::min(widest, text));
    // A label without word wrap has no height for a width: its own height then.
    const int height = label->wordWrap() ? label->heightForWidth(label->width()) : -1;
    label->setFixedHeight(height > 0 ? height : label->sizeHint().height());
    const int gap = 24;
    const int x_left = gap;
    const int x_right = area.width() - label->width() - gap;
    const int x_middle = (area.width() - label->width()) / 2;
    const int y_top = gap;
    const int y_bottom = area.height() - label->height() - gap;
    const int y_middle = (area.height() - label->height()) / 2;
    int x = x_middle;
    int y = y_bottom;
    if (position == "top") {
        y = y_top;
    } else if (position == "center") {
        y = y_middle;
    } else if (position == "top-left") {
        x = x_left;
        y = y_top;
    } else if (position == "top-right") {
        x = x_right;
        y = y_top;
    } else if (position == "bottom-left") {
        x = x_left;
    } else if (position == "bottom-right") {
        x = x_right;
    }
    label->move(x, y);
}

void check_position(const std::string& position)
{
    static const char* const known[] = {"top", "bottom", "center", "top-left", "top-right", "bottom-left", "bottom-right"};
    if (std::none_of(std::begin(known), std::end(known), [&](const char* k) { return position == k; })) {
        throw Error{"invalid-argument",
            "Argument \"position\" must be top, bottom, center, top-left, top-right, bottom-left or bottom-right."};
    }
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
    : QObject(&window), window_(window), workspace_(std::make_unique<ControlWorkspace>(*this)),
      presenter_(new Presenter(window))
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
            {"region", "[left, top, width, height] fractions of the canvas, optional: [0, 0, 0.55, 1] = left 55%"},
            {"ms", "0; a smooth zoom and pan over this time"}},
        [this](const json& args) {
            const auto area = four_numbers(args, "area");
            const auto region = four_numbers(args, "region");
            const int ms = duration_ms(args, "ms", 0);
            auto* view = window_.schematic();
            if (!area && !region) {
                view->glide_camera([view] { view->fit_content(); }, ms);
                workspace_->wait(std::chrono::milliseconds(ms));
                return json{{"zoom", view->zoom()}};
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
            view->glide_camera([view, target, fraction] { view->fit_area(target, fraction); }, ms);
            workspace_->wait(std::chrono::milliseconds(ms));
            return json{{"zoom", view->zoom()}};
        }});
    a->extend({"view.show", "Selects components (by designator) or other elements (by ID) and scrolls them into view.",
        json{{"items", "[\"R1\", 12]"}, {"ms", "0; a smooth pan over this time"}}, [this](const json& args) {
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
            const int ms = duration_ms(args, "ms", 0);
            window_.schematic()->glide_camera([this, &ids] { workspace_->highlight(ids); }, ms);
            workspace_->wait(std::chrono::milliseconds(ms));
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
    a->extend({"view.caption", "Shows a caption on the canvas (at the bottom unless placed); empty text hides it.",
        json{{"text", "Now we connect the capacitor."}, {"seconds", "0 = until replaced"},
            {"position", "bottom | top | center (or a corner: top-left...)"}, {"size", "1 (a factor)"}},
        [this](const json& args) {
            const auto it = args.find("text");
            if (it != args.end() && !it->is_string() && !it->is_null()) {
                throw Error{"invalid-argument", "Argument \"text\" must be text."};
            }
            const QString text = it == args.end() || it->is_null() ? QString() : QString::fromStdString(it->get<std::string>());
            const double seconds = std::clamp(number(args, "seconds", 0.0), 0.0, 3600.0);
            const std::string position = args.value("position", std::string("bottom"));
            check_position(position);
            caption_position_ = position;
            caption_size_ = std::clamp(number(args, "size", 1.0), 0.3, 5.0);
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
    a->extend({"view.panels",
        "Shows or hides the Components, Properties and Messages panels (Ctrl+1/2/3), or the canvas alone (F11).",
        json{{"components", "true"}, {"properties", "true"}, {"messages", "false"}, {"focus", "false: the canvas alone"}},
        [this](const json& args) {
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
            const auto focus = flag("focus");
            const std::pair<const char*, Panel> panels[] = {
                {"components", Panel::Components}, {"properties", Panel::Properties}, {"messages", Panel::Messages}};
            std::vector<std::pair<Panel, bool>> changes;
            for (const auto& [key, panel] : panels) {
                if (const auto shown = flag(key)) {
                    changes.emplace_back(panel, *shown);
                }
            }
            if (focus) {
                window_.set_focus_canvas(*focus);
            }
            for (const auto& [panel, shown] : changes) {
                window_.set_panel_visible(panel, shown);
            }
            QCoreApplication::processEvents(); // the splitters lay out the canvas again
            return json{{"components", window_.panel_visible(Panel::Components)},
                {"properties", window_.panel_visible(Panel::Properties)},
                {"messages", window_.panel_visible(Panel::Messages)}, {"focus", window_.focus_canvas_active()}};
        }});
    a->extend({"view.font",
        "Scales the interface's text for a recording (a phone shows a 1080-pixel-wide picture); 1 is the normal size. "
        "Icons keep their size; start SimNodus with --scale to scale everything.",
        json{{"scale", "1.4"}},
        [this](const json& args) {
            const double scale = std::clamp(number(args, "scale", 1.0), 0.5, 3.0);
            if (base_font_size_ <= 0) {
                base_font_size_ = QApplication::font().pointSizeF();
            }
            QFont font = QApplication::font();
            font.setPointSizeF(base_font_size_ * scale);
            QApplication::setFont(font);
            QCoreApplication::processEvents();
            return json{{"scale", scale}, {"points", font.pointSizeF()}};
        }});
    a->extend({"view.dialog", "Opens or closes a read-only dialog for a tour: the Keyboard Shortcuts (F1) or About.",
        json{{"name", "shortcuts | about"}, {"show", "true; false closes the open dialog"}},
        [this](const json& args) {
            if (!args.value("show", true)) {
                const bool open = dialog_ && dialog_->isVisible();
                if (open) {
                    dialog_->reject();
                }
                return json{{"closed", open}};
            }
            const std::string name = args.value("name", std::string());
            if (name != "shortcuts" && name != "about") {
                throw Error{"invalid-argument", "Argument \"name\" must be shortcuts or about."};
            }
            if (dialog_ && dialog_->isVisible()) {
                throw Error{"blocked", "A dialog is already open; close it first with {\"show\": false}."};
            }
            // Not modal: a modal dialog's own event loop would hold this command.
            dialog_ = name == "shortcuts" ? window_.shortcuts_dialog() : window_.about_dialog();
            dialog_->setObjectName(QStringLiteral("controlDialog"));
            dialog_->show();
            dialog_->raise();
            workspace_->wait(std::chrono::milliseconds(200));
            return json{{"shown", dialog_ && dialog_->isVisible()}};
        }});
    a->extend({"view.keys", "Shows a key badge (\"Ctrl+R\") in a corner of the canvas for a moment, as a keystroke overlay does.",
        json{{"keys", "Ctrl+R"}, {"seconds", "1.5"}, {"position", "top-left | top-right | bottom-left | bottom-right | top | bottom | center"},
            {"size", "1 (a factor)"}},
        [this](const json& args) {
            const QString keys = QString::fromStdString(args.value("keys", std::string()));
            show_keys(keys, number(args, "seconds", 1.5), args.value("position", std::string("top-right")),
                number(args, "size", 1.0));
            return json{{"shown", !keys.isEmpty()}};
        }});
    a->extend({"view.pointer",
        "Moves a drawn pointer smoothly to a target and can show a click or a double-click; nothing is clicked "
        "(ui.click clicks). Targets: [x, y] grid point, \"R1.2\" pin, \"R1\" part, {\"widget\": \"search\"}, "
        "{\"action\": \"Wire\"}, {\"part\": \"Resistor\"}, {\"field\": \"Resistance\"}, {\"menu\": \"Simulate\"}, "
        "{\"tab\": \"Logic\"}, {\"name\": \"objectName\"}, {\"area\": [x1, y1, x2, y2]}.",
        json{{"to", "target"}, {"ms", "600"}, {"click", "0, 1 or 2"}, {"hide", "true hides the pointer"}},
        [this](const json& args) {
            if (args.value("hide", false)) {
                presenter_->hide_pointer();
                return json{{"shown", false}};
            }
            const int ms = duration_ms(args, "ms", 600);
            if (const auto to = args.find("to"); to != args.end()) {
                presenter_->glide(Presenter::global_point(presenter_->resolve(*to)), ms);
                workspace_->wait(std::chrono::milliseconds(ms));
            } else {
                presenter_->glide(presenter_->pointer_position(), 0);
            }
            const int clicks = static_cast<int>(std::clamp(number(args, "click", 0.0), 0.0, 2.0));
            if (clicks > 0) {
                presenter_->show_click(clicks);
                workspace_->wait(std::chrono::milliseconds(300));
            }
            return pointer_state();
        }});
    a->extend({"view.highlight", "Draws a pulsing outline around a target (a panel, a toolbar button, a part, a grid area) "
                                 "without selecting it; {\"hide\": true} removes it.",
        json{{"target", "{\"widget\": \"components\"}, \"R1\", {\"area\": [x1, y1, x2, y2]}..."}, {"seconds", "2 (0: until replaced)"},
            {"hide", "true"}},
        [this](const json& args) {
            if (args.value("hide", false)) {
                presenter_->hide_highlight();
                return json{{"shown", false}};
            }
            const auto target = args.find("target");
            if (target == args.end()) {
                throw Error{"invalid-argument", "Argument \"target\" is required."};
            }
            const QRect rect = Presenter::global_rect(presenter_->resolve(*target));
            presenter_->highlight(rect, static_cast<int>(std::clamp(number(args, "seconds", 2.0), 0.0, 3600.0) * 1000));
            return json{{"shown", true}, {"rect", {rect.x(), rect.y(), rect.width(), rect.height()}}};
        }});
    a->extend({"view.locate",
        "Screen positions of targets (see view.pointer), in logical pixels and in the screen's physical pixels, "
        "for a recording tool's own overlays or input.",
        json{{"targets", "[\"R1.2\", {\"widget\": \"canvas\"}]"}},
        [this](const json& args) {
            const auto targets = args.find("targets");
            if (targets == args.end() || !targets->is_array()) {
                throw Error{"invalid-argument", "Argument \"targets\" must be a list of targets."};
            }
            json found = json::array();
            for (const auto& target : *targets) {
                const auto t = presenter_->resolve(target);
                const QRect rect = Presenter::global_rect(t);
                const QPoint point = Presenter::global_point(t);
                const QRect device = Presenter::physical(rect);
                const QRect device_point = Presenter::physical(QRect(point, QSize(1, 1)));
                const QScreen* screen = QGuiApplication::screenAt(point);
                found.push_back({{"target", target}, {"rect", {rect.x(), rect.y(), rect.width(), rect.height()}},
                    {"point", {point.x(), point.y()}},
                    {"physical_rect", {device.x(), device.y(), device.width(), device.height()}},
                    {"physical_point", {device_point.x(), device_point.y()}},
                    {"scale", screen ? screen->devicePixelRatio() : 1.0}});
            }
            return json{{"targets", found}};
        }});
    a->extend({"ui.move", "Glides the drawn pointer to a target, moving the real mouse over the widget there (a part "
                          "being placed follows it on the canvas).",
        json{{"to", "target"}, {"ms", "600"}},
        [this](const json& args) {
            const auto to = args.find("to");
            if (to == args.end()) {
                throw Error{"invalid-argument", "Argument \"to\" is required."};
            }
            const auto target = presenter_->resolve(*to, true);
            const int ms = duration_ms(args, "ms", 600);
            presenter_->glide(Presenter::global_point(target), ms, target.widget);
            workspace_->wait(std::chrono::milliseconds(ms + 30));
            return pointer_state();
        }});
    a->extend({"ui.click",
        "Glides the drawn pointer to a target and clicks there through the real widget, as a hand would: a toolbar "
        "button, a Components entry, a menu, a tab, a field, a point of the canvas (places a part, starts or ends "
        "a wire).",
        json{{"target", "target"}, {"ms", "600"}, {"count", "1 or 2"}, {"button", "left | right"}},
        [this](const json& args) {
            const auto it = args.find("target");
            if (it == args.end()) {
                throw Error{"invalid-argument", "Argument \"target\" is required."};
            }
            const auto target = presenter_->resolve(*it, true);
            const int ms = duration_ms(args, "ms", 600);
            const int count = static_cast<int>(std::clamp(number(args, "count", 1.0), 1.0, 2.0));
            const std::string button = args.value("button", std::string("left"));
            if (button != "left" && button != "right") {
                throw Error{"invalid-argument", "Argument \"button\" must be left or right."};
            }
            presenter_->glide(Presenter::global_point(target), ms, target.widget);
            workspace_->wait(std::chrono::milliseconds(ms + 30));
            presenter_->show_click(count);
            // The click happens after the reply: one that opens a dialog must not hold this command.
            click(target, button == "left" ? Qt::LeftButton : Qt::RightButton, count);
            return pointer_state();
        }});
    a->extend({"ui.drag", "Presses at one target, glides the pointer to another with the button held, and releases: "
                          "the real widget sees the drag (a part moved, a wire drawn, a potentiometer turned).",
        json{{"from", "target"}, {"to", "target"}, {"ms", "900"}},
        [this](const json& args) {
            const auto from = args.find("from");
            const auto to = args.find("to");
            if (from == args.end() || to == args.end()) {
                throw Error{"invalid-argument", "Arguments \"from\" and \"to\" are required."};
            }
            const auto start = presenter_->resolve(*from, true);
            presenter_->glide(Presenter::global_point(start), 500, start.widget);
            workspace_->wait(std::chrono::milliseconds(530));
            press(start, QEvent::MouseButtonPress, Qt::LeftButton);
            presenter_->set_pressed(true);
            const auto end = presenter_->resolve(*to);
            const int ms = duration_ms(args, "ms", 900);
            presenter_->glide(Presenter::global_point(end), ms, start.widget, Qt::LeftButton);
            workspace_->wait(std::chrono::milliseconds(ms + 30));
            auto release = start;
            release.point = start.widget ? start.widget->mapFromGlobal(Presenter::global_point(end)) : end.point;
            press(release, QEvent::MouseButtonRelease, Qt::LeftButton);
            presenter_->set_pressed(false);
            workspace_->wait(std::chrono::milliseconds(150));
            return pointer_state();
        }});
    a->extend({"ui.key",
        "Presses keys as a hand would (\"R\", \"W\", \"Ctrl+R\", \"Escape\", \"F5\"): a menu shortcut runs its "
        "command, other keys go to the focused widget; a key badge shows them unless \"show\" is false.",
        json{{"keys", "Ctrl+R"}, {"show", "true"}, {"seconds", "1.5"}, {"position", "top-right"}},
        [this](const json& args) {
            const QString text = QString::fromStdString(args.value("keys", std::string()));
            const QKeySequence keys = QKeySequence::fromString(text, QKeySequence::PortableText);
            if (text.isEmpty() || keys.isEmpty() || keys[0].key() == Qt::Key_unknown) {
                throw Error{"invalid-argument", "Argument \"keys\" must name keys, such as \"R\" or \"Ctrl+R\"."};
            }
            if (args.value("show", true)) {
                show_keys(keys.toString(QKeySequence::NativeText), number(args, "seconds", 1.5),
                    args.value("position", std::string("top-right")), number(args, "size", 1.0));
            }
            const QString action = press_keys(keys); // after the reply, like a click
            return json{{"keys", keys.toString(QKeySequence::PortableText).toStdString()},
                {"action", action.isEmpty() ? json() : json(action.toStdString())}};
        }});
    a->extend({"ui.type",
        "Types text letter by letter into a field, as a hand would (the Components search filters as it goes); "
        "with a target the pointer clicks the field first; \"enter\" presses Enter at the end.",
        json{{"target", "{\"widget\": \"search\"} or {\"field\": \"Resistance\"}; optional: the focused field"},
            {"text", "resistor"}, {"ms", "90 per letter"}, {"enter", "false"}, {"clear", "true"}},
        [this](const json& args) {
            const QString text = QString::fromStdString(args.value("text", std::string()));
            QWidget* field = QApplication::focusWidget();
            if (const auto it = args.find("target"); it != args.end()) {
                const auto target = presenter_->resolve(*it, true);
                presenter_->glide(Presenter::global_point(target), 500);
                workspace_->wait(std::chrono::milliseconds(530));
                presenter_->show_click(1);
                field = target.widget;
            }
            if (auto* box = qobject_cast<QComboBox*>(field); box && box->isEditable()) {
                field = box->lineEdit();
            }
            if (!field) {
                throw Error{"blocked", "No field has the focus; give a target."};
            }
            field->setFocus(Qt::MouseFocusReason);
            if (args.value("clear", true)) {
                if (auto* line = qobject_cast<QLineEdit*>(field)) {
                    line->selectAll();
                } else if (auto* spin = qobject_cast<QAbstractSpinBox*>(field)) {
                    spin->selectAll();
                }
            }
            const int ms = duration_ms(args, "ms", 90);
            for (const QChar c : text) {
                const int key = c.isLetterOrNumber() ? static_cast<int>(c.toUpper().unicode()) : static_cast<int>(Qt::Key_unknown);
                QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier, QString(c));
                QApplication::sendEvent(field, &down);
                QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier, QString(c));
                QApplication::sendEvent(field, &up);
                workspace_->wait(std::chrono::milliseconds(ms));
            }
            if (args.value("enter", false)) {
                QKeyEvent down(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &down);
                QKeyEvent up(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &up);
                workspace_->wait(std::chrono::milliseconds(150));
            }
            return json{{"typed", text.toStdString()}};
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

int ControlServer::duration_ms(const json& args, const char* key, int fallback)
{
    return static_cast<int>(std::clamp(number(args, key, fallback), 0.0, 60000.0));
}

json ControlServer::pointer_state() const
{
    const QPoint at = presenter_->pointer_position();
    return json{{"shown", presenter_->pointer_visible()}, {"point", {at.x(), at.y()}}};
}

void ControlServer::press(const Presenter::Target& target, QEvent::Type type, Qt::MouseButton button)
{
    if (!target.widget) {
        return;
    }
    const QPointF local(target.point);
    const QPointF global = target.widget->mapToGlobal(local);
    const Qt::MouseButtons held = type == QEvent::MouseButtonRelease ? Qt::MouseButtons(Qt::NoButton) : Qt::MouseButtons(button);
    QMouseEvent event(type, local, global, button, held, Qt::NoModifier);
    QApplication::sendEvent(target.widget, &event);
}

void ControlServer::click(const Presenter::Target& target, Qt::MouseButton button, int count)
{
    if (!target.widget) {
        return;
    }
    // Posted, not sent: a click may open a modal dialog, whose own event loop
    // must not run inside this command.
    const QPointF local(target.point);
    const QPointF global = target.widget->mapToGlobal(local);
    const auto post = [&](QEvent::Type type, Qt::MouseButtons held) {
        QCoreApplication::postEvent(target.widget, new QMouseEvent(type, local, global, button, held, Qt::NoModifier));
    };
    post(QEvent::MouseButtonPress, button);
    post(QEvent::MouseButtonRelease, Qt::NoButton);
    if (count == 2) {
        post(QEvent::MouseButtonDblClick, button);
        post(QEvent::MouseButtonRelease, Qt::NoButton);
    }
}

QString ControlServer::press_keys(const QKeySequence& keys)
{
    // A menu's shortcut runs its command (posted events do not reach Qt's
    // shortcut map); other keys go to the focused widget, or the canvas.
    QList<QAction*> actions = window_.findChildren<QAction*>();
    if (window_.analyzer()->isActiveWindow()) {
        actions = window_.analyzer()->findChildren<QAction*>() + actions;
    }
    for (auto* action : actions) {
        if (!action->isEnabled()) {
            continue;
        }
        for (const auto& shortcut : action->shortcuts()) {
            if (shortcut.matches(keys) == QKeySequence::ExactMatch) {
                QTimer::singleShot(0, action, &QAction::trigger); // a dialog may open
                QString name = action->text();
                return name.remove(QLatin1Char('&'));
            }
        }
    }
    QWidget* focus = QApplication::focusWidget();
    QWidget* receiver = focus ? focus : window_.schematic();
    for (int i = 0; i < keys.count(); ++i) {
        const auto combination = keys[i];
        const bool command = combination.keyboardModifiers().testAnyFlags(
            Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
        const QString text = command ? QString() : QKeySequence(combination.key()).toString().left(1).toLower();
        QCoreApplication::postEvent(receiver,
            new QKeyEvent(QEvent::KeyPress, combination.key(), combination.keyboardModifiers(),
                combination.key() < Qt::Key_Escape ? text : QString()));
        QCoreApplication::postEvent(receiver,
            new QKeyEvent(QEvent::KeyRelease, combination.key(), combination.keyboardModifiers()));
    }
    return {};
}


void ControlServer::show_keys(const QString& text, double seconds, const std::string& position, double size)
{
    check_position(position);
    auto* viewport = window_.schematic()->viewport();
    if (!keys_) {
        keys_ = new QLabel(viewport);
        keys_->setObjectName(QStringLiteral("controlKeys"));
        keys_->setAlignment(Qt::AlignCenter);
        keys_->setAttribute(Qt::WA_TransparentForMouseEvents);
        keys_->setStyleSheet(QStringLiteral(
            "QLabel { background: rgba(30, 30, 30, 215); color: white; padding: 8px 16px; border-radius: 10px;"
            " border: 2px solid rgba(255, 255, 255, 120); }"));
        viewport->installEventFilter(this);
    }
    ++keys_serial_;
    if (text.isEmpty()) {
        keys_->hide();
        return;
    }
    QFont font = window_.font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 2.0 * std::clamp(size, 0.3, 5.0));
    keys_->setFont(font);
    keys_->setText(text);
    keys_position_ = position;
    place_label(keys_, position);
    keys_->show();
    keys_->raise();
    if (seconds > 0) {
        const int serial = keys_serial_;
        QTimer::singleShot(static_cast<int>(std::min(seconds, 3600.0) * 1000), keys_, [this, serial] {
            if (serial == keys_serial_) {
                keys_->hide();
            }
        });
    }
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
        viewport->installEventFilter(this);
    }
    QFont font = window_.font();
    font.setPointSizeF(font.pointSizeF() * 1.5 * caption_size_);
    caption_->setFont(font);
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
    place_label(caption_, caption_position_);
}

bool ControlServer::eventFilter(QObject* watched, QEvent* event)
{
    if (caption_ && watched == caption_->parentWidget() && event->type() == QEvent::Resize) {
        place_caption();
    }
    if (keys_ && watched == keys_->parentWidget() && event->type() == QEvent::Resize && keys_->isVisible()) {
        place_label(keys_, keys_position_);
    }
    return QObject::eventFilter(watched, event);
}

} // namespace simnodus::ui
