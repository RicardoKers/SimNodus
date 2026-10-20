// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "schematic_scene.hpp"

#include "document.hpp"
#include "symbol_painter.hpp"
#include "theme.hpp"

#include "model/engineering.hpp"

#include <QFileInfo>
#include <QGraphicsItem>
#include <QPainter>
#include <QPainterPathStroker>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace simnodus::ui {
namespace {

constexpr double line_width = 1.4;    // scene units
constexpr double text_pixels = 11.0;  // scene units at 100 % zoom
constexpr int key_id = 0;

QFont schematic_font(bool bold = false)
{
    QFont font;
    font.setPixelSize(static_cast<int>(text_pixels));
    font.setBold(bold);
    return font;
}

QColor led_color(const std::string& name)
{
    if (name == "green") {
        return QColor(0x2e, 0xcc, 0x40);
    }
    if (name == "blue") {
        return QColor(0x2f, 0x7d, 0xff);
    }
    if (name == "yellow") {
        return QColor(0xff, 0xd0, 0x1a);
    }
    if (name == "white") {
        return QColor(0xf4, 0xf4, 0xe8);
    }
    return QColor(0xff, 0x30, 0x20);
}

QColor voltage_color(double volts, double scale)
{
    if (scale <= 0) {
        return theme::ink;
    }
    const double k = std::clamp(std::abs(volts) / scale, 0.0, 1.0);
    const QColor hot = volts >= 0 ? QColor(0xd6, 0x2d, 0x20) : QColor(0x1f, 0x5f, 0xd1);
    return QColor::fromRgbF(theme::ink.redF() + (hot.redF() - theme::ink.redF()) * k,
        theme::ink.greenF() + (hot.greenF() - theme::ink.greenF()) * k,
        theme::ink.blueF() + (hot.blueF() - theme::ink.blueF()) * k);
}

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

QColor probe_color(int index)
{
    static const QColor colors[] = {QColor(0x1f, 0x77, 0xb4), QColor(0xd6, 0x27, 0x28), QColor(0x2c, 0xa0, 0x2c),
        QColor(0xff, 0x7f, 0x0e), QColor(0x94, 0x67, 0xbd), QColor(0x8c, 0x56, 0x4b), QColor(0xe3, 0x77, 0xc2),
        QColor(0x17, 0xbe, 0xcf)};
    return colors[static_cast<std::size_t>(index) % std::size(colors)];
}

// ---- Items --------------------------------------------------------------

// Radius of the glow around a lit LED or lamp, in grid units (it must stay
// inside the item's bounding rectangle).
constexpr double halo_radius = 3.4;

class ComponentItem : public QGraphicsItem {
public:
    ComponentItem(SchematicScene& scene, bool ghost) : scene_(scene), ghost_(ghost)
    {
        if (!ghost) {
            setFlag(ItemIsSelectable, true);
        }
        setZValue(ghost ? 6 : 2);
        setAcceptHoverEvents(!ghost);
    }

    void set(const model::Component& component, const library::ComponentDefinition* definition)
    {
        if (component == component_ && definition == definition_) {
            update();
            return;
        }
        prepareGeometryChange();
        component_ = component;
        definition_ = definition;
        setPos(to_scene(component.position));
        setData(key_id, QVariant::fromValue<qulonglong>(component.id));
        update();
    }

    const model::Component& component() const { return component_; }
    const library::ComponentDefinition* definition() const { return definition_; }
    // Live state changes the readout box, so the bounds may change too.
    void refresh_live()
    {
        prepareGeometryChange();
        update();
    }

    model::Point text_offset(bool value) const
    {
        if (value && component_.value_offset) {
            return *component_.value_offset;
        }
        if (!value && component_.designator_offset) {
            return *component_.designator_offset;
        }
        if (!definition_) {
            return value ? model::Point{2, 1} : model::Point{2, -1};
        }
        // Library defaults follow the symbol orientation; text stays upright.
        const model::Point base = value ? definition_->value_offset : definition_->designator_offset;
        model::Orientation o = component_.orientation;
        return model::orient(base, o);
    }

    QString value_text() const
    {
        if (!definition_ || definition_->value_parameter.empty()) {
            return {};
        }
        const auto* p = definition_->parameter(definition_->value_parameter);
        const auto it = component_.parameters.find(definition_->value_parameter);
        QString text = QString::fromStdString(it == component_.parameters.end() ? p->default_value : it->second);
        if (p && p->kind == library::Parameter::Kind::Model && text == QString::fromStdString(p->default_value)
            && (p->default_value == "Generic" || p->model_type.empty())) {
            return {}; // the generic or behavioral model needs no text; a named part (1N5819) does
        }
        if (p && p->kind == library::Parameter::Kind::File) {
            // Only the file name; an empty value asks for a file.
            return text.isEmpty() ? QObject::tr("(no firmware)") : QFileInfo(text).fileName();
        }
        if (p && p->choices.empty() && !p->unit.empty() && p->unit != "Ohm" && p->unit != "%" && p->unit != "deg") {
            text += QString::fromStdString(p->unit);
        }
        if (p && !p->symbol.empty()) {
            text = QString::fromStdString(p->symbol) + QLatin1Char('=') + text;
        }
        return text;
    }

    QRectF text_rect(const QString& text, const model::Point& offset) const
    {
        const QFontMetricsF metrics(schematic_font());
        const QSizeF size(metrics.horizontalAdvance(text) + 2, metrics.height());
        const QPointF anchor(offset.x * scene_grid, offset.y * scene_grid);
        // Text left of the symbol is right-aligned to its anchor.
        const double left = offset.x < 0 ? anchor.x() - size.width() : anchor.x();
        return QRectF(QPointF(left, anchor.y() - size.height() / 2), size);
    }

