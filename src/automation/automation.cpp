// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "automation.hpp"

#include "editing/operations.hpp"
#include "io/project_file.hpp"
#include "library/blocks.hpp"
#include "model/engineering.hpp"
#include "netlist/connectivity.hpp"
#include "netlist/netlist.hpp"
#include "sim/measurements.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <thread>

namespace simnodus::automation {

// ---------------------------------------------------------------- workspace

HeadlessWorkspace::HeadlessWorkspace(const library::ComponentLibrary& base)
    : base_(base), library_(std::make_unique<library::ComponentLibrary>(base))
{
}

HeadlessWorkspace::~HeadlessWorkspace()
{
    if (session_) {
        session_->stop();
    }
}

void HeadlessWorkspace::apply(model::Edit edit)
{
    if (edit.empty()) {
        return;
    }
    edit.apply(schematic_);
    done_.push_back(std::move(edit));
    undone_.clear();
    modified_ = true;
    refresh_library();
}

bool HeadlessWorkspace::undo()
{
    if (done_.empty()) {
        return false;
    }
    done_.back().revert(schematic_);
    undone_.push_back(std::move(done_.back()));
    done_.pop_back();
    modified_ = true;
    refresh_library();
    return true;
}

bool HeadlessWorkspace::redo()
{
    if (undone_.empty()) {
        return false;
    }
    undone_.back().apply(schematic_);
    done_.push_back(std::move(undone_.back()));
    undone_.pop_back();
    modified_ = true;
    refresh_library();
    return true;
}

std::filesystem::path HeadlessWorkspace::base_dir() const
{
    if (file_.empty()) {
        std::error_code ec;
        return std::filesystem::current_path(ec);
    }
    return std::filesystem::absolute(file_).parent_path();
}

void HeadlessWorkspace::replace(model::Schematic schematic, const std::filesystem::path& file)
{
    if (session_) {
        session_->stop();
    }
    schematic_ = std::move(schematic);
    file_ = file;
    done_.clear();
    undone_.clear();
    modified_ = false;
    refresh_library();
}

void HeadlessWorkspace::saved(const std::filesystem::path& file)
{
    file_ = file;
    modified_ = false;
}

sim::Session& HeadlessWorkspace::session()
{
    if (!session_) {
        session_ = std::make_unique<sim::SimulationSession>();
    }
    return *session_;
}

void HeadlessWorkspace::refresh_library()
{
    library_ = std::make_unique<library::ComponentLibrary>(library::with_blocks(base_, schematic_));
}

// ---------------------------------------------------------------- helpers

namespace {

using model::Id;
using model::Point;

struct Failure {
    std::string code;
    std::string message;
};

[[noreturn]] void fail(std::string code, std::string message)
{
    throw Failure{std::move(code), std::move(message)};
}

std::filesystem::path path_from(const std::string& utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string utf8(const std::filesystem::path& path)
{
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool iequals(const std::string& a, const std::string& b)
{
    return lower(a) == lower(b);
}

const json& required(const json& args, const char* key)
{
    const auto it = args.find(key);
    if (it == args.end() || it->is_null()) {
        fail("invalid-argument", std::string("Missing argument \"") + key + "\".");
    }
    return *it;
}

std::string text(const json& args, const char* key)
{
    const auto& v = required(args, key);
    if (!v.is_string()) {
        fail("invalid-argument", std::string("Argument \"") + key + "\" must be a string.");
    }
    return v.get<std::string>();
}

std::optional<std::string> optional_text(const json& args, const char* key)
{
    if (!args.contains(key) || args[key].is_null()) {
        return std::nullopt;
    }
    return text(args, key);
}

// A number, or engineering text with an optional unit ("10m", "10ms", "4.7k").
double quantity(const json& v, const char* key, const char* unit)
{
    if (v.is_number()) {
        return v.get<double>();
    }
    if (v.is_string()) {
        if (const auto parsed = model::parse_engineering(v.get<std::string>(), unit)) {
            return *parsed;
        }
    }
    fail("invalid-argument", std::string("Argument \"") + key + "\" must be a number (\"10m\" and \"10ms\" are accepted).");
}

std::optional<double> optional_quantity(const json& args, const char* key, const char* unit)
{
    if (!args.contains(key) || args[key].is_null()) {
        return std::nullopt;
    }
    return quantity(args[key], key, unit);
}

Point point_from(const json& v, const char* key)
{
    if (!v.is_array() || v.size() != 2 || !v[0].is_number_integer() || !v[1].is_number_integer()) {
        fail("invalid-argument", std::string("Argument \"") + key + "\" must be a grid point [x, y] of integers.");
    }
    return {v[0].get<int>(), v[1].get<int>()};
}

json point_json(Point p)
{
    return json::array({p.x, p.y});
}

const char* severity_name(netlist::Severity s)
{
    switch (s) {
    case netlist::Severity::Info:
        return "info";
    case netlist::Severity::Warning:
        return "warning";
    default:
        return "error";
    }
}

const char* kind_name(library::Parameter::Kind k)
{
    switch (k) {
    case library::Parameter::Kind::Choice:
        return "choice";
    case library::Parameter::Kind::File:
        return "file";
    case library::Parameter::Kind::Model:
        return "model";
    case library::Parameter::Kind::Text:
        return "text";
    default:
        return "number";
    }
}

const char* direction_name(library::PinDirection d)
{
    switch (d) {
    case library::PinDirection::Left:
        return "left";
    case library::PinDirection::Right:
        return "right";
    case library::PinDirection::Down:
        return "down";
    default:
        return "up";
    }
}

Point direction_vector(library::PinDirection d)
{
    switch (d) {
    case library::PinDirection::Left:
        return {-1, 0};
    case library::PinDirection::Right:
        return {1, 0};
    case library::PinDirection::Down:
        return {0, 1};
    default:
        return {0, -1};
    }
}

// Bounding box of a symbol in its own coordinates: graphics and pins.
std::array<double, 4> symbol_bounds(const library::ComponentDefinition& d)
{
    double x0 = std::numeric_limits<double>::max();
    double y0 = x0;
    double x1 = -x0;
    double y1 = -x0;
    auto add = [&](double x, double y) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
    };
    for (const auto& g : d.graphics) {
        using K = library::Graphic::Kind;
        if (g.points.empty()) {
            continue;
        }
        if (g.kind == K::Rect && g.points.size() >= 2) {
            add(g.points[0].x, g.points[0].y);
            add(g.points[0].x + g.points[1].x, g.points[0].y + g.points[1].y);
        } else if (g.kind == K::Circle || g.kind == K::Arc) {
            add(g.points[0].x - g.radius, g.points[0].y - g.radius);
            add(g.points[0].x + g.radius, g.points[0].y + g.radius);
        } else {
            for (const auto& p : g.points) {
                add(p.x, p.y);
            }
        }
    }
    for (const auto& p : d.pins) {
        add(p.at.x, p.at.y);
    }
    if (x0 > x1) {
        return {0, 0, 0, 0};
    }
    return {std::floor(x0), std::floor(y0), std::ceil(x1), std::ceil(y1)};
}

// The box in schematic coordinates for a placed component.
json placed_bounds(const library::ComponentDefinition& d, const model::Component& c)
{
    const auto b = symbol_bounds(d);
    double x0 = std::numeric_limits<double>::max();
    double y0 = x0;
    double x1 = -x0;
    double y1 = -x0;
    for (const auto& corner : {model::PointF{b[0], b[1]}, model::PointF{b[2], b[3]}}) {
        const auto p = model::transform(corner, c.position, c.orientation);
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    return json::array({std::lround(x0), std::lround(y0), std::lround(x1), std::lround(y1)});
}

std::string effective(const library::ComponentDefinition& d, const model::Component& c, const library::Parameter& p)
{
    (void)d;
    const auto it = c.parameters.find(p.name);
    return it == c.parameters.end() ? p.default_value : it->second;
}

// ---------------------------------------------------------------- context

struct Context {
    Workspace& ws;
    std::unique_ptr<Automation::Run>& run;

    const model::Schematic& schematic() const { return ws.schematic(); }
    const library::ComponentLibrary& library() const { return ws.library(); }

    const library::ComponentDefinition& definition(const model::Component& c) const
    {
        const auto* d = library().find(c.type);
        if (!d) {
            fail("not-found", "The library has no part of type \"" + c.type + "\" (used by " + c.designator + ").");
        }
        return *d;
    }

    // A component by designator ("R1") or ID.
    const model::Component& component(const json& ref) const
    {
        const model::Component* c = nullptr;
        if (ref.is_number_unsigned() || ref.is_number_integer()) {
            c = schematic().component(ref.get<Id>());
        } else if (ref.is_string()) {
            const auto name = ref.get<std::string>();
            c = schematic().component_by_designator(name);
            if (!c) {
                for (const auto& candidate : schematic().components()) {
                    if (iequals(candidate.designator, name)) {
                        c = &candidate;
                        break;
                    }
                }
            }
        }
        if (!c) {
            fail("not-found", "No component " + ref.dump() + ".");
        }
        return *c;
    }

    // "R1.2" -> R1 and its pin "2".
    std::pair<const model::Component*, const library::Pin*> pin(const std::string& ref) const
    {
        const auto dot = ref.find('.');
        if (dot == std::string::npos || dot == 0 || dot + 1 >= ref.size()) {
            fail("invalid-argument", "A pin is written DESIGNATOR.PIN, like \"R1.2\"; got \"" + ref + "\".");
        }
        const auto& c = component(json(ref.substr(0, dot)));
        const auto& d = definition(c);
        const auto name = ref.substr(dot + 1);
        const auto* p = d.pin(name);
        if (!p) {
            for (const auto& candidate : d.pins) {
                if (iequals(candidate.name, name)) {
                    p = &candidate;
                    break;
                }
            }
        }
        if (!p) {
            std::string names;
            for (const auto& candidate : d.pins) {
                names += (names.empty() ? "" : ", ") + candidate.name;
            }
            fail("not-found", c.designator + " has no pin \"" + name + "\" (pins: " + names + ").");
        }
        return {&c, p};
    }

    Point pin_point(const std::string& ref) const
    {
        const auto [c, p] = pin(ref);
        return library::pin_position(definition(*c), *p, *c);
    }

    // A pin reference or a grid point.
    Point location(const json& v, const char* key) const
    {
        if (v.is_string()) {
            return pin_point(v.get<std::string>());
        }
        return point_from(v, key);
    }

    std::set<Id> items(const json& list) const
    {
        if (!list.is_array()) {
            fail("invalid-argument", "Argument \"items\" must be a list of designators or IDs.");
        }
        std::set<Id> ids;
        for (const auto& ref : list) {
            if (ref.is_number_integer() || ref.is_number_unsigned()) {
                const auto id = ref.get<Id>();
                if (!schematic().contains(id)) {
                    fail("not-found", "No element with ID " + std::to_string(id) + ".");
                }
                ids.insert(id);
            } else {
                ids.insert(component(ref).id);
            }
        }
        return ids;
    }

    void apply(model::Edit edit) const
    {
        if (!edit.empty()) {
            ws.apply(std::move(edit));
        }
    }
};

std::string pin_name(const model::Schematic& schematic, const netlist::PinRef& ref)
{
    const auto* c = schematic.component(ref.first);
    return (c ? c->designator : std::to_string(ref.first)) + "." + ref.second;
}

json diagnostics_json(const model::Schematic& schematic, const std::vector<netlist::Diagnostic>& list)
{
    json out = json::array();
    for (const auto& d : list) {
        json items = json::array();
        for (Id id : d.items) {
            if (const auto* c = schematic.component(id)) {
                items.push_back(c->designator);
            } else {
                items.push_back(id);
            }
        }
        json entry{{"severity", severity_name(d.severity)}, {"code", d.code}, {"message", d.message}, {"items", items}};
        if (d.at) {
            entry["at"] = point_json(*d.at);
        }
        out.push_back(entry);
    }
    return out;
}

// Parameter values: valid for the part, or a failure naming the problem.
std::string checked_value(const model::Component& c, const library::ComponentDefinition& d, const library::Parameter& p,
    const json& value)
{
    std::string v;
    if (value.is_string()) {
        v = value.get<std::string>();
    } else if (value.is_number()) {
        std::ostringstream s;
        s << value.get<double>();
        v = s.str();
    } else if (value.is_boolean()) {
        v = value.get<bool>() ? "yes" : "no";
    } else {
        fail("invalid-argument", "The value of " + c.designator + " " + p.name + " must be text or a number.");
    }
    if (!p.choices.empty()) {
        for (const auto& choice : p.choices) {
            if (iequals(choice, v)) {
                return choice;
            }
        }
        std::string list;
        for (const auto& choice : p.choices) {
            list += (list.empty() ? "" : ", ") + choice;
        }
        fail("invalid-argument", c.designator + " " + p.name + " is one of: " + list + ".");
    }
    if (p.kind == library::Parameter::Kind::Number) {
        const auto parsed = model::parse_engineering(v, p.unit);
        if (!parsed) {
            fail("invalid-argument", "\"" + v + "\" is not a number for " + c.designator + " " + p.name + " (unit " +
                    (p.unit.empty() ? std::string("none") : p.unit) + ").");
        }
        if ((p.min && *parsed < *p.min) || (p.max && *parsed > *p.max)) {
            fail("invalid-argument", c.designator + " " + p.name + " must be within " +
                    model::format_engineering(p.min.value_or(-1e300)) + " .. " + model::format_engineering(p.max.value_or(1e300)) +
                    ".");
        }
    }
    (void)d;
    return v;
}

// Sets parameters of `id` on `work` (models of type Model through set_model).
void set_parameters(Context& cx, model::Schematic& work, Id id, const json& parameters)
{
    if (!parameters.is_object()) {
        fail("invalid-argument", "Argument \"parameters\" must be an object {name: value}.");
    }
    for (const auto& [name, value] : parameters.items()) {
        const auto* c = work.component(id);
        const auto& d = cx.definition(*c);
        const library::Parameter* p = d.parameter(name);
        if (!p) {
            for (const auto& candidate : d.parameters) {
                if (iequals(candidate.name, name)) {
                    p = &candidate;
                }
            }
        }
        if (!p) {
            std::string names;
            for (const auto& candidate : d.parameters) {
                names += (names.empty() ? "" : ", ") + candidate.name;
            }
            fail("not-found", c->designator + " (" + d.type + ") has no parameter \"" + name + "\" (parameters: " +
                    (names.empty() ? std::string("none") : names) + ").");
        }
        const auto v = checked_value(*c, d, *p, value);
        model::Edit edit;
        if (p->kind == library::Parameter::Kind::Model) {
            if (!cx.library().model(p->model_type, v)) {
                fail("not-found", "No built-in " + p->model_type + " model named \"" + v + "\".");
            }
            edit = editing::set_model(work, id, p->name, v, "");
        } else {
            edit = editing::set_parameter(work, id, p->name, v);
        }
        edit.apply(work);
    }
}

json component_json(Context& cx, const model::Component& c, const netlist::Connectivity* connectivity)
{
    const auto* d = cx.library().find(c.type);
    json pins = json::array();
    json parameters = json::object();
    if (d) {
        for (const auto& p : d->pins) {
            json pin{{"name", p.name}, {"at", point_json(library::pin_position(*d, p, c))}};
            if (connectivity) {
                const auto it = connectivity->pin_net.find({c.id, p.name});
                pin["net"] = it == connectivity->pin_net.end()
                    ? json(nullptr)
                    : json(connectivity->nets[static_cast<std::size_t>(it->second)].name);
            }
            pins.push_back(pin);
        }
        for (const auto& p : d->parameters) {
            parameters[p.name] = effective(*d, c, p);
        }
    }
    json out{{"id", c.id}, {"designator", c.designator}, {"type", c.type}, {"at", point_json(c.position)},
        {"rotation", c.orientation.rotation}, {"mirrored", c.orientation.mirrored}, {"parameters", parameters},
        {"pins", pins}};
    if (d) {
        out["bounds"] = placed_bounds(*d, c);
    }
    return out;
}

// ---------------------------------------------------------------- signals

struct Signal {
    std::string name;      // as asked, normalized: "V(OUT)", "V(R1.2,R2.1)", "I(R1.1)"
    bool current = false;
    std::string a;         // voltage: pin ref or net name; current: pin ref
    std::string b;         // voltage reference; empty = ground
    std::string vector;    // resolved after start
    std::string reference; // resolved reference vector; empty or "0" = ground
    bool valid = false;
    std::string problem;
};

Signal parse_signal(const std::string& spec)
{
    Signal s;
    std::string t = spec;
    t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char ch) { return std::isspace(ch); }), t.end());
    std::string inner = t;
    if (t.size() > 3 && (t[0] == 'V' || t[0] == 'v' || t[0] == 'I' || t[0] == 'i') && t[1] == '(' && t.back() == ')') {
        s.current = t[0] == 'I' || t[0] == 'i';
        inner = t.substr(2, t.size() - 3);
    }
    const auto comma = inner.find(',');
    s.a = inner.substr(0, comma);
    if (comma != std::string::npos) {
        s.b = inner.substr(comma + 1);
    }
    if (s.a.empty() || (s.current && !s.b.empty())) {
        fail("invalid-argument", "A signal is V(NET), V(R1.2), V(A,B) or I(R1.1); got \"" + spec + "\".");
    }
    s.name = std::string(s.current ? "I(" : "V(") + s.a + (s.b.empty() ? "" : "," + s.b) + ")";
    return s;
}

std::optional<std::string> net_vector(Context& cx, const netlist::Netlist& n, const std::string& ref)
{
    if (ref == "0" || iequals(ref, "gnd")) {
        return std::string("0");
    }
    if (ref.find('.') != std::string::npos) {
        const auto dot = ref.find('.');
        const auto* c = cx.schematic().component_by_designator(ref.substr(0, dot));
        if (c) {
            const auto it = n.pin_node.find({c->id, ref.substr(dot + 1)});
            if (it != n.pin_node.end()) {
                return lower(it->second);
            }
            for (const auto& [pin, node] : n.pin_node) {
                if (pin.first == c->id && iequals(pin.second, ref.substr(dot + 1))) {
                    return lower(node);
                }
            }
            return std::nullopt;
        }
    }
    for (const auto& net : n.connectivity.nets) {
        if (iequals(net.name, ref) || std::any_of(net.label_names.begin(), net.label_names.end(),
                                          [&](const std::string& l) { return iequals(l, ref); })) {
            return net.ground ? std::string("0") : lower(net.name);
        }
    }
    return std::nullopt;
}

void resolve(Context& cx, const netlist::Netlist& n, Signal& s)
{
    if (s.current) {
        const auto dot = s.a.find('.');
        const auto* c = dot == std::string::npos ? nullptr : cx.schematic().component_by_designator(s.a.substr(0, dot));
        if (c) {
            for (const auto& [pin, vector] : n.pin_current) {
                if (pin.first == c->id && iequals(pin.second, s.a.substr(dot + 1))) {
                    s.vector = lower(vector);
                    s.valid = true;
                    return;
                }
            }
        }
        s.problem = "no current probe on " + s.a;
        return;
    }
    const auto a = net_vector(cx, n, s.a);
    const auto b = s.b.empty() ? std::optional<std::string>("0") : net_vector(cx, n, s.b);
    if (!a || !b) {
        s.problem = "no net or pin " + (!a ? s.a : s.b);
        return;
    }
    s.vector = *a;
    s.reference = *b;
    s.valid = true;
}

// Signals of the project's probes, or of the named nets when it has none.
std::vector<std::string> default_signals(Context& cx)
{
    std::vector<std::string> specs;
    const auto& schematic = cx.schematic();
    const auto connectivity = netlist::build_connectivity(schematic, cx.library());
    for (const auto& p : schematic.probes()) {
        const auto* c = schematic.component(p.target.item);
        if (p.kind == model::ProbeKind::Current) {
            if (c) {
                specs.push_back("I(" + c->designator + "." + p.target.pin + ")");
            }
            continue;
        }
        auto net_of = [&](const model::Anchor& a) -> std::optional<std::string> {
            if (const auto* pc = schematic.component(a.item)) {
                const auto it = connectivity.pin_net.find({pc->id, a.pin});
                if (it != connectivity.pin_net.end()) {
                    return connectivity.nets[static_cast<std::size_t>(it->second)].name;
                }
                return std::nullopt;
            }
            const auto it = connectivity.wire_net.find(a.item);
            if (it != connectivity.wire_net.end()) {
                return connectivity.nets[static_cast<std::size_t>(it->second)].name;
            }
            return std::nullopt;
        };
        const auto a = net_of(p.target);
        if (!a) {
            continue;
        }
        std::string spec = "V(" + *a;
        if (p.reference) {
            if (const auto b = net_of(*p.reference)) {
                spec += "," + *b;
            }
        }
        specs.push_back(spec + ")");
    }
    if (specs.empty()) {
        for (const auto& net : connectivity.nets) {
            if (!net.ground && !net.labels.empty()) {
                specs.push_back("V(" + net.name + ")");
            }
        }
    }
    return specs;
}

} // namespace

