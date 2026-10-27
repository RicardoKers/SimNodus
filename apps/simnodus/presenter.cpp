// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "presenter.hpp"

#include "analyzer_window.hpp"
#include "component_panel.hpp"
#include "document.hpp"
#include "logic_widget.hpp"
#include "main_window.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"

#include "library/component_library.hpp"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace simnodus::ui {

using automation::Error;
using automation::json;

namespace {

constexpr Qt::WindowFlags overlay_flags = Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
    | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint;

void make_overlay(QWidget* w)
{
    w->setAttribute(Qt::WA_TranslucentBackground);
    w->setAttribute(Qt::WA_TransparentForMouseEvents);
    w->setAttribute(Qt::WA_ShowWithoutActivating);
    w->setAttribute(Qt::WA_NoSystemBackground);
}

// "&Wire..." -> "wire"
QString plain(QString text)
{
    text.remove(QLatin1Char('&'));
    if (text.endsWith(QStringLiteral("..."))) {
        text.chop(3);
    }
    const int tab = text.indexOf(QLatin1Char('\t'));
    return (tab >= 0 ? text.left(tab) : text).trimmed().toLower();
}

std::string text_of(const json& value, const char* key)
{
    const auto it = value.find(key);
    if (it == value.end() || !it->is_string() || it->get<std::string>().empty()) {
        throw Error{"invalid-argument", std::string("Target \"") + key + "\" must be text."};
    }
    return it->get<std::string>();
}

} // namespace

// The drawn pointer: an arrow with its tip at the window's center, ripples
// for clicks and a dot while the button is held.
class PointerOverlay : public QWidget {
public:
    static constexpr int half = 56;

    PointerOverlay() : QWidget(nullptr, overlay_flags)
    {
        make_overlay(this);
        setFixedSize(2 * half, 2 * half);
        ripple_.setInterval(16);
        connect(&ripple_, &QTimer::timeout, this, [this] {
            const qint64 now = clock_.elapsed();
            if (std::all_of(ripples_.begin(), ripples_.end(), [now](qint64 at) { return now - at > duration; })) {
                ripples_.clear();
                ripple_.stop();
            }
            update();
        });
        clock_.start();
    }

    void set_tip(QPoint global)
    {
        tip_ = global;
        move(global - QPoint(half, half));
    }
    QPoint tip() const { return tip_; }

    void click(int count)
    {
        for (int i = 0; i < count; ++i) {
            ripples_.push_back(clock_.elapsed() + i * 140);
        }
        ripple_.start();
        update();
    }

    void set_pressed(bool pressed)
    {
        pressed_ = pressed;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF tip(half, half);
        const qint64 now = clock_.elapsed();
        for (const qint64 at : ripples_) {
            const double t = static_cast<double>(now - at) / duration;
            if (t < 0 || t > 1) {
                continue;
            }
            QColor ring(255, 152, 0);
            ring.setAlphaF(1.0 - t);
            painter.setPen(QPen(ring, 3));
            painter.setBrush(Qt::NoBrush);
            const double r = 6 + 30 * t;
            painter.drawEllipse(tip, r, r);
        }
        if (pressed_) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 152, 0, 170));
            painter.drawEllipse(tip, 11, 11);
        }
        // An arrow like the system's, twice as large, so a phone screen shows it.
        QPainterPath arrow;
        const double s = 2.0;
        const QPointF points[] = {{0, 0}, {0, 17}, {4, 13}, {7, 20}, {10, 19}, {7, 12}, {12, 12}};
        arrow.moveTo(tip + points[0] * s);
        for (std::size_t i = 1; i < std::size(points); ++i) {
            arrow.lineTo(tip + points[i] * s);
        }
        arrow.closeSubpath();
        painter.setPen(QPen(Qt::black, 2.0));
        painter.setBrush(Qt::white);
        painter.drawPath(arrow);
    }

private:
    static constexpr qint64 duration = 450;
    QPoint tip_;
    bool pressed_ = false;
    std::vector<qint64> ripples_;
    QTimer ripple_;
    QElapsedTimer clock_;
};

// An outline around a rectangle of the screen, pulsing.
class HighlightOverlay : public QWidget {
public:
    static constexpr int margin = 8;

    HighlightOverlay() : QWidget(nullptr, overlay_flags)
    {
        make_overlay(this);
        pulse_.setInterval(33);
        connect(&pulse_, &QTimer::timeout, this, qOverload<>(&QWidget::update));
        clock_.start();
    }

