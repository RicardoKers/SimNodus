// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "rc_canvas.hpp"
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <set>

namespace {
QString text(std::string_view value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
QStringList ids(const std::vector<std::string>& path)
{
    QStringList result;
    for(const auto& id : path) result << text(id);
    return result;
}
std::vector<std::string> native(const QStringList& path)
{
    std::vector<std::string> result;
    for(const auto& id : path) result.push_back(id.toUtf8().toStdString());
    return result;
}
QRectF hitBox(int index) { return index == 0 ? QRectF(190, 170, 130, 40) : QRectF(425, 240, 40, 80); }
QString endpointKind(simnodus::DeclaredEndpointKind kind)
{
    switch(kind) {
    case simnodus::DeclaredEndpointKind::component_pin: return "Component pin";
    case simnodus::DeclaredEndpointKind::local_port: return "Local circuit port";
    case simnodus::DeclaredEndpointKind::circuit_port: return "Subcircuit port";
    }
    return {};
}
}

RcCanvas::RcCanvas()
{
    setMinimumSize(360, 280);
    setAccessibleName("Fixed declared RC circuit; click an existing resistor or capacitor to inspect it");
}
QString RcCanvas::parameterCaption(const simnodus::EffectiveParameter& parameter)
{
    const auto raw = text(parameter.value);
    const auto fallback = raw + " " + text(parameter.unit);
    if((parameter.unit != "ohm" && parameter.unit != "F") || raw.isEmpty() || raw.size() > 128) return fallback;
    auto point = raw.indexOf('.');
    if(point < 0) point = raw.size();
    if(point == 0 || (point < raw.size() && (point == raw.size() - 1 || raw.indexOf('.', point + 1) >= 0)) ||
        (point > 1 && raw[0] == '0')) return fallback;
    for(const auto ch : raw) if(ch != '.' && (ch < '0' || ch > '9')) return fallback;
    auto digits = raw;
    digits.remove('.');
    qsizetype first = 0;
    while(first < digits.size() && digits[first] == '0') ++first;
    if(first == digits.size()) return fallback;
    const auto order = point - first - 1;
    int exponent;
    QString suffix;
    if(parameter.unit == "ohm") {
        if(order < 0 || order >= 9) return fallback;
        exponent = order >= 6 ? 6 : order >= 3 ? 3 : 0;
        const std::array<QString, 3> units{QStringLiteral("\u03A9"), QStringLiteral("k\u03A9"), QStringLiteral("M\u03A9")};
        suffix = units[static_cast<std::size_t>(exponent / 3)];
    } else {
        if(order < -9 || order >= 0) return fallback;
        exponent = order >= -3 ? -3 : order >= -6 ? -6 : -9;
        const std::array<QString, 3> units{"nF", QStringLiteral("\u00B5F"), "mF"};
        suffix = units[static_cast<std::size_t>((exponent + 9) / 3)];
    }
    // Exact decimal-point movement only; no rounded or binary floating value.
    point -= first + exponent;
    digits.remove(0, first);
    if(point <= 0) digits = "0." + QString(-point, '0') + digits;
    else if(point >= digits.size()) digits += QString(point - digits.size(), '0');
    else digits.insert(point, '.');
    if(digits.contains('.')) {
        while(digits.endsWith('0')) digits.chop(1);
        if(digits.endsWith('.')) digits.chop(1);
    }
    return digits + " " + suffix;
}
void RcCanvas::setGraph(std::shared_ptr<const simnodus::ProjectGraph> graph, const QStringList& context)
{
    graph_ = std::move(graph);
    if(context != context_) selected_.clear();
    context_ = context;
    supported_ = resolve();
    if(!supported_) selected_.clear();
    update();
}
bool RcCanvas::resolve()
{
    if(!graph_ || context_.size() != 2 || context_[0] != "main" || (context_[1] != "left" && context_[1] != "right") ||
        graph_->connectivity.root != "main") return false;
    const auto local = std::find_if(graph_->connectivity.circuits.begin(), graph_->connectivity.circuits.end(),
        [](const auto& circuit) { return circuit.identity.id == "rc"; });
    if(local == graph_->connectivity.circuits.end() || local->instances.size() != 2 || local->ports.size() != 3 || local->nets.size() != 3) return false;
    std::set<std::string> ports;
    for(const auto& port : local->ports) ports.insert(port.identity.id);
    if(ports != std::set<std::string>{"input", "output", "reference"}) return false;
    for(int i = 0; i < 2; ++i) {
        auto path = native(context_);
        path.push_back(i == 0 ? "r" : "c");
        const auto component = simnodus::inspect_component_occurrence(*graph_, path);
        const auto parameter = i == 0 ? "resistance" : "capacitance";
        const auto unit = i == 0 ? "ohm" : "F";
        if(!component || component->source_circuit != "rc" || component->component != (i == 0 ? "resistor" : "capacitor") ||
            component->terminals.size() != 2 || component->terminals[0].pin != "n" || component->terminals[1].pin != "p" ||
            !component->parameters.contains(parameter) || component->parameters.at(parameter).unit != unit ||
            component->parameters.at(parameter).origin != std::string("containing-circuit:") + parameter) return false;
        components_[static_cast<std::size_t>(i)] = *component;
    }
    const std::array<std::string, 3> net_ids{"drive", "junction", "return"};
    const std::array<std::string, 3> pins{"p", "n", "n"};
    using Endpoint = std::pair<simnodus::DeclaredEndpointKind, std::vector<std::string>>;
    const auto base = native(context_);
    const auto path = [&](const std::string& entity, const std::string& pin = {}) {
        auto result = base;
        result.push_back(entity);
        if(!pin.empty()) result.push_back(pin);
        return result;
    };
    const std::array<std::set<Endpoint>, 3> expected{{
        {{simnodus::DeclaredEndpointKind::local_port, path("input")}, {simnodus::DeclaredEndpointKind::component_pin, path("r", "p")}},
        {{simnodus::DeclaredEndpointKind::local_port, path("output")}, {simnodus::DeclaredEndpointKind::component_pin, path("r", "n")},
            {simnodus::DeclaredEndpointKind::component_pin, path("c", "p")}},
        {{simnodus::DeclaredEndpointKind::local_port, path("reference")}, {simnodus::DeclaredEndpointKind::component_pin, path("c", "n")}}
    }};
    for(std::size_t i = 0; i < nets_.size(); ++i) {
        const auto details = simnodus::inspect_occurrence_local_net(*graph_, components_[i == 2 ? 1 : 0].path, pins[i]);
        if(!details || details->net.id != net_ids[i] || details->net.path != path(net_ids[i])) return false;
        std::set<Endpoint> members;
        for(const auto& endpoint : details->endpoints) members.emplace(endpoint.kind, endpoint.path);
        if(members != expected[i] || members.size() != details->endpoints.size()) return false;
        nets_[i] = *details;
    }
    return true;
}
void RcCanvas::setSelection(const QStringList& path)
{
    selected_.clear();
    if(supported_) for(const auto& component : components_) if(ids(component.path) == path) selected_ = path;
    update();
}
QRectF RcCanvas::fittedView() const
{
    const auto scale = std::max(0.0, std::min((width() - 24.0) / 700, (height() - 24.0) / 420));
    return {(width() - 700 * scale) / 2, (height() - 420 * scale) / 2, 700 * scale, 420 * scale};
}
QPoint RcCanvas::componentPoint(const QString& id) const
{
    const auto view = fittedView();
    const auto point = hitBox(id == "r" ? 0 : 1).center();
    return QPointF(view.left() + point.x() * view.width() / 700, view.top() + point.y() * view.height() / 420).toPoint();
}
void RcCanvas::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) return;
    // Reconfirm the current owned graph before accepting a painted target.
    supported_ = resolve();
    const auto view = fittedView();
    if(!supported_ || view.isEmpty()) { selected_.clear(); update(); return; }
    const QPointF point((event->position().x() - view.left()) * 700 / view.width(),
        (event->position().y() - view.top()) * 420 / view.height());
    for(int i = 0; i < 2; ++i) if(hitBox(i).contains(point)) {
        setSelection(ids(components_[static_cast<std::size_t>(i)].path));
        if(selected) selected(selected_);
        return;
    }
}
void RcCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    if(!supported_) {
        painter.setPen(QColor(90, 100, 115));
        painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
            graph_ ? "Circuit diagram unavailable for this declaration.\nUse Details to inspect its structure." : "Open a project to view its circuit.");
        return;
    }
    const auto view = fittedView();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(view.topLeft());
    painter.scale(view.width() / 700, view.height() / 420);
    QFont font = painter.font();
    font.setPixelSize(17);
    painter.setFont(font);
    painter.setPen(QColor(65, 75, 90));
    painter.drawText(QPointF(70, 66), "RC / " + context_[1]);
    painter.setPen(QPen(Qt::black, 2));
    for(std::size_t i = 0; i < components_.size(); ++i) if(ids(components_[i].path) == selected_) {
        painter.fillRect(hitBox(static_cast<int>(i)).adjusted(-8, -8, 8, 8), QColor(223, 237, 255));
        painter.setPen(QPen(QColor(35, 103, 185), 1));
        painter.drawRect(hitBox(static_cast<int>(i)).adjusted(-8, -8, 8, 8));
        painter.setPen(QPen(Qt::black, 2));
    }
    // Three exact local nets, not a flattened circuit or an inferred ground.
    painter.drawLine(QPointF(70, 190), QPointF(210, 190));
    painter.drawLine(QPointF(300, 190), QPointF(625, 190));
    painter.drawLine(QPointF(445, 190), QPointF(445, 240));
    painter.drawLine(QPointF(445, 320), QPointF(445, 350));
    painter.drawLine(QPointF(445, 350), QPointF(70, 350));
    QPainterPath resistor;
    resistor.moveTo(210, 190);
    for(int i = 0; i < 8; ++i) resistor.lineTo(216 + 10 * i, i % 2 == 0 ? 178 : 202);
    resistor.lineTo(300, 190);
    painter.drawPath(resistor);
    painter.drawLine(QPointF(445, 240), QPointF(445, 265));
    painter.drawLine(QPointF(425, 265), QPointF(465, 265));
    painter.drawLine(QPointF(425, 280), QPointF(465, 280));
    painter.drawLine(QPointF(445, 280), QPointF(445, 320));
    painter.setBrush(Qt::black);
    painter.drawEllipse(QPointF(445, 190), 3, 3);
    painter.setBrush(Qt::white);
    for(const auto& point : {QPointF(70, 190), QPointF(625, 190), QPointF(70, 350)}) painter.drawEllipse(point, 4, 4);
    const auto portName = [this](std::size_t net) {
        for(const auto& endpoint : nets_[net].endpoints) if(endpoint.kind == simnodus::DeclaredEndpointKind::local_port) return text(endpoint.name);
        return QString{};
    };
    painter.drawText(QPointF(70, 168), portName(0));
    painter.drawText(QRectF(495, 140, 135, 35), Qt::AlignRight | Qt::AlignVCenter, portName(1));
    painter.drawText(QPointF(70, 383), portName(2));
    font.setPixelSize(22);
    painter.setFont(font);
    painter.drawText(QPointF(210, 135), text(components_[0].name));
    painter.drawText(QPointF(480, 260), text(components_[1].name));
    font.setPixelSize(16);
    painter.setFont(font);
    const auto& r = components_[0].parameters.at("resistance");
    const auto& c = components_[1].parameters.at("capacitance");
    painter.drawText(QPointF(210, 159), parameterCaption(r));
    painter.drawText(QPointF(480, 288), parameterCaption(c));
}
QJsonObject RcCanvas::snapshot() const
{
    QJsonArray components, nets;
    if(supported_) {
        for(const auto& component : components_) {
            QJsonObject parameters;
            for(const auto& [id, parameter] : component.parameters) parameters[text(id)] = QJsonObject{
                {"value", text(parameter.value)}, {"unit", text(parameter.unit)}, {"origin", text(parameter.origin)}};
            QJsonArray terminals;
            for(const auto& terminal : component.terminals) terminals.append(QJsonObject{{"pin", text(terminal.pin)}, {"name", text(terminal.name)},
                {"net_path", QJsonArray::fromStringList(terminal.net ? ids(terminal.net->path) : QStringList{})}});
            components.append(QJsonObject{{"path", QJsonArray::fromStringList(ids(component.path))}, {"name", text(component.name)},
                {"component", text(component.component)}, {"parameters", parameters}, {"terminals", terminals},
                {"caption", parameterCaption(component.parameters.at(component.component == "resistor" ? "resistance" : "capacitance"))}});
        }
        for(const auto& net : nets_) {
            QJsonArray endpoints;
            for(const auto& endpoint : net.endpoints) endpoints.append(QJsonObject{{"kind", endpointKind(endpoint.kind)},
                {"instance", text(endpoint.instance)}, {"terminal", text(endpoint.terminal)}, {"name", text(endpoint.name)},
                {"definition", text(endpoint.definition)}, {"path", QJsonArray::fromStringList(ids(endpoint.path))}});
            nets.append(QJsonObject{{"id", text(net.net.id)}, {"name", text(net.net.name)},
                {"path", QJsonArray::fromStringList(ids(net.net.path))}, {"endpoints", endpoints}});
        }
    }
    return {{"supported", supported_}, {"context", QJsonArray::fromStringList(context_)},
        {"selected_path", QJsonArray::fromStringList(selected_)}, {"components", components}, {"nets", nets}};
}