// ---------------------------------------------------------------- run

struct Automation::Run {
    model::SimulationMode mode = model::SimulationMode::Interactive;
    std::vector<Signal> signals;
    std::vector<std::string> messages; // engine and board messages
};

namespace {

std::vector<sim::Sample> raw_series(sim::Session& session, model::SimulationMode mode, const std::string& vector)
{
    if (vector == "0") {
        return {};
    }
    if (mode == model::SimulationMode::Transient) {
        const auto result = session.transient_result();
        const auto it = result.vectors.find(vector);
        if (it == result.vectors.end()) {
            return {};
        }
        std::vector<sim::Sample> out;
        const auto n = std::min(it->second.size(), result.time.size());
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            out.push_back({result.time[i], it->second[i]});
        }
        return out;
    }
    return session.samples(vector);
}

std::vector<sim::Sample> series(sim::Session& session, model::SimulationMode mode, const Signal& s)
{
    auto values = raw_series(session, mode, s.vector);
    if (s.reference.empty() || s.reference == "0") {
        return values;
    }
    auto reference = raw_series(session, mode, s.reference);
    if (s.vector == "0") {
        for (auto& r : reference) {
            r.value = -r.value;
        }
        return reference;
    }
    // Both vectors come from the same accepted time points.
    const std::size_t n = std::min(values.size(), reference.size());
    const std::size_t skip_v = values.size() - n;
    const std::size_t skip_r = reference.size() - n;
    std::vector<sim::Sample> diff;
    diff.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        diff.push_back({values[skip_v + i].time, values[skip_v + i].value - reference[skip_r + i].value});
    }
    return diff;
}