    void show_around(const QRect& global, int ms)
    {
        setGeometry(global.adjusted(-margin, -margin, margin, margin));
        ++serial_;
        show();
        raise();
        pulse_.start();
        if (ms > 0) {
            const int serial = serial_;
            QTimer::singleShot(ms, this, [this, serial] {
                if (serial == serial_) {
                    hide();
                }
            });
        }
    }

protected:
    void hideEvent(QHideEvent*) override { pulse_.stop(); }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double phase = 0.5 + 0.5 * std::sin(static_cast<double>(clock_.elapsed()) / 1000.0 * 2 * std::numbers::pi);
        QColor color(255, 152, 0);
        color.setAlphaF(0.55 + 0.45 * phase);
        painter.setPen(QPen(color, 3 + 1.5 * phase));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(3, 3, -3, -3), 8, 8);
    }

private:
    QTimer pulse_;
    QElapsedTimer clock_;
    int serial_ = 0;
};

Presenter::Presenter(MainWindow& window) : QObject(&window), window_(window)
{
}

Presenter::~Presenter()
{
    delete pointer_;
    delete outline_;
}

Presenter::Target Presenter::resolve(const json& target, bool scroll) const
{
    auto* view = window_.schematic();
    const auto& schematic = window_.document()->schematic();
    const auto canvas_rect = [&](const QRectF& scene_rect) {
        if (scroll) {
            view->ensureVisible(scene_rect, 40, 40);
        }
        Target t;
        t.widget = view->viewport();
        t.rect = view->mapFromScene(scene_rect).boundingRect();
        t.point = t.rect.center();
        t.canvas = true;
        return t;
    };
    const auto grid_point = [&](double x, double y) {
        const QPointF scene(x * SchematicView::grid, y * SchematicView::grid);
        Target t = canvas_rect(QRectF(scene - QPointF(4, 4), QSizeF(8, 8)));
        t.point = view->mapFromScene(scene);
        return t;
    };
    const auto widget_target = [](QWidget* w, QRect rect = {}) {
        Target t;
        t.widget = w;
        t.rect = rect.isNull() ? w->rect() : rect;
        t.point = t.rect.center();
        return t;
    };

    if (target.is_array() && target.size() == 2 && target[0].is_number() && target[1].is_number()) {
        return grid_point(target[0].get<double>(), target[1].get<double>());
    }
    if (target.is_string()) {
        const std::string ref = target.get<std::string>();
        const auto dot = ref.find('.');
        const auto* component = schematic.component_by_designator(ref.substr(0, dot));
        if (!component) {
            throw Error{"not-found", "No component " + ref.substr(0, dot) + " on the schematic."};
        }
        if (dot == std::string::npos) {
            const auto* item = window_.scene()->item_of(component->id);
            if (!item) {
                throw Error{"not-found", "No drawing of " + ref + "."};
            }
            return canvas_rect(item->sceneBoundingRect());
        }
        const auto* definition = window_.library().find(component->type);
        const auto* pin = definition ? definition->pin(ref.substr(dot + 1)) : nullptr;
        if (!pin) {
            throw Error{"not-found", "No pin " + ref + "."};
        }
        const auto at = library::pin_position(*definition, *pin, *component);
        return grid_point(at.x, at.y);
    }
    if (!target.is_object()) {
        throw Error{"invalid-argument",
            "A target is a grid point [x, y], a pin \"R1.2\", a part \"R1\" or an object such as {\"widget\": \"search\"}."};
    }
    if (target.contains("area")) {
        const auto& a = target["area"];
        if (!a.is_array() || a.size() != 4 || !std::all_of(a.begin(), a.end(), [](const json& v) { return v.is_number(); })) {
            throw Error{"invalid-argument", "Target \"area\" must be [x1, y1, x2, y2] grid points."};
        }
        const double g = SchematicView::grid;
        return canvas_rect(QRectF(QPointF(std::min(a[0].get<double>(), a[2].get<double>()) * g,
                                      std::min(a[1].get<double>(), a[3].get<double>()) * g),
            QPointF(std::max(a[0].get<double>(), a[2].get<double>()) * g, std::max(a[1].get<double>(), a[3].get<double>()) * g)));
    }
    auto* analyzer = window_.analyzer();
    if (target.contains("widget")) {
        const std::string name = text_of(target, "widget");
        QWidget* w = nullptr;
        if (name == "components") {
            w = window_.components();
        } else if (name == "search") {
            w = window_.components()->search_field();
        } else if (name == "tree") {
            w = window_.components()->tree();
        } else if (name == "properties") {
            w = window_.properties();
        } else if (name == "messages") {
            w = window_.messages();
        } else if (name == "canvas") {
            w = view->viewport();
        } else if (name == "toolbar") {
            w = window_.findChild<QToolBar*>(QStringLiteral("mainToolbar"));
        } else if (name == "statusbar") {
            w = window_.statusBar();
        } else if (name == "menubar") {
            w = window_.menuBar();
        } else if (name == "window") {
            w = &window_;
        } else if (name == "analyzer") {
            w = analyzer;
        } else if (name == "scope") {
            w = analyzer->scope();
        } else if (name == "logic") {
            w = analyzer->logic();
        } else if (name == "analyzer-panel") {
            w = analyzer->findChild<QWidget*>(QStringLiteral("analyzerControls"));
        } else {
            throw Error{"invalid-argument", "Unknown widget \"" + name
                    + "\"; known: components, search, tree, properties, messages, canvas, toolbar, statusbar, menubar, "
                      "window, analyzer, scope, logic, analyzer-panel."};
        }
        if (!w || !w->isVisible()) {
            throw Error{"blocked", "The " + name + " is not shown."};
        }
        return widget_target(w);
    }
    if (target.contains("action")) {
        const QString wanted = plain(QString::fromStdString(text_of(target, "action")));
        for (auto* toolbar : window_.findChildren<QToolBar*>() + analyzer->findChildren<QToolBar*>()) {
            for (auto* action : toolbar->actions()) {
                auto* button = toolbar->widgetForAction(action);
                if (button && button->isVisible() && plain(action->text()) == wanted) {
                    return widget_target(button);
                }
            }
        }
        throw Error{"not-found", "No toolbar button \"" + target["action"].get<std::string>()
                + "\" is shown; use {\"menu\": ..., \"item\": ...} for a menu entry."};
    }
    if (target.contains("part")) {
        const QString wanted = QString::fromStdString(text_of(target, "part")).toLower();
        auto* tree = window_.components()->tree();
        QTreeWidgetItem* found = nullptr;
        for (QTreeWidgetItemIterator it(tree); *it && !found; ++it) {
            if (!(*it)->isHidden() && (*it)->childCount() == 0 && (*it)->text(0).toLower() == wanted) {
                found = *it;
            }
        }
        for (QTreeWidgetItemIterator it(tree); *it && !found; ++it) {
            if (!(*it)->isHidden() && (*it)->childCount() == 0 && (*it)->text(0).toLower().startsWith(wanted)) {
                found = *it;
            }
        }
        if (!found) {
            throw Error{"not-found", "No entry \"" + target["part"].get<std::string>()
                    + "\" is shown in Components (the search may hide it)."};
        }
        tree->scrollToItem(found);
        return widget_target(tree->viewport(), tree->visualItemRect(found));
    }
    if (target.contains("field")) {
        const QString wanted = QString::fromStdString(text_of(target, "field")).toLower();
        QCoreApplication::processEvents(); // the panel rebuilds after a change of the selection
        auto* host = window_.properties()->findChild<QWidget*>(QStringLiteral("parameterForm"));
        auto* form = host ? qobject_cast<QFormLayout*>(host->layout()) : nullptr;
        QWidget* best = nullptr;
        for (int row = 0; form && row < form->rowCount() && !best; ++row) {
            auto* label = form->itemAt(row, QFormLayout::LabelRole);
            auto* field = form->itemAt(row, QFormLayout::FieldRole);
            auto* text = label ? qobject_cast<QLabel*>(label->widget()) : nullptr;
            if (text && field && field->widget() && plain(text->text()).startsWith(wanted)) {
                best = field->widget();
            }
        }
        if (!best || !best->isVisible()) {
            throw Error{"not-found", "No field \"" + target["field"].get<std::string>()
                    + "\" in Properties; select the part first."};
        }
        return widget_target(best);
    }
    if (target.contains("menu")) {
        const QString wanted = plain(QString::fromStdString(text_of(target, "menu")));
        auto* bar = window_.menuBar();
        for (auto* action : bar->actions()) {
            if (plain(action->text()) != wanted || !action->menu()) {
                continue;
            }
            if (!target.contains("item")) {
                return widget_target(bar, bar->actionGeometry(action));
            }
            auto* menu = action->menu();
            if (!menu->isVisible()) {
                throw Error{"blocked", "The menu " + target["menu"].get<std::string>()
                        + " is closed; open it first (ui.click {\"menu\": ...})."};
            }
            const QString item = plain(QString::fromStdString(text_of(target, "item")));
            for (auto* entry : menu->actions()) {
                if (plain(entry->text()) == item) {
                    return widget_target(menu, menu->actionGeometry(entry));
                }
            }
            throw Error{"not-found", "No entry \"" + target["item"].get<std::string>() + "\" in that menu."};
        }
        throw Error{"not-found", "No menu \"" + target["menu"].get<std::string>() + "\"."};
    }
    if (target.contains("tab")) {
        const QString wanted = plain(QString::fromStdString(text_of(target, "tab")));
        auto* bar = analyzer->tabs()->tabBar();
        for (int i = 0; i < bar->count(); ++i) {
            if (plain(bar->tabText(i)) == wanted && bar->isVisible()) {
                return widget_target(bar, bar->tabRect(i));
            }
        }
        throw Error{"not-found", "No analyzer tab \"" + target["tab"].get<std::string>() + "\" is shown."};
    }
    if (target.contains("name")) {
        const QString name = QString::fromStdString(text_of(target, "name"));
        QWidget* w = window_.findChild<QWidget*>(name);
        w = w ? w : analyzer->findChild<QWidget*>(name);
        if (!w || !w->isVisible()) {
            throw Error{"not-found", "No shown widget named \"" + name.toStdString() + "\"."};
        }
        return widget_target(w);
    }
    throw Error{"invalid-argument", "Unknown target " + target.dump() + "."};
}