    QRectF symbol_rect() const
    {
        if (!definition_) {
            return QRectF(-scene_grid, -scene_grid, 2 * scene_grid, 2 * scene_grid);
        }
        QTransform t;
        t.rotate(component_.orientation.rotation);
        t.scale(component_.orientation.mirrored ? -1 : 1, 1);
        const QRectF b = symbol_bounds(*definition_);
        const QRectF scaled(b.x() * scene_grid, b.y() * scene_grid, b.width() * scene_grid, b.height() * scene_grid);
        return t.mapRect(scaled);
    }

    QRectF boundingRect() const override
    {
        QRectF r = symbol_rect();
        if (definition_ && definition_->readout && definition_->readout->glows()) {
            // The halo of a lit LED or lamp reaches beyond the symbol: without it here,
            // the scene neither repaints nor clears that part (a cut-off, stale glow).
            const double h = halo_radius * scene_grid;
            r = r.united(QRectF(-h, -h, 2 * h, 2 * h));
        }
        if (!ghost_) {
            if (show_designator()) {
                r = r.united(text_rect(QString::fromStdString(component_.designator), text_offset(false)));
            }
            const QString value = value_text();
            if (!value.isEmpty()) {
                r = r.united(text_rect(value, text_offset(true)));
            }
            r = r.united(readout_rect());
            if (const QRectF w = wiper_text_rect(); !w.isNull()) {
                r = r.united(w);
            }
        }
        return r.adjusted(-2, -2, 2, 2);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.addRect(symbol_rect());
        return path;
    }

    bool show_designator() const { return !definition_ || definition_->show_designator; }

    QRectF readout_rect() const
    {
        const auto& live = scene_.live();
        const auto it = live.readouts.find(component_.id);
        if (!live.running || it == live.readouts.end()) {
            return {};
        }
        QFont font = schematic_font(true);
        const QFontMetricsF metrics(font);
        const QRectF symbol = symbol_rect();
        const QSizeF size(metrics.horizontalAdvance(it->second) + 8, metrics.height() + 4);
        if (symbol.height() > symbol.width()) {
            // Beside a vertical part, under its name: the wires leave at the top and bottom.
            return QRectF(QPointF(symbol.right() + 4, symbol.center().y() + 2), size);
        }
        return QRectF(QPointF(symbol.center().x() - size.width() / 2, symbol.bottom() + 3), size);
    }

    // Potentiometer position 0..1 (UI-18): the live control while running, else
    // the position parameter; nullopt for parts without a wiper.
    std::optional<double> wiper_position() const
    {
        if (!definition_ || !definition_->interactive || !definition_->interactive->wiper) {
            return std::nullopt;
        }
        const auto& live = scene_.live();
        if (live.running) {
            if (const auto it = live.controls.find(component_.id); it != live.controls.end()) {
                return std::clamp(it->second, 0.0, 1.0);
            }
        }
        const auto* p = definition_->parameter(definition_->interactive->parameter);
        if (!p) {
            return 0.5;
        }
        const auto it = component_.parameters.find(p->name);
        const auto value = model::parse_engineering(it == component_.parameters.end() ? p->default_value : it->second, p->unit);
        return std::clamp(value.value_or(50.0) / 100.0, 0.0, 1.0);
    }

    // Percentage next to the wiper, upright whatever the symbol's orientation.
    QRectF wiper_text_rect(QString* text = nullptr) const
    {
        const auto position = wiper_position();
        if (!position) {
            return {};
        }
        const QString label = QStringLiteral("%1 %").arg(qRound(*position * 100));
        if (text) {
            *text = label;
        }
        const auto& w = *definition_->interactive->wiper;
        const QPointF tip = wiper_tip(*definition_, *position);
        // Opposite the wiper pin, a little away from the body.
        const auto* pin = definition_->pin(w.pin);
        const double side = pin && pin->at.x > tip.x() ? -1.0 : 1.0;
        QTransform t;
        t.scale(scene_grid, scene_grid);
        t.rotate(component_.orientation.rotation);
        t.scale(component_.orientation.mirrored ? -1 : 1, 1);
        const QPointF anchor = t.map(QPointF(tip.x() + side * 1.6, tip.y()));
        const QFontMetricsF metrics(schematic_font());
        const QSizeF size(metrics.horizontalAdvance(label) + 2, metrics.height());
        const QPointF outward = anchor - t.map(QPointF(tip.x(), tip.y()));
        const double left = outward.x() < -0.1 ? anchor.x() - size.width()
            : outward.x() > 0.1                ? anchor.x()
                                               : anchor.x() - size.width() / 2;
        const double top = outward.y() < -0.1 ? anchor.y() - size.height()
            : outward.y() > 0.1               ? anchor.y()
                                              : anchor.y() - size.height() / 2;
        return QRectF(QPointF(left, top), size);
    }