json number_or_null(const std::optional<double>& v)
{
    return v ? json(*v) : json(nullptr);
}

json status_json(const sim::RunStatus& s)
{
    json out{{"state", sim::to_string(s.state)}, {"mode", s.mode == model::SimulationMode::Transient ? "transient" : "interactive"},
        {"time", s.sim_time}, {"speed", s.speed}, {"real_time_factor", s.achieved_factor}};
    if (s.mode == model::SimulationMode::Transient) {
        out["progress"] = s.progress;
    }
    if (!s.message.empty()) {
        out["message"] = s.message;
    }
    if (!s.engine_output.empty()) {
        out["engine_output"] = s.engine_output;
    }
    return out;
}

void collect_messages(Context& cx)
{
    if (!cx.run) {
        return;
    }
    for (const auto& [board, message] : cx.ws.session().take_mcu_diagnostics()) {
        const auto* c = cx.schematic().component(board);
        cx.run->messages.push_back((c ? c->designator + ": " : std::string()) + message);
    }
}

Automation::Run& active_run(Context& cx)
{
    if (!cx.run) {
        fail("not-running", "No simulation was started; use sim.start or sim.run.");
    }
    return *cx.run;
}

// Starts a run of the workspace's project.
void start_run(Context& cx, const json& args)
{
    auto& session = cx.ws.session();
    session.stop();
    cx.run.reset();
    model::Schematic schematic = cx.schematic();
    auto settings = schematic.settings();
    if (const auto mode = optional_text(args, "mode")) {
        if (iequals(*mode, "transient")) {
            settings.mode = model::SimulationMode::Transient;
        } else if (iequals(*mode, "interactive")) {
            settings.mode = model::SimulationMode::Interactive;
        } else {
            fail("invalid-argument", "Argument \"mode\" is \"interactive\" or \"transient\".");
        }
    }
    if (const auto stop = optional_quantity(args, "duration", "s"); stop && settings.mode == model::SimulationMode::Transient) {
        settings.transient_stop = *stop;
    }
    // Without a window nobody watches: as fast as possible unless asked.
    settings.speed = optional_quantity(args, "speed", "").value_or(0.0);
    schematic.set_settings(settings);

    auto run = std::make_unique<Automation::Run>();
    run->mode = settings.mode;
    std::vector<std::string> specs;
    if (args.contains("signals") && !args["signals"].is_null()) {
        if (!args["signals"].is_array()) {
            fail("invalid-argument", "Argument \"signals\" must be a list like [\"V(OUT)\", \"I(R1.1)\"].");
        }
        for (const auto& s : args["signals"]) {
            if (!s.is_string()) {
                fail("invalid-argument", "Each signal must be a string.");
            }
            specs.push_back(s.get<std::string>());
        }
    } else {
        specs = default_signals(cx);
    }
    for (const auto& spec : specs) {
        run->signals.push_back(parse_signal(spec));
    }

    sim::RunRequest request;
    request.schematic = schematic;
    request.library = &cx.library();
    const auto connectivity = netlist::build_connectivity(schematic, cx.library());
    for (const auto& net : connectivity.nets) {
        if (!net.ground) {
            request.vectors.push_back(lower(net.name));
        }
    }
    for (const auto& p : schematic.probes()) {
        if (p.kind == model::ProbeKind::Current && schematic.component(p.target.item)) {
            request.current_probes.push_back({p.target.item, p.target.pin});
        }
    }
    for (const auto& s : run->signals) {
        if (s.current) {
            const auto [c, pin] = cx.pin(s.a);
            const netlist::PinRef ref{c->id, pin->name};
            if (std::find(request.current_probes.begin(), request.current_probes.end(), ref) == request.current_probes.end()) {
                request.current_probes.push_back(ref);
            }
        }
    }
    if (const auto history = optional_quantity(args, "history", "")) {
        request.ring_capacity = static_cast<std::size_t>(std::clamp(*history, 1000.0, 5e6));
    }
    sim::McuEnvironment environment;
    environment.project_dir = cx.ws.base_dir();
    std::string reason;
    std::vector<std::string> warnings;
    if (!sim::build_mcu_requests(schematic, cx.library(), environment, request.mcus, reason, &warnings)) {
        fail("run-failed", reason);
    }
    run->messages = warnings;
    if (!session.start(std::move(request), reason)) {
        const auto& found = session.diagnostics();
        if (std::any_of(found.begin(), found.end(),
                [](const netlist::Diagnostic& d) { return d.severity == netlist::Severity::Error; })) {
            throw Failure{"circuit-errors", "The circuit has errors: " +
                    diagnostics_json(cx.schematic(), session.diagnostics()).dump()};
        }
        fail("run-failed", reason);
    }
    for (auto& s : run->signals) {
        resolve(cx, session.netlist(), s);
    }
    cx.run = std::move(run);
}