QRect Presenter::global_rect(const Target& target)
{
    if (!target.widget) {
        return {};
    }
    return QRect(target.widget->mapToGlobal(target.rect.topLeft()), target.rect.size());
}

QPoint Presenter::global_point(const Target& target)
{
    return target.widget ? target.widget->mapToGlobal(target.point) : QPoint();
}

QRect Presenter::physical(const QRect& global)
{
    const QScreen* screen = QGuiApplication::screenAt(global.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return global;
    }
    // A screen's logical origin is its physical one; the rest scales.
    const QPoint origin = screen->geometry().topLeft();
    const double ratio = screen->devicePixelRatio();
    const auto scale = [&](QPoint p) {
        return origin + QPoint(static_cast<int>(std::lround((p.x() - origin.x()) * ratio)),
                            static_cast<int>(std::lround((p.y() - origin.y()) * ratio)));
    };
    return QRect(scale(global.topLeft()), scale(global.topLeft() + QPoint(global.width(), global.height())))
        .adjusted(0, 0, -1, -1);
}

QPoint Presenter::start_point() const
{
    return window_.mapToGlobal(window_.rect().center());
}

void Presenter::glide(QPoint global, int ms, QWidget* drive, Qt::MouseButtons held)
{
    if (!pointer_) {
        pointer_ = new PointerOverlay;
        pointer_->set_tip(start_point());
        motion_ = new QVariantAnimation(this);
        motion_->setEasingCurve(QEasingCurve::InOutCubic);
    }
    if (!pointer_->isVisible()) {
        pointer_->show();
    }
    pointer_->raise();
    motion_->stop();
    motion_->disconnect(this);
    const QPoint from = pointer_->tip();
    QPointer<QWidget> target(drive);
    const auto step = [this, target, held](QPoint at) {
        pointer_->set_tip(at);
        if (target) {
            const QPointF local = target->mapFromGlobal(QPointF(at));
            QMouseEvent move(QEvent::MouseMove, local, QPointF(at), Qt::NoButton, held, Qt::NoModifier);
            QApplication::sendEvent(target, &move);
        }
    };
    if (ms <= 0 || from == global) {
        step(global);
        return;
    }
    motion_->setStartValue(QPointF(from));
    motion_->setEndValue(QPointF(global));
    motion_->setDuration(ms);
    connect(motion_, &QVariantAnimation::valueChanged, this,
        [step](const QVariant& value) { step(value.toPointF().toPoint()); });
    motion_->start();
}

void Presenter::show_click(int count)
{
    if (pointer_) {
        pointer_->click(count);
    }
}

void Presenter::set_pressed(bool pressed)
{
    if (pointer_) {
        pointer_->set_pressed(pressed);
    }
}

void Presenter::hide_pointer()
{
    if (pointer_) {
        motion_->stop();
        pointer_->hide();
    }
}

bool Presenter::pointer_visible() const
{
    return pointer_ && pointer_->isVisible();
}

QPoint Presenter::pointer_position() const
{
    return pointer_ ? pointer_->tip() : start_point();
}

void Presenter::highlight(const QRect& global, int ms)
{
    if (!outline_) {
        outline_ = new HighlightOverlay;
    }
    outline_->show_around(global, ms);
}

void Presenter::hide_highlight()
{
    if (outline_) {
        outline_->hide();
    }
}

bool Presenter::highlight_visible() const
{
    return outline_ && outline_->isVisible();
}

} // namespace simnodus::ui