    QString state_key() const
    {
        if (!definition_) {
            return {};
        }
        const auto& live = scene_.live();
        if (!definition_->interactive) {
            // State graphics driven by a current (a relay's contact): at rest when not running.
            if (definition_->state_graphics.empty()) {
                return {};
            }
            const auto it = live.states.find(component_.id);
            return live.running && it != live.states.end() ? it->second : QStringLiteral("0");
        }
        if (live.running) {
            const auto it = live.controls.find(component_.id);
            if (it != live.controls.end()) {
                if (definition_->interactive->kind == library::Interactive::Kind::Keypad) {
                    return QString::number(std::lround(it->second)); // the key held, "0" for none
                }
                return it->second >= 0.5 ? QStringLiteral("1") : QStringLiteral("0");
            }
        }
        const auto& interactive = *definition_->interactive;
        if (interactive.kind == library::Interactive::Kind::Toggle && !interactive.parameter.empty()) {
            const auto* p = definition_->parameter(interactive.parameter);
            const auto it = component_.parameters.find(interactive.parameter);
            const std::string value = it == component_.parameters.end() ? p->default_value : it->second;
            return p && !p->choices.empty() && value == p->choices.back() ? QStringLiteral("1") : QStringLiteral("0");
        }
        return QStringLiteral("0");
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const QColor ghost(theme::muted_text.red(), theme::muted_text.green(), theme::muted_text.blue(), 170);
        const QColor ink = ghost_ ? ghost : isSelected() ? theme::selection : theme::ink;
        if (!definition_) {
            painter->setPen(QPen(QColor(0xb4, 0x23, 0x18), line_width, Qt::DashLine));
            painter->drawRect(symbol_rect());
            painter->drawText(symbol_rect(), Qt::AlignCenter, QStringLiteral("?"));
            return;
        }
        SymbolStyle style;
        style.ink = ink;
        style.line_width = line_width / scene_grid;
        style.state = state_key();
        style.draw_pins = ghost_;
        style.wiper = wiper_position().value_or(0.5);
        const auto& live = scene_.live();
        if (live.running) {
            if (const auto it = live.glow.find(component_.id); it != live.glow.end()) {
                const std::string& name = it->second.color;
                QColor glow = !name.empty() && name[0] == '#' ? QColor(QString::fromStdString(name)) : led_color(name);
                glow.setAlphaF(std::clamp(it->second.level, 0.0, 1.0));
                style.glow = glow;
            }
            if (const auto it = live.lights.find(component_.id); it != live.lights.end() && definition_->lights) {
                const std::string& name = it->second.color;
                const QColor base = !name.empty() && name[0] == '#' ? QColor(QString::fromStdString(name)) : led_color(name);
                const auto& lights = definition_->lights->lights;
                for (std::size_t i = 0; i < lights.size() && i < it->second.levels.size(); ++i) {
                    QColor c = base;
                    // A small lit shape needs more color than a halo to read as lit.
                    c.setAlphaF(std::clamp(it->second.levels[i] * 1.4, 0.0, 1.0));
                    if (c.alphaF() > 0.02) {
                        style.fills[lights[i].graphic] = c;
                    }
                }
            }
        }
        painter->save();
        painter->scale(scene_grid, scene_grid);
        painter->rotate(component_.orientation.rotation);
        if (component_.orientation.mirrored) {
            painter->scale(-1, 1);
        }
        if (style.glow.isValid() && style.glow.alpha() > 0) {
            // Halo around a lit LED or lamp.
            constexpr double radius = halo_radius;
            QRadialGradient halo(QPointF(0, 0), radius);
            QColor inner = style.glow;
            inner.setAlphaF(style.glow.alphaF() * 0.85);
            QColor middle = style.glow;
            middle.setAlphaF(style.glow.alphaF() * 0.45);
            QColor outer = style.glow;
            outer.setAlpha(0);
            halo.setColorAt(0, inner);
            halo.setColorAt(0.45, middle);
            halo.setColorAt(1, outer);
            painter->setPen(Qt::NoPen);
            painter->setBrush(halo);
            painter->drawEllipse(QPointF(0, 0), radius, radius);
        }
        paint_symbol(*painter, *definition_, style);
        if (definition_->mcu && live.running && !ghost_) {
            paint_mcu(*painter, live);
        }
        if (definition_->screen && !ghost_) {
            paint_screen(*painter, live);
        }
        painter->restore();
        if (ghost_) {
            return;
        }
        painter->setFont(schematic_font());
        painter->setPen(isSelected() ? theme::selection : theme::ink);
        if (show_designator()) {
            const QString designator = QString::fromStdString(component_.designator);
            painter->drawText(text_rect(designator, text_offset(false)), Qt::AlignLeft | Qt::AlignVCenter, designator);
        }
        const QString value = value_text();
        if (!value.isEmpty()) {
            painter->setPen(isSelected() ? theme::selection : theme::muted_text);
            painter->drawText(text_rect(value, text_offset(true)), Qt::AlignLeft | Qt::AlignVCenter, value);
        }
        QString position;
        if (const QRectF r = wiper_text_rect(&position); !position.isEmpty()) {
            painter->setPen(isSelected() ? theme::selection : theme::muted_text);
            painter->drawText(r, Qt::AlignCenter, position);
        }
        const QRectF readout = readout_rect();
        if (!readout.isNull()) {
            painter->setPen(QPen(theme::selection, 0.8));
            painter->setBrush(theme::readout_background);
            painter->drawRoundedRect(readout, 3, 3);
            painter->setFont(schematic_font(true));
            painter->setPen(theme::selection);
            painter->drawText(readout, Qt::AlignCenter, live.readouts.at(component_.id));
        }
    }

private:
    // A character display (SN-206): the glass, and while running the text of
    // its chip in a grid of the part's rows and columns, in symbol coordinates.
    void paint_screen(QPainter& painter, const LiveState& live) const
    {
        const auto& screen = *definition_->screen;
        const QRectF glass(screen.at.x, screen.at.y, screen.size.x, screen.size.y);
        auto parameter = [&](const std::string& name, int fallback) {
            const auto it = component_.parameters.find(name);
            const auto* p = definition_->parameter(name);
            const std::string value = it != component_.parameters.end() ? it->second : p ? p->default_value : std::string();
            const int n = std::atoi(value.c_str());
            return n > 0 ? n : fallback;
        };
        const int rows = std::clamp(parameter(screen.rows_parameter, 2), 1, 4);
        const int columns = std::clamp(parameter(screen.columns_parameter, 16), 1, 40);
        const bool on = live.running;
        painter.setPen(Qt::NoPen);
        painter.setBrush(on ? QColor(0x9a, 0xc8, 0x3c) : QColor(0x6f, 0x8a, 0x3a));
        painter.drawRect(glass);
        const double cell_w = glass.width() / columns;
        const double cell_h = glass.height() / rows;
        const auto it = live.texts.find(component_.id);
        const QStringList lines = on && it != live.texts.end() ? it->second.split(QLatin1Char('\n')) : QStringList();
        QFont font(QStringLiteral("Consolas"));
        font.setStyleHint(QFont::Monospace);
        font.setPointSizeF(std::max(0.2, std::min(cell_h * 0.8, cell_w * 1.3)));
        painter.setFont(font);
        for (int r = 0; r < rows; ++r) {
            const QString line = r < lines.size() ? lines[r] : QString();
            for (int c = 0; c < columns; ++c) {
                const QRectF cell(glass.left() + c * cell_w, glass.top() + r * cell_h, cell_w, cell_h);
                // The dot-matrix cells, faint, as on the glass.
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0x80, 0xa8, 0x30, on ? 120 : 60));
                painter.drawRect(cell.adjusted(cell_w * 0.08, cell_h * 0.08, -cell_w * 0.08, -cell_h * 0.08));
                if (c < line.size() && line[c] != QLatin1Char(' ')) {
                    painter.setPen(QColor(0x10, 0x20, 0x10));
                    painter.drawText(cell, Qt::AlignCenter, QString(line[c]));
                }
            }
        }
    }

    // Pin states and the on-board LED of a running microcontroller board, in
    // symbol coordinates (INT-06).
    void paint_mcu(QPainter& painter, const LiveState& live) const
    {
        const auto board = live.mcu_pins.find(component_.id);
        if (board == live.mcu_pins.end()) {
            return;
        }
        const auto& states = board->second;
        for (const auto& pin : definition_->pins) {
            const auto it = states.find(pin.name);
            if (it == states.end() || it->second.mode == mcu::PinMode::Unknown) {
                continue;
            }
            const auto& state = it->second;
            // Marker on the pin stub, between the pin tip and the board edge.
            const double dx = pin.direction == library::PinDirection::Left ? 1.0
                : pin.direction == library::PinDirection::Right             ? -1.0
                                                                            : 0.0;
            const double dy = pin.direction == library::PinDirection::Up ? 1.0
                : pin.direction == library::PinDirection::Down           ? -1.0
                                                                         : 0.0;
            const QPointF center(pin.at.x + dx, pin.at.y + dy);
            const QRectF marker(center.x() - 0.55, center.y() - 0.4, 1.1, 0.8);
            if (state.mode == mcu::PinMode::Input || state.mode == mcu::PinMode::Analog) {
                // Hollow; the border shows the level the circuit gives an input (red: 1).
                painter.setBrush(theme::canvas_background);
                painter.setPen(state.mode == mcu::PinMode::Input && state.level ? QPen(theme::pin_high, 0.18)
                                                                                : QPen(theme::muted_text, 0.12));
            } else {
                painter.setBrush(state.level ? theme::pin_high : theme::pin_low);
                painter.setPen(state.mode == mcu::PinMode::Alternate ? QPen(theme::ink, 0.16, Qt::DotLine)
                                                                      : QPen(theme::ink, 0.08));
            }
            painter.drawRoundedRect(marker, 0.15, 0.15);
        }
        if (const auto& led = definition_->mcu->led) {
            const auto it = states.find(led->pin);
            const bool lit = it != states.end() && it->second.mode == mcu::PinMode::Output
                && it->second.level != led->active_low;
            if (lit) {
                const QPointF at(led->at.x, led->at.y);
                QRadialGradient halo(at, led->radius * 3);
                halo.setColorAt(0, QColor(0x2e, 0xcc, 0x40, 200));
                halo.setColorAt(1, QColor(0x2e, 0xcc, 0x40, 0));
                painter.setPen(Qt::NoPen);
                painter.setBrush(halo);
                painter.drawEllipse(at, led->radius * 3, led->radius * 3);
                painter.setBrush(QColor(0x2e, 0xcc, 0x40));
                painter.setPen(QPen(theme::ink, 0.08));
                painter.drawEllipse(at, led->radius, led->radius);
            }
        }
    }

    SchematicScene& scene_;
    bool ghost_ = false;
    model::Component component_;
    const library::ComponentDefinition* definition_ = nullptr;
};