// Lets an interactive run reach simulated time `target` and pauses it there
// (at the first accepted point at or after it). Returns the time reached.
double advance_to(Context& cx, double target, double timeout_s)
{
    auto& session = cx.ws.session();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_s);
    if (session.status().sim_time >= target) {
        session.pause();
    } else {
        session.pause_at(target);
        if (session.status().state == sim::RunState::Paused) {
            session.resume();
        }
        for (;;) {
            const auto st = session.status();
            if (!session.active() || (st.state == sim::RunState::Paused && st.sim_time >= target)) {
                break;
            }
            if (std::chrono::steady_clock::now() > deadline) {
                session.pause_at(-1.0);
                session.pause();
                fail("timeout", "The run did not reach " + model::format_engineering(target) + "s within " +
                        model::format_engineering(timeout_s) + "s of wall time (it is at " +
                        model::format_engineering(st.sim_time) + "s).");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    collect_messages(cx);
    return session.status().sim_time;
}

double wait_transient(Context& cx, double timeout_s)
{
    auto& session = cx.ws.session();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_s);
    while (session.active()) {
        if (std::chrono::steady_clock::now() > deadline) {
            session.stop();
            fail("timeout", "The transient analysis did not finish within " + model::format_engineering(timeout_s) + "s.");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return session.status().sim_time;
}

void check_failed(Context& cx)
{
    const auto st = cx.ws.session().status();
    if (st.state == sim::RunState::Failed) {
        fail("run-failed", st.message.empty() ? std::string("The simulation failed.") : st.message);
    }
}

// Values of the signals: the last value, and measurements within [from, to],
// plus `points` evenly spaced values when asked.
json read_signals(Context& cx, const std::vector<Signal>& signals, std::optional<double> from, std::optional<double> to,
    int points)
{
    auto& session = cx.ws.session();
    const auto mode = active_run(cx).mode;
    const double now = session.status().sim_time;
    const double t0 = from.value_or(0.0);
    const double t1 = to.value_or(now);
    json out = json::array();
    for (const auto& s : signals) {
        json entry{{"signal", s.name}, {"unit", s.current ? "A" : "V"}};
        if (!s.valid) {
            entry["error"] = s.problem;
            out.push_back(entry);
            continue;
        }
        const auto samples = series(session, mode, s);
        if (samples.empty()) {
            entry["value"] = s.vector == "0" && (s.reference.empty() || s.reference == "0") ? json(0.0) : json(nullptr);
            out.push_back(entry);
            continue;
        }
        entry["value"] = samples.back().value;
        entry["time"] = samples.back().time;
        if (const auto m = sim::measure(samples, t0, t1)) {
            entry["min"] = m->min;
            entry["max"] = m->max;
            entry["mean"] = m->mean;
            entry["rms"] = m->rms;
            entry["peak_to_peak"] = m->peak_to_peak;
            entry["frequency"] = number_or_null(m->frequency);
            entry["duty"] = number_or_null(m->duty);
        }
        if (samples.front().time > t0 + std::max(1e-6, 1e-3 * (t1 - t0))) {
            entry["history_from"] = samples.front().time; // older samples were dropped (see "history")
        }
        if (points > 0) {
            json values = json::array();
            for (int i = 0; i < points; ++i) {
                const double t = points == 1 ? t1 : t0 + (t1 - t0) * i / (points - 1);
                // Before the first sample (the run starts at its first time step): the first value.
                const auto v = t < samples.front().time ? std::optional<double>(samples.front().value)
                                                        : sim::value_at(samples, t);
                values.push_back(json::array({t, number_or_null(v)}));
            }
            entry["points"] = values;
        }
        out.push_back(entry);
    }
    return out;
}

void write_csv(Context& cx, const std::vector<Signal>& signals, const std::filesystem::path& path)
{
    auto& session = cx.ws.session();
    const auto mode = active_run(cx).mode;
    std::vector<std::vector<sim::Sample>> data;
    for (const auto& s : signals) {
        data.push_back(s.valid ? series(session, mode, s) : std::vector<sim::Sample>{});
    }
    std::size_t base = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i].size() > data[base].size()) {
            base = i;
        }
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        fail("io-error", "Cannot write " + path.string() + ".");
    }
    file << "time";
    for (const auto& s : signals) {
        file << ',' << s.name;
    }
    file << '\n';
    file.precision(9);
    if (!data.empty()) {
        for (std::size_t k = 0; k < data[base].size(); ++k) {
            const double t = data[base][k].time;
            file << t;
            for (std::size_t i = 0; i < data.size(); ++i) {
                file << ',';
                if (i == base) {
                    file << data[i][k].value;
                } else if (const auto v = sim::value_at(data[i], t)) {
                    file << *v;
                }
            }
            file << '\n';
        }
    }
    if (!file) {
        fail("io-error", "Cannot write " + path.string() + ".");
    }
}

double control_value(const json& v)
{
    if (v.is_boolean()) {
        return v.get<bool>() ? 1.0 : 0.0;
    }
    if (v.is_number()) {
        return v.get<double>();
    }
    if (v.is_string()) {
        const auto t = lower(v.get<std::string>());
        if (t == "on" || t == "closed" || t == "pressed" || t == "true") {
            return 1.0;
        }
        if (t == "off" || t == "open" || t == "released" || t == "false") {
            return 0.0;
        }
        if (const auto parsed = model::parse_engineering(t, "%"); parsed && !t.empty() && t.back() == '%') {
            return *parsed / 100.0;
        }
        if (const auto parsed = model::parse_engineering(t)) {
            return *parsed;
        }
    }
    fail("invalid-argument", "A control value is a number (0/1, 0..1 for a potentiometer, a key number for a keypad), "
                             "true/false, \"on\"/\"off\" or a percentage.");
}

const model::Component& control_component(Context& cx, const json& ref)
{
    const auto& c = cx.component(ref);
    const auto& controls = cx.ws.session().netlist().controls;
    if (std::none_of(controls.begin(), controls.end(), [&](const netlist::ControlSource& s) { return s.component == c.id; })) {
        fail("invalid-argument", c.designator + " is not an interactive control (switch, button, potentiometer, keypad, "
                                 "adjustable source) of the running circuit.");
    }
    return c;
}

// ---------------------------------------------------------------- commands

using Fn = json (*)(Context&, const json&);

struct Command {
    const char* name;
    const char* summary;
    const char* args; // JSON text: argument -> description
    Fn fn;
};

json cmd_hello(Context&, const json&)
{
    return {{"application", "SimNodus"}, {"protocol", protocol_version}};
}

json cmd_parts_list(Context& cx, const json& args)
{
    const auto search = lower(optional_text(args, "search").value_or(""));
    const auto category = optional_text(args, "category");
    json parts = json::array();
    for (const auto& d : cx.library().definitions()) {
        if (category && !iequals(*category, d.category)) {
            continue;
        }
        if (!search.empty()) {
            const auto haystack = lower(d.type + " " + d.name + " " + d.category + " " + d.description);
            if (haystack.find(search) == std::string::npos) {
                continue;
            }
        }
        parts.push_back({{"type", d.type}, {"name", d.name}, {"category", d.category}, {"description", d.description}});
    }
    return {{"categories", cx.library().categories()}, {"parts", parts}};
}

