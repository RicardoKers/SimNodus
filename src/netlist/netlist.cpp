// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "netlist.hpp"

#include "library/spice_template.hpp"
#include "model/engineering.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace simnodus::netlist {
namespace {

using library::ComponentDefinition;
using model::Component;

class UnionFind {
public:
    int id(const std::string& node)
    {
        auto it = index_.find(node);
        if (it == index_.end()) {
            it = index_.emplace(node, static_cast<int>(parent_.size())).first;
            parent_.push_back(it->second);
        }
        return it->second;
    }
    int find(int x)
    {
        while (parent_[static_cast<std::size_t>(x)] != x) {
            x = parent_[static_cast<std::size_t>(x)] = parent_[static_cast<std::size_t>(parent_[static_cast<std::size_t>(x)])];
        }
        return x;
    }
    // Returns false if a and b were already connected.
    bool unite(const std::string& a, const std::string& b)
    {
        const int ra = find(id(a));
        const int rb = find(id(b));
        if (ra == rb) {
            return false;
        }
        parent_[static_cast<std::size_t>(std::max(ra, rb))] = std::min(ra, rb);
        return true;
    }
    bool connected(const std::string& a, const std::string& b) { return find(id(a)) == find(id(b)); }

private:
    std::map<std::string, int> index_;
    std::vector<int> parent_;
};

std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string w;
    while (in >> w) {
        words.push_back(w);
    }
    return words;
}

// Node count of an element line by its letter (the remaining words are values).
std::size_t node_count(char letter)
{
    switch (letter) {
    case 'R':
    case 'C':
    case 'L':
    case 'V':
    case 'I':
    case 'D':
    case 'B':
        return 2;
    case 'Q':
        return 3;
    case 'S':
    case 'M':
        return 4;
    default:
        return 0;
    }
}

// Node index pairs that conduct at DC (for the "no DC path" check).
std::vector<std::pair<std::size_t, std::size_t>> dc_paths(char letter)
{
    switch (letter) {
    case 'R':
    case 'L':
    case 'V':
    case 'D':
    case 'B':
        return {{0, 1}};
    case 'S':
        return {{0, 1}};
    case 'Q':
        return {{0, 1}, {1, 2}};
    case 'M':
        return {{0, 2}, {2, 3}};
    default:
        return {}; // C, I: no DC path
    }
}

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

bool Netlist::ok() const
{
    return count(Severity::Error) == 0;
}

std::size_t Netlist::count(Severity severity) const
{
    return static_cast<std::size_t>(std::count_if(diagnostics.begin(), diagnostics.end(),
        [severity](const Diagnostic& d) { return d.severity >= severity; }));
}