class WireItem : public QGraphicsItem {
public:
    explicit WireItem(SchematicScene& scene, bool preview = false) : scene_(scene), preview_(preview)
    {
        if (!preview) {
            setFlag(ItemIsSelectable, true);
        }
        setZValue(preview ? 6 : 1);
    }

    void set(const model::Wire& wire)
    {
        if (wire == wire_) {
            update();
            return;
        }
        prepareGeometryChange();
        wire_ = wire;
        setData(key_id, QVariant::fromValue<qulonglong>(wire.id));
        path_ = QPainterPath();
        for (std::size_t i = 0; i < wire.points.size(); ++i) {
            if (i == 0) {
                path_.moveTo(to_scene(wire.points[i]));
            } else {
                path_.lineTo(to_scene(wire.points[i]));
            }
        }
        update();
    }

    QRectF boundingRect() const override { return path_.boundingRect().adjusted(-4, -4, 4, 4); }

    QPainterPath shape() const override
    {
        QPainterPathStroker stroker;
        stroker.setWidth(7);
        return stroker.createStroke(path_);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        QColor color = isSelected() ? theme::selection : theme::ink;
        if (preview_) {
            color = theme::selection;
        } else if (!isSelected() && scene_.show_voltages() && scene_.live().running) {
            const auto& nets = scene_.live().nets;
            double scale = 0;
            for (const auto& [name, volts] : nets) {
                scale = std::max(scale, std::abs(volts));
            }
            const auto it = scene_.connectivity().wire_net.find(wire_.id);
            if (it != scene_.connectivity().wire_net.end()) {
                const auto& name = scene_.connectivity().nets[static_cast<std::size_t>(it->second)].name;
                const auto v = nets.find(lower(name));
                if (v != nets.end()) {
                    color = voltage_color(v->second, scale);
                }
            }
        }
        QPen pen(color, line_width, preview_ ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path_);
    }