json cmd_parts_describe(Context& cx, const json& args)
{
    const auto type = text(args, "type");
    const auto* d = cx.library().find(type);
    if (!d) {
        fail("not-found", "No part of type \"" + type + "\"; parts.list lists them.");
    }
    json pins = json::array();
    for (const auto& p : d->pins) {
        pins.push_back({{"name", p.name}, {"at", point_json(p.at)}, {"direction", direction_name(p.direction)},
            {"optional", p.optional}});
    }
    json parameters = json::array();
    for (const auto& p : d->parameters) {
        json entry{{"name", p.name}, {"label", p.label}, {"kind", kind_name(p.kind)}, {"default", p.default_value}};
        if (!p.unit.empty()) {
            entry["unit"] = p.unit;
        }
        if (p.min) {
            entry["min"] = *p.min;
        }
        if (p.max) {
            entry["max"] = *p.max;
        }
        if (!p.choices.empty()) {
            entry["choices"] = p.choices;
        }
        if (!p.description.empty()) {
            entry["description"] = p.description;
        }
        if (p.kind == library::Parameter::Kind::Model) {
            json models = json::array();
            for (const auto& m : cx.library().models()) {
                if (iequals(m.type, p.model_type) && library::model_fits(p, m)) {
                    models.push_back(m.name);
                }
            }
            entry["models"] = models;
        }
        if (p.live) {
            entry["live"] = true;
        }
        parameters.push_back(entry);
    }
    const auto b = symbol_bounds(*d);
    json out{{"type", d->type}, {"name", d->name}, {"category", d->category}, {"description", d->description},
        {"prefix", d->prefix}, {"pins", pins}, {"parameters", parameters},
        {"bounds", json::array({std::lround(b[0]), std::lround(b[1]), std::lround(b[2]), std::lround(b[3])})}};
    if (d->interactive) {
        const char* kinds[] = {"toggle", "button", "potentiometer", "keypad"};
        out["control"] = kinds[static_cast<int>(d->interactive->kind)];
    }
    if (d->mcu) {
        out["microcontroller"] = {{"firmware_parameter", d->mcu->firmware_parameter}};
    }
    return out;
}

json cmd_project_new(Context& cx, const json&)
{
    cx.run.reset();
    cx.ws.replace(model::Schematic{}, {});
    return {{"file", nullptr}};
}

json cmd_project_open(Context& cx, const json& args)
{
    const std::filesystem::path path = path_from(text(args, "path"));
    auto result = io::load_project(path);
    if (!result.schematic) {
        std::string why;
        for (const auto& e : result.errors) {
            why += (why.empty() ? "" : " ") + e;
        }
        fail("io-error", "Cannot open " + utf8(path) + ": " + why);
    }
    cx.run.reset();
    cx.ws.replace(std::move(*result.schematic), std::filesystem::absolute(path));
    return {{"file", utf8(std::filesystem::absolute(path))}, {"warnings", result.warnings},
        {"components", cx.schematic().components().size()}};
}

json cmd_project_save(Context& cx, const json& args)
{
    std::filesystem::path path = cx.ws.file();
    if (const auto p = optional_text(args, "path")) {
        path = path_from(*p);
    }
    if (path.empty()) {
        fail("invalid-argument", "A new project needs \"path\" to be saved.");
    }
    const auto result = io::save_project(cx.schematic(), path);
    if (!result.ok) {
        fail("io-error", result.error);
    }
    cx.ws.saved(std::filesystem::absolute(path));
    return {{"file", utf8(std::filesystem::absolute(path))}};
}

json cmd_project_info(Context& cx, const json&)
{
    const auto& s = cx.schematic();
    return {{"file", cx.ws.file().empty() ? json(nullptr) : json(utf8(cx.ws.file()))}, {"modified", cx.ws.modified()},
        {"components", s.components().size()}, {"wires", s.wires().size()}, {"labels", s.labels().size()},
        {"probes", s.probes().size()}, {"notes", s.notes().size()}};
}

json settings_json(const model::SimulationSettings& s)
{
    return {{"mode", s.mode == model::SimulationMode::Transient ? "transient" : "interactive"}, {"speed", s.speed},
        {"max_step", s.max_step}, {"segment", s.segment}, {"transient_stop", s.transient_stop},
        {"transient_max_step", s.transient_max_step}, {"mcu_quantum", s.mcu_quantum}, {"exact_edges", s.exact_edges}};
}

json cmd_circuit_inspect(Context& cx, const json&)
{
    const auto& s = cx.schematic();
    const auto connectivity = netlist::build_connectivity(s, cx.library());
    json components = json::array();
    for (const auto& c : s.components()) {
        components.push_back(component_json(cx, c, &connectivity));
    }
    json wires = json::array();
    for (const auto& w : s.wires()) {
        json points = json::array();
        for (const auto& p : w.points) {
            points.push_back(point_json(p));
        }
        const auto it = connectivity.wire_net.find(w.id);
        wires.push_back({{"id", w.id}, {"points", points},
            {"net", it == connectivity.wire_net.end() ? json(nullptr)
                                                      : json(connectivity.nets[static_cast<std::size_t>(it->second)].name)}});
    }
    json labels = json::array();
    for (const auto& l : s.labels()) {
        labels.push_back({{"id", l.id}, {"name", l.name}, {"at", point_json(l.position)}});
    }
    json nets = json::array();
    for (const auto& n : connectivity.nets) {
        json pins = json::array();
        for (const auto& p : n.pins) {
            pins.push_back(pin_name(s, p));
        }
        nets.push_back({{"name", n.name}, {"ground", n.ground}, {"pins", pins}, {"labels", n.label_names}});
    }
    json probes = json::array();
    for (const auto& p : s.probes()) {
        json entry{{"id", p.id}, {"kind", p.kind == model::ProbeKind::Voltage ? "voltage" : "current"}};
        if (const auto* c = s.component(p.target.item)) {
            entry["at"] = c->designator + "." + p.target.pin;
        } else if (const auto it = connectivity.wire_net.find(p.target.item); it != connectivity.wire_net.end()) {
            entry["at"] = connectivity.nets[static_cast<std::size_t>(it->second)].name;
        } else {
            entry["at"] = p.target.item;
        }
        if (p.reference) {
            const auto* r = s.component(p.reference->item);
            entry["reference"] = r ? json(r->designator + "." + p.reference->pin) : json(p.reference->item);
        }
        probes.push_back(entry);
    }
    json notes = json::array();
    for (const auto& n : s.notes()) {
        notes.push_back({{"id", n.id}, {"at", point_json(n.position)}, {"text", n.text}});
    }
    json unconnected = json::array();
    for (const auto& p : connectivity.unconnected_pins) {
        unconnected.push_back(pin_name(s, p));
    }
    json dangling = json::array();
    for (const auto& p : connectivity.dangling_ends) {
        dangling.push_back(point_json(p));
    }
    return {{"components", components}, {"wires", wires}, {"labels", labels}, {"nets", nets}, {"probes", probes},
        {"notes", notes}, {"unconnected_pins", unconnected}, {"dangling_wire_ends", dangling},
        {"settings", settings_json(s.settings())}};
}

json cmd_circuit_check(Context& cx, const json&)
{
    const auto n = netlist::generate_netlist(cx.schematic(), cx.library());
    return {{"ok", n.ok()}, {"errors", n.count(netlist::Severity::Error)},
        {"warnings", n.count(netlist::Severity::Warning) - n.count(netlist::Severity::Error)},
        {"diagnostics", diagnostics_json(cx.schematic(), n.diagnostics)}};
}

json cmd_component_add(Context& cx, const json& args)
{
    const auto type = text(args, "type");
    const auto* d = cx.library().find(type);
    if (!d) {
        fail("not-found", "No part of type \"" + type + "\"; parts.list lists them.");
    }
    model::Orientation orientation;
    if (args.contains("rotation")) {
        const int r = required(args, "rotation").get<int>();
        if (r % 90 != 0) {
            fail("invalid-argument", "Argument \"rotation\" is 0, 90, 180 or 270 (clockwise).");
        }
        orientation.rotation = model::normalized_rotation(r);
    }
    orientation.mirrored = args.value("mirrored", false);
    Point at;
    if (args.contains("attach")) {
        // Places the part so that one of its pins lands on a pin or point.
        const auto& attach = args["attach"];
        if (!attach.is_object()) {
            fail("invalid-argument", "Argument \"attach\" is {\"pin\": \"1\", \"to\": \"R1.2\" or [x, y]}.");
        }
        // The pin may be left out for a part with one pin (ground, label-like parts).
        const auto pin_name = d->pins.size() == 1 && !attach.contains("pin") ? d->pins.front().name : text(attach, "pin");
        const auto* pin = d->pin(pin_name);
        if (!pin) {
            fail("not-found", type + " has no pin \"" + pin_name + "\".");
        }
        at = cx.location(required(attach, "to"), "to") - model::orient(pin->at, orientation);
    } else {
        at = point_from(required(args, "at"), "at");
    }
    Id id = model::no_id;
    model::Edit added = editing::add_component(cx.ws.id_source(), *d, at, orientation, &id);
    model::Schematic work = cx.schematic();
    added.apply(work);
    if (const auto designator = optional_text(args, "designator")) {
        if (const auto* other = work.component_by_designator(*designator); other && other->id != id) {
            fail("invalid-argument", "Designator " + *designator + " is taken.");
        }
        editing::set_designator(work, id, *designator).apply(work);
    }
    if (args.contains("parameters")) {
        set_parameters(cx, work, id, args["parameters"]);
    }
    const auto* c = work.component(id);
    cx.apply(model::make_edit(cx.schematic(), work, "Add " + c->designator));
    return component_json(cx, *cx.schematic().component(id), nullptr);
}