Netlist generate_netlist(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const NetlistOptions& options)
{
    Netlist out;
    out.connectivity = build_connectivity(schematic, library);
    auto report = [&out](Severity severity, std::string code, std::string message, std::vector<Id> items,
                      std::optional<Point> at = std::nullopt) {
        out.diagnostics.push_back({severity, std::move(code), std::move(message), std::move(items), at});
    };

    for (const auto& [pin, net] : out.connectivity.pin_net) {
        out.pin_node[pin] = out.connectivity.nets[static_cast<std::size_t>(net)].name;
    }

    // Designators must stay unique after sanitizing (they name SPICE elements).
    std::map<std::string, Id> refs;
    for (const auto& c : schematic.components()) {
        const auto* d = library.find(c.type);
        if (d && !d->ties_net.empty()) {
            continue; // ground symbols produce no element
        }
        const std::string key = lower(sanitize_name(c.designator));
        if (auto [it, inserted] = refs.emplace(key, c.id); !inserted) {
            report(Severity::Error, "duplicate-designator",
                "Two components are named " + c.designator + ". Rename one of them.", {it->second, c.id});
        }
    }

    bool has_elements = false;
    std::set<std::string> model_lines;
    std::vector<std::string> probe_lines;
    for (const auto& c : schematic.components()) {
        const ComponentDefinition* d = library.find(c.type);
        if (!d) {
            report(Severity::Error, "unknown-type",
                c.designator + " uses the component type \"" + c.type + "\", which is not in the library.", {c.id},
                c.position);
            continue;
        }
        if (d->spice.empty()) {
            continue;
        }
        has_elements = true;
        const std::string ref = sanitize_name(c.designator);

        // Parameter values: numbers for the template, or chosen words.
        std::map<std::string, double> numbers;
        std::map<std::string, std::string> words;
        std::vector<std::string> subcircuit_pins; // a vendor subcircuit replaces the template (ADR 0097)
        std::string subcircuit_name;
        bool values_ok = true;
        for (const auto& p : d->parameters) {
            const auto found = c.parameters.find(p.name);
            const std::string text = found == c.parameters.end() ? p.default_value : found->second;
            if (p.kind == library::Parameter::Kind::File) {
                words[p.name] = text; // used by the run, not by the template
                continue;
            }
            if (p.kind == library::Parameter::Kind::Model) {
                // ADR 0097: a vendor subcircuit kept in the component replaces the
                // part's template with an instance of it, wrapped in a subcircuit
                // of SimNodus's own name so that its models and blocks stay local.
                if (const auto kept = c.models.find(p.name); kept != c.models.end() && !p.subcircuit.empty()) {
                    std::string why;
                    if (const auto sub = library::parse_subcircuit_text(kept->second, &why); sub && sub->name == text) {
                        if (sub->ports.size() != p.subcircuit.size()) {
                            report(Severity::Error, "unknown-model",
                                c.designator + ": the subcircuit " + text + " has " + std::to_string(sub->ports.size())
                                    + " pins; this part needs " + std::to_string(p.subcircuit.size()) + ".",
                                {c.id}, c.position);
                            values_ok = false;
                            continue;
                        }
                        // Part pin of each subcircuit port: the kept assignment, else the usual order.
                        std::vector<std::string> pins = p.subcircuit;
                        if (const auto map = c.models.find(p.name + ".pins"); map != c.models.end()) {
                            std::vector<std::string> chosen_pins;
                            std::stringstream list(map->second);
                            for (std::string pin; std::getline(list, pin, ',');) {
                                chosen_pins.push_back(pin);
                            }
                            auto sorted_a = chosen_pins;
                            auto sorted_b = p.subcircuit;
                            std::sort(sorted_a.begin(), sorted_a.end());
                            std::sort(sorted_b.begin(), sorted_b.end());
                            if (sorted_a == sorted_b) {
                                pins = chosen_pins;
                            } else {
                                report(Severity::Warning, "model-pins",
                                    c.designator + ": the pin assignment of " + text
                                        + " is not valid; the usual order is used.",
                                    {c.id}, c.position);
                            }
                        }
                        const std::string wrapper = "UX_" + sanitize_name(c.designator) + "_" + sanitize_name(p.name);
                        std::string header = ".subckt " + wrapper;
                        std::string call = "X1";
                        for (std::size_t k = 0; k < pins.size(); ++k) {
                            header += " p" + std::to_string(k + 1);
                            call += " p" + std::to_string(k + 1);
                        }
                        out.models.push_back(header);
                        // The vendor's own name may hold characters ngspice treats specially
                        // ("LM358/NS"): the last block, the one the part uses, becomes SNX.
                        std::vector<std::string> lines;
                        std::stringstream body(kept->second);
                        for (std::string line; std::getline(body, line);) {
                            if (!line.empty() && line.back() == '\r') {
                                line.pop_back();
                            }
                            lines.push_back(line);
                        }
                        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                            const auto words_of_line = split(*it);
                            if (!words_of_line.empty() && lower(words_of_line[0]) == ".subckt" && words_of_line.size() > 1) {
                                const auto at = it->find(words_of_line[1]);
                                it->replace(at, words_of_line[1].size(), "SNX");
                                break;
                            }
                        }
                        out.models.insert(out.models.end(), lines.begin(), lines.end());
                        out.models.push_back(call + " SNX");
                        out.models.push_back(".ends " + wrapper);
                        subcircuit_pins = pins;
                        subcircuit_name = wrapper;
                        continue;
                    } else if (!sub && why.find("no .subckt") == std::string::npos) {
                        report(Severity::Error, "unknown-model",
                            c.designator + ": the subcircuit " + text + " cannot be used: " + why + ".", {c.id}, c.position);
                        values_ok = false;
                        continue;
                    }
                }
                // PRJ-07: a model kept in the component (not built in), else a
                // built-in one. Netlist names are SimNodus's own, so any model
                // name works and two parts never clash.
                std::optional<library::SpiceModel> chosen;
                if (const auto kept = c.models.find(p.name); kept != c.models.end()) {
                    chosen = library::parse_spice_model(kept->second);
                    if (chosen && chosen->name != text) {
                        chosen.reset();
                    }
                    // Parameters ngspice could not use stay out of the netlist; say so.
                    for (const auto& left_out : chosen ? chosen->dropped : std::vector<std::string>{}) {
                        report(Severity::Warning, "model-parameter",
                            c.designator + ": model " + text + " left out " + left_out + ".", {c.id}, c.position);
                    }
                }
                std::string as_name;
                if (chosen) {
                    as_name = "UM_" + sanitize_name(c.designator) + "_" + sanitize_name(p.name);
                } else if (const auto* built_in = library.model(p.model_type, text)) {
                    chosen = *built_in;
                    as_name = "BM_" + sanitize_name(p.model_type) + "_" + sanitize_name(text);
                }
                if (!chosen && p.model_type.empty() && text == p.default_value) {
                    continue; // the part's own template (an op-amp's behavioral model)
                }
                if (!chosen || chosen->type != p.model_type) {
                    report(Severity::Error, "unknown-model",
                        c.designator + ": the " + p.model_type + " model \"" + text
                            + "\" is not available. Choose another model in Properties.",
                        {c.id}, c.position);
                    values_ok = false;
                    continue;
                }
                const std::string card = chosen->card(as_name);
                if (model_lines.insert(card).second) {
                    out.models.push_back(card);
                }
                words[p.name] = as_name;
                continue;
            }
            if (!p.choices.empty()) {
                if (std::find(p.choices.begin(), p.choices.end(), text) == p.choices.end()) {
                    report(Severity::Error, "invalid-value",
                        c.designator + ": \"" + text + "\" is not a valid " + p.label + ".", {c.id}, c.position);
                    values_ok = false;
                }
                words[p.name] = text;
                continue;
            }
            const auto value = model::parse_engineering(text, p.unit);
            if (!value) {
                report(Severity::Error, "invalid-value",
                    c.designator + ": " + p.label + " \"" + text + "\" is not a number.", {c.id}, c.position);
                values_ok = false;
                continue;
            }
            if ((p.min && *value < *p.min) || (p.max && *value > *p.max)) {
                std::string range = p.min ? ">= " + model::format_engineering(*p.min) : "";
                if (p.max) {
                    range += std::string(range.empty() ? "" : " and ") + "<= " + model::format_engineering(*p.max);
                }
                report(Severity::Error, "value-out-of-range",
                    c.designator + ": " + p.label + " must be " + range + " " + p.unit + ".", {c.id}, c.position);
                values_ok = false;
                continue;
            }
            numbers[p.name] = *value;
        }
        if (!values_ok) {
            continue;
        }

        // Current probes on this component's pins.
        std::map<PinRef, std::string> probe_nodes;
        for (const auto& pin : options.current_probes) {
            if (pin.first == c.id && d->pin(pin.second) && out.pin_node.count(pin)) {
                const std::string node = "pm_" + ref + "_" + sanitize_name(pin.second);
                const std::string source = "V_pm_" + ref + "_" + sanitize_name(pin.second);
                probe_nodes[pin] = node;
                probe_lines.push_back(source + " " + out.pin_node.at(pin) + " " + node + " DC 0");
                out.pin_current[pin] = lower(source) + "#branch";
                out.elements.push_back({c.id, source, 'V', {out.pin_node.at(pin), node}, "DC 0"});
            }
        }

        std::string control_node;
        if (d->interactive) {
            ControlSource control;
            control.component = c.id;
            control.kind = d->interactive->kind;
            control.source = "V_ctl_" + ref;
            control.node = "ctl_" + ref;
            if (!d->interactive->parameter.empty()) {
                const auto* p = d->parameter(d->interactive->parameter);
                if (p && !p->choices.empty()) {
                    control.initial = words[p->name] == p->choices.back() ? 1.0 : 0.0;
                } else if (p) {
                    control.initial = numbers[p->name] / (p->unit == "%" ? 100.0 : 1.0);
                }
            }
            control.bounce = words.count("bounce") && words["bounce"] == "yes";
            control_node = control.node;
            out.controls.push_back(control);
        }

        const library::ValueLookup lookup = [&](std::string_view name) -> std::optional<double> {
            if (name == "t0") {
                return options.t0;
            }
            const auto it = numbers.find(std::string(name));
            return it == numbers.end() ? std::nullopt : std::optional<double>(it->second);
        };
        if (!subcircuit_name.empty()) {
            Element e;
            e.component = c.id;
            e.name = "X_" + ref;
            e.letter = 'X';
            std::string line = e.name;
            for (const auto& pin : subcircuit_pins) {
                const PinRef pin_ref{c.id, pin};
                const auto probe = probe_nodes.find(pin_ref);
                const std::string node = probe == probe_nodes.end() ? out.pin_node.at(pin_ref) : probe->second;
                line += " " + node;
                e.nodes.push_back(node);
            }
            line += " " + subcircuit_name;
            e.value_text = subcircuit_name;
            out.lines.push_back(line);
            out.elements.push_back(e);
        }
        static const std::vector<std::string> no_template;
        for (const auto& line_template : subcircuit_name.empty() ? d->spice : no_template) {
            std::string error;
            const auto tokens = library::tokenize_template(line_template, error);
            std::string line;
            for (const auto& t : *tokens) {
                switch (t.kind) {
                case library::TemplateToken::Kind::Literal:
                    line += t.text;
                    break;
                case library::TemplateToken::Kind::Ref:
                    line += ref;
                    break;
                case library::TemplateToken::Kind::Control:
                    line += control_node;
                    break;
                case library::TemplateToken::Kind::External: {
                    const std::string node = "ext_" + ref + "_" + t.text;
                    const bool known = std::any_of(out.externals.begin(), out.externals.end(),
                        [&](const ExternalSource& e) { return e.component == c.id && e.name == t.text; });
                    if (!known) {
                        out.externals.push_back({c.id, t.text, "V_ext_" + ref + "_" + t.text, node});
                    }
                    line += node;
                    break;
                }
                case library::TemplateToken::Kind::Node: {
                    const PinRef pin{c.id, t.text};
                    const auto probe = probe_nodes.find(pin);
                    line += probe == probe_nodes.end() ? out.pin_node.at(pin) : probe->second;
                    break;
                }
                case library::TemplateToken::Kind::Parameter:
                    if (words.count(t.text)) {
                        line += words[t.text];
                    } else {
                        line += model::spice_number(numbers[t.text]);
                    }
                    break;
                case library::TemplateToken::Kind::Expression:
                    if (const auto v = library::evaluate_expression(t.text, lookup, error)) {
                        line += model::spice_number(*v);
                    } else {
                        report(Severity::Error, "invalid-value",
                            c.designator + ": cannot compute \"" + t.text + "\" (" + error + ").", {c.id},
                            c.position);
                        line += "0";
                    }
                    break;
                }
            }
            out.lines.push_back(line);
            const auto words_of_line = split(line);
            if (!words_of_line.empty()) {
                Element e;
                e.component = c.id;
                e.name = words_of_line[0];
                e.letter = static_cast<char>(std::toupper(static_cast<unsigned char>(e.name[0])));
                const std::size_t n = node_count(e.letter);
                for (std::size_t i = 1; i <= n && i < words_of_line.size(); ++i) {
                    e.nodes.push_back(words_of_line[i]);
                }
                std::string rest;
                for (std::size_t i = n + 1; i < words_of_line.size(); ++i) {
                    rest += (rest.empty() ? "" : " ") + words_of_line[i];
                }
                e.value_text = rest;
                out.elements.push_back(e);
            }
        }
        for (const auto& m : d->models) {
            if (model_lines.insert(m).second) {
                out.models.push_back(m);
            }
        }
    }
    out.lines.insert(out.lines.end(), probe_lines.begin(), probe_lines.end());
    for (const auto& control : out.controls) {
        // ngspice adds the DC value to an external source's value (vsrcload.c),
        // so an interactive control is DC 0 and the session supplies all of it.
        const double v = options.interactive ? 0.0 : control.initial;
        out.lines.push_back(control.source + " " + control.node + " 0 DC " + model::spice_number(v)
            + (options.interactive ? " external" : ""));
        out.elements.push_back({control.component, control.source, 'V', {control.node, "0"}, ""});
    }
    for (const auto& external : out.externals) {
        out.lines.push_back(external.source + " " + external.node + " 0 DC 0" + (options.interactive ? " external" : ""));
        out.elements.push_back({external.component, external.source, 'V', {external.node, "0"}, ""});
    }

    // ---- Pre-run checks -------------------------------------------------
    const auto& nets = out.connectivity.nets;
    const bool any_ground = std::any_of(nets.begin(), nets.end(), [](const Net& n) { return n.ground; });
    if (has_elements && !any_ground) {
        report(Severity::Error, "no-ground",
            "The circuit has no ground. Place a ground symbol (G) on the reference point.", {});
    }
    for (const auto& pin : out.connectivity.unconnected_pins) {
        const auto* c = schematic.component(pin.first);
        const auto* d = c ? library.find(c->type) : nullptr;
        const auto* p = d ? d->pin(pin.second) : nullptr;
        if (d && d->mcu) {
            continue; // most pins of a microcontroller board stay unused
        }
        report(Severity::Warning, "unconnected-pin",
            (c ? c->designator : std::string("?")) + ": pin " + pin.second + " is not connected.", {pin.first},
            c && d && p ? std::optional<Point>(library::pin_position(*d, *p, *c)) : std::nullopt);
    }
    for (const auto& end : out.connectivity.dangling_ends) {
        Id wire = model::no_id;
        for (const auto& w : schematic.wires()) {
            if (w.points.front() == end || w.points.back() == end) {
                wire = w.id;
            }
        }
        report(Severity::Warning, "open-wire-end", "A wire ends without a connection.", {wire}, end);
    }

    // Voltage sources and inductors in a loop (or a shorted source).
    UnionFind vl;
    for (const auto& e : out.elements) {
        if ((e.letter == 'V' || e.letter == 'L') && e.nodes.size() == 2) {
            if (e.nodes[0] == e.nodes[1]) {
                const auto* c = schematic.component(e.component);
                report(Severity::Error, "shorted-source",
                    (c ? c->designator : e.name) + " is short-circuited: both terminals are on the same net.",
                    {e.component});
            } else if (!vl.unite(e.nodes[0], e.nodes[1])) {
                const auto* c = schematic.component(e.component);
                report(Severity::Error, "source-loop",
                    (c ? c->designator : e.name)
                        + " closes a loop of voltage sources and inductors. Add a resistor in the loop.",
                    {e.component});
            }
        }
    }

    // Every node needs a DC path to ground.
    if (any_ground) {
        UnionFind dc;
        std::set<std::string> nodes;
        for (const auto& e : out.elements) {
            for (const auto& n : e.nodes) {
                nodes.insert(n);
                dc.id(n);
            }
            for (const auto& [a, b] : dc_paths(e.letter)) {
                if (a < e.nodes.size() && b < e.nodes.size()) {
                    dc.unite(e.nodes[a], e.nodes[b]);
                }
            }
            // A subcircuit's inside is not known here: assume its pins are joined.
            for (std::size_t k = 1; e.letter == 'X' && k < e.nodes.size(); ++k) {
                dc.unite(e.nodes[0], e.nodes[k]);
            }
        }
        dc.id("0");
        for (const auto& net : nets) {
            if (net.ground || !nodes.count(net.name) || dc.connected(net.name, "0")) {
                continue;
            }
            std::vector<Id> items;
            std::string names;
            for (const auto& pin : net.pins) {
                if (const auto* c = schematic.component(pin.first)) {
                    if (std::find(items.begin(), items.end(), c->id) == items.end()) {
                        items.push_back(c->id);
                        names += (names.empty() ? "" : ", ") + c->designator;
                    }
                }
            }
            report(Severity::Error, "floating-net",
                "The net connecting " + names
                    + " has no DC path to ground (only capacitors or current sources reach it). Add a resistor to ground or connect it.",
                items);
        }
    }

    for (const auto& probe : schematic.probes()) {
        auto valid = [&](const model::Anchor& a) {
            if (const auto* c = schematic.component(a.item)) {
                const auto* d = library.find(c->type);
                return d && d->pin(a.pin);
            }
            return schematic.wire(a.item) != nullptr;
        };
        if (!valid(probe.target) || (probe.reference && !valid(*probe.reference))) {
            report(Severity::Warning, "invalid-probe", "A probe points at an item that no longer exists.", {probe.id});
        }
    }
    return out;
}

std::vector<std::string> build_deck(const Netlist& netlist, const std::string& title,
    const std::vector<std::string>& extra)
{
    std::vector<std::string> deck{"* " + title};
    deck.insert(deck.end(), netlist.lines.begin(), netlist.lines.end());
    deck.insert(deck.end(), netlist.models.begin(), netlist.models.end());
    deck.insert(deck.end(), extra.begin(), extra.end());
    deck.push_back(".end");
    return deck;
}

} // namespace simnodus::netlist