    const model::Wire& wire() const { return wire_; }

private:
    SchematicScene& scene_;
    bool preview_ = false;
    model::Wire wire_;
    QPainterPath path_;
};

class LabelItem : public QGraphicsItem {
public:
    LabelItem()
    {
        setFlag(ItemIsSelectable, true);
        setZValue(3);
    }

    void set(const model::NetLabel& label)
    {
        if (label == label_) {
            return;
        }
        prepareGeometryChange();
        label_ = label;
        setPos(to_scene(label.position));
        setData(key_id, QVariant::fromValue<qulonglong>(label.id));
        update();
    }

    QRectF text_box() const
    {
        const QFontMetricsF metrics(schematic_font(true));
        const QString text = QString::fromStdString(label_.name);
        const double width = metrics.horizontalAdvance(text) + 8;
        // Mirrored, the name sits left of the point so that a wire can come from the right.
        return QRectF(label_.mirrored ? -4 - width : 4, -metrics.height() - 4, width, metrics.height() + 2);
    }

    QRectF boundingRect() const override { return text_box().united(QRectF(-3, -3, 6, 6)).adjusted(-2, -2, 2, 2); }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const QColor color = isSelected() ? theme::selection : theme::ink;
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(color, 1.0));
        painter->setBrush(Qt::NoBrush);
        const QRectF box = text_box();
        painter->drawLine(QPointF(0, 0), label_.mirrored ? box.bottomRight() : box.bottomLeft());
        painter->setBrush(theme::label_background);
        painter->drawRoundedRect(box, 2, 2);
        painter->setFont(schematic_font(true));
        painter->drawText(box, Qt::AlignCenter, QString::fromStdString(label_.name));
        painter->setBrush(color);
        painter->drawEllipse(QPointF(0, 0), 1.6, 1.6);
    }

private:
    model::NetLabel label_;
};

// Bus (UI-19): a thick line with its name at the start; it connects nothing.
class BusItem : public QGraphicsItem {
public:
    BusItem()
    {
        setFlag(ItemIsSelectable, true);
        setZValue(1);
    }

    void set(const model::Bus& bus)
    {
        if (bus == bus_) {
            return;
        }
        prepareGeometryChange();
        bus_ = bus;
        setData(key_id, QVariant::fromValue<qulonglong>(bus.id));
        path_ = QPainterPath();
        for (std::size_t i = 0; i < bus.points.size(); ++i) {
            if (i == 0) {
                path_.moveTo(to_scene(bus.points[i]));
            } else {
                path_.lineTo(to_scene(bus.points[i]));
            }
        }
        update();
    }

    QRectF name_rect() const
    {
        if (bus_.points.size() < 2) {
            return {};
        }
        const QFontMetricsF metrics(schematic_font(true));
        const QString name = QString::fromStdString(bus_.name);
        const QPointF a = to_scene(bus_.points[0]);
        const QPointF b = to_scene(bus_.points[1]);
        const QSizeF size(metrics.horizontalAdvance(name) + 2, metrics.height());
        // Beside the first segment, next to its start.
        if (a.y() == b.y()) {
            const double x = b.x() > a.x() ? a.x() + 6 : a.x() - 6 - size.width();
            return QRectF(QPointF(x, a.y() - size.height() - 2), size);
        }
        const double y = b.y() > a.y() ? a.y() + 4 : a.y() - 4 - size.height();
        return QRectF(QPointF(a.x() + 6, y), size);
    }

    QRectF boundingRect() const override { return path_.boundingRect().adjusted(-5, -5, 5, 5).united(name_rect()); }

    QPainterPath shape() const override
    {
        QPainterPathStroker stroker;
        stroker.setWidth(9);
        QPainterPath shape = stroker.createStroke(path_);
        shape.addRect(name_rect());
        return shape;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const QColor color = isSelected() ? theme::selection : theme::bus;
        painter->setPen(QPen(color, 4.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path_);
        painter->setFont(schematic_font(true));
        painter->drawText(name_rect(), Qt::AlignCenter, QString::fromStdString(bus_.name));
    }

private:
    model::Bus bus_;
    QPainterPath path_;
};

// Splitter (UI-19): the bus side, a bar and one stub per bit with its number.
class SplitterItem : public QGraphicsItem {
public:
    SplitterItem()
    {
        setFlag(ItemIsSelectable, true);
        setZValue(3);
    }

    void set(const model::Splitter& splitter)
    {
        if (splitter == splitter_) {
            return;
        }
        prepareGeometryChange();
        splitter_ = splitter;
        setData(key_id, QVariant::fromValue<qulonglong>(splitter.id));
        update();
    }