json cmd_component_set(Context& cx, const json& args)
{
    const auto& c = cx.component(required(args, "component"));
    const Id id = c.id;
    model::Schematic work = cx.schematic();
    if (const auto designator = optional_text(args, "designator")) {
        if (const auto* other = work.component_by_designator(*designator); other && other->id != id) {
            fail("invalid-argument", "Designator " + *designator + " is taken.");
        }
        editing::set_designator(work, id, *designator).apply(work);
    }
    if (args.contains("parameters")) {
        set_parameters(cx, work, id, args["parameters"]);
    }
    if (args.contains("mirrored") && args["mirrored"].get<bool>() != work.component(id)->orientation.mirrored) {
        editing::mirror_items(work, cx.library(), {id}).apply(work);
    }
    if (args.contains("rotation")) {
        const int wanted = model::normalized_rotation(args["rotation"].get<int>());
        for (int i = 0; i < 4 && work.component(id)->orientation.rotation != wanted; ++i) {
            editing::rotate_items(work, cx.library(), {id}).apply(work);
        }
    }
    cx.apply(model::make_edit(cx.schematic(), work, "Change " + work.component(id)->designator));
    return component_json(cx, *cx.schematic().component(id), nullptr);
}

json cmd_items_move(Context& cx, const json& args)
{
    std::set<Id> ids;
    Point delta;
    if (args.contains("to")) {
        const auto& c = cx.component(required(args, "component"));
        ids = {c.id};
        delta = point_from(args["to"], "to") - c.position;
    } else {
        ids = cx.items(required(args, "items"));
        delta = point_from(required(args, "by"), "by");
    }
    cx.apply(editing::move_items(cx.schematic(), cx.library(), ids, delta));
    return {{"moved", ids.size()}};
}

json cmd_items_delete(Context& cx, const json& args)
{
    const auto ids = cx.items(required(args, "items"));
    cx.apply(editing::delete_items(cx.schematic(), ids));
    return {{"deleted", ids.size()}};
}

json wire_result(Id id, const std::vector<Point>& points)
{
    json out = json::array();
    for (const auto& p : points) {
        out.push_back(point_json(p));
    }
    return {{"id", id}, {"points", out}};
}

json cmd_wire_add(Context& cx, const json& args)
{
    const auto& list = required(args, "points");
    if (!list.is_array() || list.size() < 2) {
        fail("invalid-argument", "Argument \"points\" is a list of at least two grid points.");
    }
    std::vector<Point> points;
    for (const auto& p : list) {
        points.push_back(cx.location(p, "points"));
    }
    if (!editing::is_orthogonal(editing::normalize_polyline(points))) {
        fail("invalid-argument", "Wires are orthogonal: consecutive points must share x or y.");
    }
    Id id = model::no_id;
    auto edit = editing::add_wire(cx.ws.id_source(), points, &id);
    if (edit.empty()) {
        fail("invalid-argument", "The wire has no length.");
    }
    cx.apply(std::move(edit));
    return wire_result(id, cx.schematic().wire(id)->points);
}

// Pins and labels of the nets at the given points (what a wire between them may join).
std::pair<std::set<netlist::PinRef>, std::set<Id>> reachable(const netlist::Connectivity& connectivity,
    const model::Schematic& schematic, const library::ComponentLibrary& library, const std::vector<Point>& ends)
{
    std::set<netlist::PinRef> pins;
    std::set<Id> labels;
    for (const auto& p : ends) {
        if (const auto net = connectivity.net_at(schematic, library, p)) {
            const auto& n = connectivity.nets[static_cast<std::size_t>(*net)];
            pins.insert(n.pins.begin(), n.pins.end());
            labels.insert(n.labels.begin(), n.labels.end());
        }
    }
    return {pins, labels};
}

json cmd_wire_connect(Context& cx, const json& args)
{
    const auto& from_ref = required(args, "from");
    const auto& to_ref = required(args, "to");
    const Point from = cx.location(from_ref, "from");
    const Point to = cx.location(to_ref, "to");
    std::vector<Point> stops{from};
    if (args.contains("via")) {
        for (const auto& p : args["via"]) {
            stops.push_back(point_from(p, "via"));
        }
    }
    stops.push_back(to);
    // The first leg leaves a pin the way the pin points.
    bool horizontal_first = true;
    if (from_ref.is_string()) {
        const auto [c, pin] = cx.pin(from_ref.get<std::string>());
        horizontal_first = model::orient(direction_vector(pin->direction), c->orientation).y == 0;
    }
    const auto before = netlist::build_connectivity(cx.schematic(), cx.library());
    const auto allowed = reachable(before, cx.schematic(), cx.library(), {from, to});
    // Both corner choices of every leg, the preferred one first.
    for (const bool first : {horizontal_first, !horizontal_first}) {
        std::vector<Point> points{stops.front()};
        for (std::size_t i = 1; i < stops.size(); ++i) {
            const Point a = points.back();
            const Point b = stops[i];
            if (a.x != b.x && a.y != b.y) {
                points.push_back((i == 1 ? first : true) ? Point{b.x, a.y} : Point{a.x, b.y});
            }
            points.push_back(b);
        }
        points = editing::normalize_polyline(points);
        if (points.size() < 2) {
            fail("invalid-argument", "The two ends are the same point.");
        }
        model::Schematic work = cx.schematic();
        Id id = model::no_id;
        auto edit = editing::add_wire(work, points, &id);
        edit.apply(work);
        // The wire may join only what is at its ends: never another pin it passes.
        const auto after = netlist::build_connectivity(work, cx.library());
        const auto net = after.wire_net.find(id);
        if (net == after.wire_net.end()) {
            continue;
        }
        const auto& joined = after.nets[static_cast<std::size_t>(net->second)];
        const bool clean = std::all_of(joined.pins.begin(), joined.pins.end(), [&](const netlist::PinRef& p) {
            return allowed.first.count(p) > 0;
        }) && std::all_of(joined.labels.begin(), joined.labels.end(), [&](Id l) { return allowed.second.count(l) > 0; });
        if (!clean) {
            continue;
        }
        Id created = model::no_id;
        auto real = editing::add_wire(cx.ws.id_source(), points, &created);
        cx.apply(std::move(real));
        auto result = wire_result(created, cx.schematic().wire(created)->points);
        result["net"] = joined.name;
        return result;
    }
    fail("blocked", "Every orthogonal route would touch another pin or wire; give corner points in \"via\" or "
                    "connect with net labels (label.add).");
}

json cmd_label_add(Context& cx, const json& args)
{
    const auto name = text(args, "name");
    const Point at = cx.location(required(args, "at"), "at");
    Id id = model::no_id;
    auto edit = editing::add_label(cx.ws.id_source(), at, name, &id);
    cx.apply(std::move(edit));
    return {{"id", id}, {"name", name}, {"at", point_json(at)}};
}

model::Anchor anchor(Context& cx, const std::string& ref)
{
    if (ref.find('.') != std::string::npos) {
        const auto [c, pin] = cx.pin(ref);
        return {c->id, pin->name, library::pin_position(cx.definition(*c), *pin, *c)};
    }
    const auto connectivity = netlist::build_connectivity(cx.schematic(), cx.library());
    for (const auto& n : connectivity.nets) {
        if (!iequals(n.name, ref) && std::none_of(n.label_names.begin(), n.label_names.end(),
                                         [&](const std::string& l) { return iequals(l, ref); })) {
            continue;
        }
        if (!n.pins.empty()) {
            const auto* c = cx.schematic().component(n.pins.front().first);
            const auto* p = cx.definition(*c).pin(n.pins.front().second);
            return {c->id, p->name, library::pin_position(cx.definition(*c), *p, *c)};
        }
        if (!n.wires.empty()) {
            return {n.wires.front(), {}, cx.schematic().wire(n.wires.front())->points.front()};
        }
    }
    fail("not-found", "No pin or net \"" + ref + "\".");
}