    // Local splitter point (grid units) -> item coordinates (the item sits at scene 0,0).
    QPointF map(double x, double y) const
    {
        const auto p = model::transform(model::PointF{x, y}, splitter_.position, splitter_.orientation);
        return {p.x * scene_grid, p.y * scene_grid};
    }

    int rows() const
    {
        return static_cast<int>(std::max(splitter_.inputs.size(), splitter_.outputs.size()));
    }

    // Bit range of a pin, counted along the splitter ("0-3", "5").
    static QString range(int from, int width)
    {
        return width == 1 ? QString::number(from) : QStringLiteral("%1-%2").arg(from).arg(from + width - 1);
    }

    // Above the pin's line, starting at the bar and growing outward (over the wire, not across the bar).
    QRectF label_rect(bool left, int row, const QString& text) const
    {
        const QFontMetricsF metrics(schematic_font());
        const QSizeF size(metrics.horizontalAdvance(text) + 2, metrics.height());
        const QPointF bar = map(1, 2.0 * row - 0.6);
        const QPointF outward = map(left ? 0 : 2, 2.0 * row - 0.6);
        const bool grows_left = outward.x() < bar.x() - 0.5;
        const bool grows_right = outward.x() > bar.x() + 0.5;
        QPointF top_left = bar - QPointF(size.width() / 2, size.height() / 2); // vertical part: centred
        if (grows_left) {
            top_left = QPointF(bar.x() - 2 - size.width(), bar.y() - size.height() / 2);
        } else if (grows_right) {
            top_left = QPointF(bar.x() + 2, bar.y() - size.height() / 2);
        }
        return QRectF(top_left, size);
    }

    // Every pin: its side, row, width and label.
    struct PinView {
        bool left;
        int row;
        int width;
        QString label;
    };
    std::vector<PinView> pin_views() const
    {
        std::vector<PinView> views;
        for (const bool left : {true, false}) {
            int bit = splitter_.first;
            int row = 0;
            for (const int width : left ? splitter_.inputs : splitter_.outputs) {
                views.push_back({left, row++, width, range(bit, width)});
                bit += width;
            }
        }
        return views;
    }

    QRectF boundingRect() const override
    {
        QRectF r = QRectF(map(-0.3, -1), map(2.3, 2.0 * (rows() - 1) + 1)).normalized();
        for (const auto& pin : pin_views()) {
            r = r.united(label_rect(pin.left, pin.row, pin.label));
        }
        return r.adjusted(-4, -4, 4, 4);
    }

    QPainterPath shape() const override
    {
        QPainterPath path;
        path.addRect(QRectF(map(-0.2, -0.8), map(2.0, 2.0 * (rows() - 1) + 0.8)).normalized());
        return path;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const QColor bus = isSelected() ? theme::selection : theme::bus;
        const QColor ink = isSelected() ? theme::selection : theme::ink;
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(bus, 4.0, Qt::SolidLine, Qt::FlatCap));
        painter->drawLine(map(1, -0.6), map(1, 2.0 * (rows() - 1) + 0.6));
        painter->setFont(schematic_font());
        for (const auto& pin : pin_views()) {
            // Wider pins are buses, drawn thick; single bits are wires.
            painter->setPen(pin.width > 1 ? QPen(bus, 4.0, Qt::SolidLine, Qt::FlatCap)
                                          : QPen(ink, line_width, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(map(pin.left ? 0 : 1, 2.0 * pin.row), map(pin.left ? 1 : 2, 2.0 * pin.row));
            painter->setPen(ink);
            painter->drawText(label_rect(pin.left, pin.row, pin.label), Qt::AlignCenter, pin.label);
        }
    }

private:
    model::Splitter splitter_;
};

// Text note (UI-17): the text as typed, line by line, from its top-left corner.
class NoteItem : public QGraphicsItem {
public:
    NoteItem()
    {
        setFlag(ItemIsSelectable, true);
        setZValue(2);
    }

    void set(const model::TextNote& note)
    {
        if (note == note_) {
            return;
        }
        prepareGeometryChange();
        note_ = note;
        setPos(to_scene(note.position));
        setData(key_id, QVariant::fromValue<qulonglong>(note.id));
        update();
    }

    QRectF text_box() const
    {
        const QFontMetricsF metrics(schematic_font(false));
        const QRectF text = metrics.boundingRect(
            QRectF(0, 0, 4000, 4000), Qt::AlignLeft | Qt::AlignTop, QString::fromStdString(note_.text));
        return text.adjusted(-3, -2, 3, 2);
    }

    QRectF boundingRect() const override { return text_box().adjusted(-1, -1, 1, 1); }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        const QRectF box = text_box();
        if (isSelected()) {
            painter->setPen(QPen(theme::selection, 1.0, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(box);
        }
        painter->setPen(isSelected() ? theme::selection : theme::ink);
        painter->setFont(schematic_font(false));
        painter->drawText(box.adjusted(3, 2, -3, -2), Qt::AlignLeft | Qt::AlignTop, QString::fromStdString(note_.text));
    }

private:
    model::TextNote note_;
};

class ProbeItem : public QGraphicsItem {
public:
    ProbeItem()
    {
        setFlag(ItemIsSelectable, true);
        setZValue(4);
    }

    void set(const model::Probe& probe, const QPointF& at, int index)
    {
        prepareGeometryChange();
        probe_ = probe;
        color_ = probe.color.empty() ? probe_color(index) : QColor(QString::fromStdString(probe.color));
        setPos(at);
        setData(key_id, QVariant::fromValue<qulonglong>(probe.id));
        update();
    }

    QRectF boundingRect() const override { return QRectF(-2, -18, 20, 20); }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        const QColor color = isSelected() ? theme::selection : color_;
        painter->setPen(QPen(color, 1.2));
        painter->drawLine(QPointF(0, 0), QPointF(6, -6));
        painter->setBrush(color);
        painter->drawEllipse(QRectF(5, -16, 11, 11));
        painter->setPen(Qt::white);
        QFont font = schematic_font(true);
        font.setPixelSize(8);
        painter->setFont(font);
        painter->drawText(QRectF(5, -16, 11, 11), Qt::AlignCenter,
            probe_.kind == model::ProbeKind::Voltage ? QStringLiteral("V") : QStringLiteral("A"));
    }

private:
    model::Probe probe_;
    QColor color_;
};

class DecorationItem : public QGraphicsItem {
public:
    DecorationItem() { setZValue(5); }

    void set(std::vector<model::Point> junctions, std::vector<model::Point> open_ends)
    {
        prepareGeometryChange();
        junctions_ = std::move(junctions);
        open_ends_ = std::move(open_ends);
        bounds_ = QRectF();
        for (const auto& points : {junctions_, open_ends_}) {
            for (const auto& p : points) {
                const QRectF r(to_scene(p) - QPointF(5, 5), QSizeF(10, 10));
                bounds_ = bounds_.isNull() ? r : bounds_.united(r);
            }
        }
        update();
    }

    QRectF boundingRect() const override { return bounds_; }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(theme::ink);
        for (const auto& p : junctions_) {
            painter->drawEllipse(to_scene(p), 3.2, 3.2);
        }
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(QColor(0xd0, 0x30, 0x20), 1.2));
        for (const auto& p : open_ends_) {
            painter->drawEllipse(to_scene(p), 3.0, 3.0);
        }
    }

private:
    std::vector<model::Point> junctions_;
    std::vector<model::Point> open_ends_;
    QRectF bounds_;
};

// ---- Scene --------------------------------------------------------------

SchematicScene::SchematicScene(Document& document, QObject* parent) : QGraphicsScene(parent), document_(document)
{
    setSceneRect(-500 * scene_grid, -500 * scene_grid, 1000 * scene_grid, 1000 * scene_grid);
    setItemIndexMethod(QGraphicsScene::NoIndex);
    decorations_ = new DecorationItem;
    addItem(decorations_);
    shown_ = &document.schematic();
    connect(&document, &Document::changed, this, &SchematicScene::sync);
    sync();
}

const library::ComponentLibrary& SchematicScene::library() const
{
    return document_.library();
}

void SchematicScene::sync()
{
    shown_ = &document_.schematic();
    rebuild(*shown_);
}

void SchematicScene::sync_from(const model::Schematic& schematic)
{
    preview_copy_ = schematic;
    shown_ = &preview_copy_;
    rebuild(preview_copy_);
}

void SchematicScene::rebuild(const model::Schematic& s)
{
    connectivity_ = netlist::build_connectivity(s, library());
    auto prune = [this](auto& items, auto exists) {
        for (auto it = items.begin(); it != items.end();) {
            if (!exists(it->first)) {
                removeItem(it->second);
                delete it->second;
                it = items.erase(it);
            } else {
                ++it;
            }
        }
    };
    prune(components_, [&s](model::Id id) { return s.component(id) != nullptr; });
    prune(wires_, [&s](model::Id id) { return s.wire(id) != nullptr; });
    prune(labels_, [&s](model::Id id) { return s.label(id) != nullptr; });
    prune(probes_, [&s](model::Id id) { return s.probe(id) != nullptr; });
    prune(notes_, [&s](model::Id id) { return s.note(id) != nullptr; });
    prune(buses_, [&s](model::Id id) { return s.bus(id) != nullptr; });
    prune(splitters_, [&s](model::Id id) { return s.splitter(id) != nullptr; });
    for (const auto& c : s.components()) {
        auto& item = components_[c.id];
        if (!item) {
            item = new ComponentItem(*this, false);
            addItem(item);
        }
        item->set(c, library().find(c.type));
    }
    for (const auto& w : s.wires()) {
        auto& item = wires_[w.id];
        if (!item) {
            item = new WireItem(*this);
            addItem(item);
        }
        item->set(w);
    }
    for (const auto& l : s.labels()) {
        auto& item = labels_[l.id];
        if (!item) {
            item = new LabelItem;
            addItem(item);
        }
        item->set(l);
    }
    for (const auto& b : s.buses()) {
        auto& item = buses_[b.id];
        if (!item) {
            item = new BusItem;
            addItem(item);
        }
        item->set(b);
    }
    for (const auto& sp : s.splitters()) {
        auto& item = splitters_[sp.id];
        if (!item) {
            item = new SplitterItem;
            addItem(item);
        }
        item->set(sp);
    }
    for (const auto& n : s.notes()) {
        auto& item = notes_[n.id];
        if (!item) {
            item = new NoteItem;
            addItem(item);
        }
        item->set(n);
    }
    int index = 0;
    for (const auto& p : s.probes()) {
        auto& item = probes_[p.id];
        if (!item) {
            item = new ProbeItem;
            addItem(item);
        }
        QPointF at = to_scene(p.target.point);
        if (const auto* c = s.component(p.target.item)) {
            if (const auto* d = library().find(c->type)) {
                if (const auto* pin = d->pin(p.target.pin)) {
                    at = to_scene(library::pin_position(*d, *pin, *c));
                }
            }
        }
        item->set(p, at, index++);
    }
    std::vector<model::Point> open_ends = connectivity_.dangling_ends;
    for (const auto& pin : connectivity_.unconnected_pins) {
        if (const auto* c = s.component(pin.first)) {
            if (const auto* d = library().find(c->type); d && !d->mcu) { // unused board pins are normal
                if (const auto* p = d->pin(pin.second)) {
                    open_ends.push_back(library::pin_position(*d, *p, *c));
                }
            }
        }
    }
    decorations_->set(connectivity_.junctions, open_ends);
    emit synced();
}