json cmd_probe_add(Context& cx, const json& args)
{
    model::Probe probe;
    const auto kind = optional_text(args, "kind").value_or("voltage");
    probe.kind = iequals(kind, "current") ? model::ProbeKind::Current : model::ProbeKind::Voltage;
    const auto at = text(args, "at");
    if (probe.kind == model::ProbeKind::Current) {
        if (at.find('.') == std::string::npos) {
            fail("invalid-argument", "A current probe goes on a pin, like \"R1.1\" (current into the pin).");
        }
    }
    probe.target = anchor(cx, at);
    if (const auto reference = optional_text(args, "reference")) {
        probe.reference = anchor(cx, *reference);
    }
    probe.color = optional_text(args, "color").value_or("");
    Id id = model::no_id;
    cx.apply(editing::add_probe(cx.ws.id_source(), probe, &id));
    return {{"id", id}};
}

json cmd_note_add(Context& cx, const json& args)
{
    Id id = model::no_id;
    auto edit = editing::add_note(cx.ws.id_source(), point_from(required(args, "at"), "at"), text(args, "text"), &id);
    if (edit.empty()) {
        fail("invalid-argument", "A note needs text.");
    }
    cx.apply(std::move(edit));
    return {{"id", id}};
}

json cmd_settings_get(Context& cx, const json&)
{
    return settings_json(cx.schematic().settings());
}

json cmd_settings_set(Context& cx, const json& args)
{
    auto s = cx.schematic().settings();
    if (const auto mode = optional_text(args, "mode")) {
        if (iequals(*mode, "transient")) {
            s.mode = model::SimulationMode::Transient;
        } else if (iequals(*mode, "interactive")) {
            s.mode = model::SimulationMode::Interactive;
        } else {
            fail("invalid-argument", "Argument \"mode\" is \"interactive\" or \"transient\".");
        }
    }
    auto positive = [&](const char* key, double& target) {
        if (const auto v = optional_quantity(args, key, "s")) {
            if (*v <= 0) {
                fail("invalid-argument", std::string("Argument \"") + key + "\" must be positive.");
            }
            target = *v;
        }
    };
    positive("max_step", s.max_step);
    positive("segment", s.segment);
    positive("transient_stop", s.transient_stop);
    positive("transient_max_step", s.transient_max_step);
    positive("mcu_quantum", s.mcu_quantum);
    if (const auto v = optional_quantity(args, "speed", "")) {
        s.speed = std::max(0.0, *v);
    }
    if (args.contains("exact_edges")) {
        s.exact_edges = args["exact_edges"].get<bool>();
    }
    cx.apply(editing::set_settings(cx.schematic(), s));
    return settings_json(cx.schematic().settings());
}

json cmd_undo(Context& cx, const json&)
{
    return {{"undone", cx.ws.undo()}};
}

json cmd_redo(Context& cx, const json&)
{
    return {{"redone", cx.ws.redo()}};
}

json run_result(Context& cx, const json& args)
{
    const auto from = optional_quantity(args, "from", "s");
    const auto to = optional_quantity(args, "to", "s");
    const int points = args.value("points", 0);
    if (points < 0 || points > 10000) {
        fail("invalid-argument", "Argument \"points\" is 0..10000.");
    }
    auto& run = active_run(cx);
    std::vector<Signal> signals = run.signals;
    if (args.contains("signals") && !args["signals"].is_null()) {
        signals.clear();
        for (const auto& s : args["signals"]) {
            auto signal = parse_signal(s.get<std::string>());
            resolve(cx, cx.ws.session().netlist(), signal);
            signals.push_back(signal);
        }
    }
    json out{{"status", status_json(cx.ws.session().status())},
        {"signals", read_signals(cx, signals, from, to, points)}};
    collect_messages(cx);
    if (!run.messages.empty()) {
        out["messages"] = run.messages;
    }
    const auto texts = cx.ws.session().device_texts();
    if (!texts.empty()) {
        json displays = json::object();
        for (const auto& [id, t] : texts) {
            const auto* c = cx.schematic().component(id);
            displays[c ? c->designator : std::to_string(id)] = t;
        }
        out["displays"] = displays;
    }
    if (const auto csv = optional_text(args, "csv")) {
        write_csv(cx, signals, path_from(*csv));
        out["csv"] = *csv;
    }
    return out;
}

json cmd_sim_start(Context& cx, const json& args)
{
    start_run(cx, args);
    json signals = json::array();
    for (const auto& s : cx.run->signals) {
        signals.push_back(s.valid ? json(s.name) : json({{"signal", s.name}, {"error", s.problem}}));
    }
    json controls = json::array();
    for (const auto& c : cx.ws.session().netlist().controls) {
        if (const auto* component = cx.schematic().component(c.component)) {
            controls.push_back(component->designator);
        }
    }
    return {{"status", status_json(cx.ws.session().status())}, {"signals", signals}, {"controls", controls}};
}

json cmd_sim_advance(Context& cx, const json& args)
{
    auto& run = active_run(cx);
    const double timeout = optional_quantity(args, "timeout", "s").value_or(300.0);
    const double now = cx.ws.session().status().sim_time;
    double target = 0.0;
    if (const auto to = optional_quantity(args, "to", "s")) {
        target = *to;
    } else {
        target = now + quantity(required(args, "by"), "by", "s");
    }
    if (run.mode == model::SimulationMode::Transient) {
        wait_transient(cx, timeout);
    } else {
        advance_to(cx, target, timeout);
    }
    check_failed(cx);
    return {{"status", status_json(cx.ws.session().status())}};
}

json cmd_sim_control(Context& cx, const json& args)
{
    active_run(cx);
    const auto& c = control_component(cx, required(args, "component"));
    const double value = control_value(required(args, "value"));
    cx.ws.session().set_control(c.id, value);
    return {{"component", c.designator}, {"value", value}, {"time", cx.ws.session().status().sim_time}};
}

json cmd_sim_read(Context& cx, const json& args)
{
    active_run(cx);
    return run_result(cx, args);
}

json cmd_sim_status(Context& cx, const json&)
{
    if (!cx.run) {
        return {{"status", {{"state", "idle"}}}};
    }
    collect_messages(cx);
    json out{{"status", status_json(cx.ws.session().status())}};
    if (!cx.run->messages.empty()) {
        out["messages"] = cx.run->messages;
    }
    return out;
}

json cmd_sim_pause(Context& cx, const json&)
{
    active_run(cx);
    cx.ws.session().pause();
    return {{"status", status_json(cx.ws.session().status())}};
}

json cmd_sim_stop(Context& cx, const json&)
{
    cx.ws.session().stop();
    json out{{"status", status_json(cx.ws.session().status())}};
    return out;
}

json cmd_sim_run(Context& cx, const json& args)
{
    start_run(cx, args);
    auto& run = *cx.run;
    const double timeout = optional_quantity(args, "timeout", "s").value_or(300.0);
    json applied = json::array();
    if (run.mode == model::SimulationMode::Transient) {
        if (args.contains("events") && !args["events"].empty()) {
            cx.ws.session().stop();
            fail("invalid-argument", "Transient runs keep the controls as they are; use mode \"interactive\" for events.");
        }
        wait_transient(cx, timeout);
    } else {
        const double duration = quantity(required(args, "duration"), "duration", "s");
        struct Event {
            double at;
            Id component;
            std::string designator;
            double value;
        };
        std::vector<Event> events;
        if (args.contains("events")) {
            for (const auto& e : args["events"]) {
                const auto& c = control_component(cx, required(e, "component"));
                events.push_back({quantity(required(e, "at"), "at", "s"), c.id, c.designator, control_value(required(e, "value"))});
            }
        }
        std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
        for (const auto& e : events) {
            if (e.at > duration) {
                break;
            }
            advance_to(cx, e.at, timeout);
            check_failed(cx);
            cx.ws.session().set_control(e.component, e.value);
            applied.push_back({{"component", e.designator}, {"value", e.value}, {"at", e.at},
                {"applied_at", cx.ws.session().status().sim_time}});
        }
        advance_to(cx, duration, timeout);
    }
    check_failed(cx);
    json args_for_read = args;
    if (run.mode == model::SimulationMode::Interactive && !args.contains("to")) {
        args_for_read["to"] = quantity(args["duration"], "duration", "s");
    }
    args_for_read.erase("signals"); // already the run's signals
    auto out = run_result(cx, args_for_read);
    if (!applied.empty()) {
        out["events"] = applied;
    }
    if (!args.value("keep", false)) {
        cx.ws.session().stop();
        out["status"]["state"] = sim::to_string(cx.ws.session().status().state);
    }
    return out;
}