std::set<model::Id> SchematicScene::selected_ids() const
{
    std::set<model::Id> ids;
    for (const auto* item : selectedItems()) {
        if (const model::Id id = id_of(item)) {
            ids.insert(id);
        }
    }
    return ids;
}

void SchematicScene::select_ids(const std::set<model::Id>& ids)
{
    QSignalBlocker blocker(this);
    clearSelection();
    for (model::Id id : ids) {
        if (auto* item = item_of(id)) {
            item->setSelected(true);
        }
    }
    blocker.unblock();
    emit selectionChanged();
}

model::Id SchematicScene::id_of(const QGraphicsItem* item) const
{
    return item ? item->data(key_id).value<qulonglong>() : model::no_id;
}

QGraphicsItem* SchematicScene::item_of(model::Id id) const
{
    if (auto it = components_.find(id); it != components_.end()) {
        return it->second;
    }
    if (auto it = wires_.find(id); it != wires_.end()) {
        return it->second;
    }
    if (auto it = labels_.find(id); it != labels_.end()) {
        return it->second;
    }
    if (auto it = probes_.find(id); it != probes_.end()) {
        return it->second;
    }
    if (auto it = notes_.find(id); it != notes_.end()) {
        return it->second;
    }
    if (auto it = buses_.find(id); it != buses_.end()) {
        return it->second;
    }
    if (auto it = splitters_.find(id); it != splitters_.end()) {
        return it->second;
    }
    return nullptr;
}

model::Id SchematicScene::element_at(const QPointF& scene_point) const
{
    for (const auto* item : items(scene_point, Qt::IntersectsItemShape, Qt::DescendingOrder)) {
        if (item == ghost_ || item == wire_preview_ || item == decorations_) {
            continue;
        }
        if (const model::Id id = id_of(item)) {
            return id;
        }
    }
    return model::no_id;
}

std::optional<std::pair<model::Id, bool>> SchematicScene::text_at(const QPointF& scene_point) const
{
    for (const auto& [id, item] : components_) {
        const QPointF local = item->mapFromScene(scene_point);
        const auto& c = item->component();
        if (!item->value_text().isEmpty() && item->text_rect(item->value_text(), item->text_offset(true)).contains(local)) {
            return std::make_pair(id, true);
        }
        if (item->show_designator()
            && item->text_rect(QString::fromStdString(c.designator), item->text_offset(false)).contains(local)) {
            return std::make_pair(id, false);
        }
    }
    return std::nullopt;
}

model::Point SchematicScene::text_offset(model::Id component, bool value) const
{
    const auto it = components_.find(component);
    return it == components_.end() ? model::Point{} : it->second->text_offset(value);
}

std::optional<netlist::PinRef> SchematicScene::pin_at(const model::Point& point) const
{
    for (const auto& c : shown_->components()) {
        if (const auto* d = library().find(c.type)) {
            for (const auto& pin : d->pins) {
                if (library::pin_position(*d, pin, c) == point) {
                    return netlist::PinRef{c.id, pin.name};
                }
            }
        }
    }
    return std::nullopt;
}

bool SchematicScene::wire_at(const model::Point& point, model::Id* wire) const
{
    for (const auto& w : shown_->wires()) {
        for (std::size_t i = 1; i < w.points.size(); ++i) {
            if (model::on_segment(point, w.points[i - 1], w.points[i])) {
                if (wire) {
                    *wire = w.id;
                }
                return true;
            }
        }
    }
    return false;
}

std::optional<std::string> SchematicScene::net_name_at(const model::Point& point) const
{
    const auto net = connectivity_.net_at(*shown_, library(), point);
    if (!net) {
        return std::nullopt;
    }
    return connectivity_.nets[static_cast<std::size_t>(*net)].name;
}

void SchematicScene::set_live(LiveState live)
{
    live_ = std::move(live);
    for (auto& [id, item] : components_) {
        item->refresh_live();
    }
    if (show_voltages_) {
        for (auto& [id, item] : wires_) {
            item->update();
        }
    }
}

void SchematicScene::clear_live()
{
    set_live({});
    for (auto& [id, item] : wires_) {
        item->update();
    }
}

void SchematicScene::set_show_voltages(bool show)
{
    show_voltages_ = show;
    for (auto& [id, item] : wires_) {
        item->update();
    }
}

void SchematicScene::set_ghost(const library::ComponentDefinition* definition, const model::Point& position,
    const model::Orientation& orientation)
{
    if (!definition) {
        clear_ghost();
        return;
    }
    if (!ghost_) {
        ghost_ = new ComponentItem(*this, true);
        addItem(ghost_);
    }
    model::Component c;
    c.type = definition->type;
    c.position = position;
    c.orientation = orientation;
    ghost_->set(c, definition);
}

void SchematicScene::clear_ghost()
{
    if (ghost_) {
        removeItem(ghost_);
        delete ghost_;
        ghost_ = nullptr;
    }
}

void SchematicScene::set_wire_preview(const std::vector<model::Point>& points)
{
    if (points.size() < 2) {
        if (wire_preview_) {
            removeItem(wire_preview_);
            delete wire_preview_;
            wire_preview_ = nullptr;
        }
        return;
    }
    if (!wire_preview_) {
        wire_preview_ = new WireItem(*this, true);
        addItem(wire_preview_);
    }
    wire_preview_->set(model::Wire{model::no_id, points});
}

} // namespace simnodus::ui