json cmd_sim_uart(Context& cx, const json& args)
{
    active_run(cx);
    const auto& board = cx.component(required(args, "board"));
    const auto uart = lower(optional_text(args, "uart").value_or("usart1"));
    if (const auto send = optional_text(args, "send")) {
        cx.ws.session().mcu_uart_send(board.id, uart, *send);
    }
    std::string bytes;
    cx.ws.session().mcu_uart_received(board.id, uart, 0, bytes);
    return {{"board", board.designator}, {"uart", uart}, {"received", bytes}};
}

json cmd_help(Context&, const json&);

const Command commands[] = {
    {"hello", "Protocol version.", "{}", cmd_hello},
    {"help", "This list.", "{}", cmd_help},
    {"parts.list", "Parts of the library.",
        R"j({"search": "text in type, name, category or description", "category": "exact category"})j", cmd_parts_list},
    {"parts.describe", "Pins (grid points relative to the origin), parameters, bounds of a part.", R"j({"type": "resistor"})j",
        cmd_parts_describe},
    {"project.new", "Starts an empty project.", "{}", cmd_project_new},
    {"project.open", "Opens a .snx file.", R"j({"path": "file.snx"})j", cmd_project_open},
    {"project.save", "Saves the project (keeps the previous version as .snx.bak).",
        R"j({"path": "file.snx; optional once the project has a file"})j", cmd_project_save},
    {"project.info", "File, modified flag, element counts.", "{}", cmd_project_info},
    {"circuit.inspect", "Components with pins, positions and nets; wires, labels, nets, probes, notes, settings.", "{}",
        cmd_circuit_inspect},
    {"circuit.check", "Pre-run checks: diagnostics with stable codes.", "{}", cmd_circuit_check},
    {"component.add", "Places a part, at a point or with one of its pins on a pin or point.",
        R"j({"type": "resistor", "at": "[x, y]", "attach": {"pin": "1 (optional for a part with one pin)", "to": "R1.2 or [x, y]"}, "rotation": "0, 90, 180 or 270 (clockwise)", "mirrored": "false", "designator": "R1", "parameters": {"R": "4.7k"}})j",
        cmd_component_add},
    {"component.set", "Changes parameters, designator, rotation or mirroring.",
        R"j({"component": "R1", "parameters": {"R": "10k"}, "designator": "R2", "rotation": "90", "mirrored": "true"})j",
        cmd_component_set},
    {"items.move", "Moves a component to a point, or items by an offset; attached wires follow.",
        R"j({"component": "R1", "to": "[x, y]", "items": "[\"R1\", 12] instead of component", "by": "[dx, dy] instead of to"})j",
        cmd_items_move},
    {"items.delete", "Deletes components (by designator) or other elements (by ID).", R"j({"items": "[\"R1\", 12]"})j",
        cmd_items_delete},
    {"wire.add", "Draws an orthogonal wire through points (pins allowed as points).",
        R"j({"points": "[[x, y], \"R1.2\", [x, y]]"})j", cmd_wire_add},
    {"wire.connect", "Wires two pins or points with an orthogonal route that touches nothing else.",
        R"j({"from": "R1.2 or [x, y]", "to": "C1.1 or [x, y]", "via": "[[x, y]] corner points, optional"})j", cmd_wire_connect},
    {"label.add", "Net label: equal names connect; GND or 0 is ground.", R"j({"name": "OUT", "at": "R1.2 or [x, y]"})j",
        cmd_label_add},
    {"probe.add", "Probe for the Signal Analyzer.",
        R"j({"kind": "voltage or current", "at": "R1.2 or a net name", "reference": "pin or net, optional", "color": "#rrggbb, optional"})j",
        cmd_probe_add},
    {"note.add", "Text note on the schematic.", R"j({"at": "[x, y]", "text": "..."})j", cmd_note_add},
    {"settings.get", "Simulation settings.", "{}", cmd_settings_get},
    {"settings.set", "Changes simulation settings.",
        R"j({"mode": "interactive or transient", "transient_stop": "20m", "transient_max_step": "1u", "max_step": "100u", "segment": "1", "speed": "1 (0 = maximum)", "mcu_quantum": "1m", "exact_edges": "false"})j",
        cmd_settings_set},
    {"edit.undo", "Undoes the last change.", "{}", cmd_undo},
    {"edit.redo", "Redoes the last undone change.", "{}", cmd_redo},
    {"sim.run", "Runs and measures: a transient analysis, or an interactive run for a duration with timed control changes.",
        R"j({"mode": "interactive or transient (default: the project's)", "duration": "20m", "signals": ["V(OUT)", "V(R1.2,R1.1)", "I(R1.1)"], "events": [{"at": "5m", "component": "SW1", "value": "1"}], "points": "50", "csv": "out.csv", "keep": "false", "timeout": "300"})j",
        cmd_sim_run},
    {"sim.start", "Starts a run and keeps it for sim.advance, sim.control and sim.read.",
        R"j({"mode": "interactive or transient", "duration": "transient stop time", "signals": ["V(OUT)"], "speed": "0 (maximum)", "history": "100000 samples per net"})j",
        cmd_sim_start},
    {"sim.advance", "Lets an interactive run reach a time, then pauses it; waits for a transient analysis.",
        R"j({"to": "10m", "by": "1m instead of to", "timeout": "300"})j", cmd_sim_advance},
    {"sim.control", "Operates a switch, button, potentiometer, keypad or adjustable source.",
        R"j({"component": "SW1", "value": "1, 0, 0.5, true, on, 50%"})j", cmd_sim_control},
    {"sim.read", "Values and measurements of the signals.",
        R"j({"signals": ["V(OUT)"], "from": "0", "to": "10m", "points": "20", "csv": "file.csv"})j", cmd_sim_read},
    {"sim.status", "State, simulated time, speed, messages.", "{}", cmd_sim_status},
    {"sim.pause", "Pauses the run.", "{}", cmd_sim_pause},
    {"sim.stop", "Ends the run.", "{}", cmd_sim_stop},
    {"sim.uart", "Text a board sent on a serial port; optionally sends text to it.",
        R"j({"board": "U1", "uart": "usart1", "send": "text, optional"})j", cmd_sim_uart},
};

json cmd_help(Context&, const json&)
{
    return Automation::help();
}

} // namespace

// ---------------------------------------------------------------- automation

Automation::Automation(Workspace& workspace) : workspace_(workspace) {}

Automation::~Automation() = default;

json Automation::help()
{
    json list = json::array();
    for (const auto& c : commands) {
        const json args = json::parse(c.args);
        list.push_back({{"command", c.name}, {"summary", c.summary}, {"args", args}});
    }
    return {{"protocol", protocol_version}, {"commands", list},
        {"notes", json::array({"Grid points are integers; y grows downwards; one unit is the canvas grid step.",
                      "Components are named by designator (\"R1\") or ID; pins as DESIGNATOR.PIN (\"R1.2\").",
                      "Quantities accept numbers or engineering text: \"4.7k\", \"10m\", \"10ms\", \"100u\".",
                      "Interactive runs are approximate (segments, 100 us steps by default); transient runs are one "
                      "deterministic analysis without microcontrollers."})}};
}

json Automation::execute(const json& request)
{
    json reply = json::object();
    if (request.is_object() && request.contains("id")) {
        reply["id"] = request["id"];
    }
    try {
        if (!request.is_object()) {
            fail("bad-request", "A request is a JSON object {\"command\": ..., \"args\": {...}}.");
        }
        const auto it = request.find("command");
        if (it == request.end() || !it->is_string()) {
            fail("bad-request", "The request has no \"command\".");
        }
        const auto name = it->get<std::string>();
        json args = request.value("args", json::object());
        if (args.is_null()) {
            args = json::object();
        }
        if (!args.is_object()) {
            fail("bad-request", "\"args\" must be an object.");
        }
        const auto* command = std::find_if(std::begin(commands), std::end(commands),
            [&](const Command& c) { return name == c.name; });
        if (command == std::end(commands)) {
            fail("unknown-command", "Unknown command \"" + name + "\"; \"help\" lists them.");
        }
        Context cx{workspace_, run_};
        reply["ok"] = true;
        reply["result"] = command->fn(cx, args);
    } catch (const Failure& f) {
        reply["ok"] = false;
        reply["error"] = {{"code", f.code}, {"message", f.message}};
    } catch (const json::exception& e) {
        reply["ok"] = false;
        reply["error"] = {{"code", "invalid-argument"}, {"message", std::string("Wrong argument type: ") + e.what()}};
    } catch (const std::exception& e) {
        reply["ok"] = false;
        reply["error"] = {{"code", "internal"}, {"message", e.what()}};
    }
    return reply;
}

json Automation::execute_line(const std::string& line)
{
    json request;
    try {
        request = json::parse(line);
    } catch (const json::exception& e) {
        return {{"ok", false}, {"error", {{"code", "bad-request"}, {"message", std::string("Not JSON: ") + e.what()}}}};
    }
    return execute(request);
}

} // namespace simnodus::automation
