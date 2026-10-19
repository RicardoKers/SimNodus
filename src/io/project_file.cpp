// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "project_file.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <random>
#include <set>
#include <sstream>

namespace simnodus::io {
namespace {

using nlohmann::json;
using namespace simnodus::model;

constexpr const char* format_name = "simnodus-project";
constexpr const char* clipboard_name = "simnodus-clipboard";

json point_json(const Point& p)
{
    return json::array({p.x, p.y});
}

Point point_from(const json& j, const char* what)
{
    if (!j.is_array() || j.size() != 2 || !j[0].is_number_integer() || !j[1].is_number_integer()) {
        throw std::runtime_error(std::string(what) + " must be [x, y] with integer grid coordinates");
    }
    return {j[0].get<int>(), j[1].get<int>()};
}

Id id_from(const json& j, const char* what)
{
    if (!j.is_number_unsigned() || j.get<Id>() == no_id) {
        throw std::runtime_error(std::string(what) + " must be a positive integer ID");
    }
    return j.get<Id>();
}

std::string string_from(const json& object, const char* key, bool required = true)
{
    const auto it = object.find(key);
    if (it == object.end()) {
        if (required) {
            throw std::runtime_error(std::string("missing \"") + key + "\"");
        }
        return {};
    }
    if (!it->is_string()) {
        throw std::runtime_error(std::string("\"") + key + "\" must be text");
    }
    return it->get<std::string>();
}

json anchor_json(const Anchor& a)
{
    json j{{"item", a.item}, {"at", point_json(a.point)}};
    if (!a.pin.empty()) {
        j["pin"] = a.pin;
    }
    return j;
}

Anchor anchor_from(const json& j)
{
    if (!j.is_object()) {
        throw std::runtime_error("probe target must be an object");
    }
    return {id_from(j.value("item", json()), "probe target item"), string_from(j, "pin", false),
        point_from(j.value("at", json::array({0, 0})), "probe target position")};
}

json element_json(const Element& element)
{
    return std::visit(
        [](const auto& e) -> json {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Component>) {
                json j{{"id", e.id}, {"type", e.type}, {"designator", e.designator}, {"at", point_json(e.position)},
                    {"rotation", e.orientation.rotation}, {"mirrored", e.orientation.mirrored},
                    {"parameters", e.parameters}};
                if (e.designator_offset) {
                    j["designator_offset"] = point_json(*e.designator_offset);
                }
                if (e.value_offset) {
                    j["value_offset"] = point_json(*e.value_offset);
                }
                if (!e.models.empty()) {
                    j["models"] = e.models;
                }
                return j;
            } else if constexpr (std::is_same_v<T, Wire>) {
                json points = json::array();
                for (const auto& p : e.points) {
                    points.push_back(point_json(p));
                }
                return json{{"id", e.id}, {"points", points}};
            } else if constexpr (std::is_same_v<T, NetLabel>) {
                json j{{"id", e.id}, {"at", point_json(e.position)}, {"name", e.name}};
                if (e.mirrored) {
                    j["mirrored"] = true;
                }
                return j;
            } else if constexpr (std::is_same_v<T, TextNote>) {
                return json{{"id", e.id}, {"at", point_json(e.position)}, {"text", e.text}};
            } else if constexpr (std::is_same_v<T, Bus>) {
                json points = json::array();
                for (const auto& p : e.points) {
                    points.push_back(point_json(p));
                }
                return json{{"id", e.id}, {"points", points}, {"name", e.name}};
            } else if constexpr (std::is_same_v<T, Splitter>) {
                json j{{"id", e.id}, {"at", point_json(e.position)}, {"rotation", e.orientation.rotation},
                    {"mirrored", e.orientation.mirrored}, {"bus", e.bus}, {"first", e.first},
                    {"inputs", e.inputs}, {"outputs", e.outputs}};
                // A bus split into single wires is also written the way SimNodus 0.3 reads it.
                if (e.inputs.size() == 1 && e.outputs == std::vector<int>(static_cast<std::size_t>(e.inputs[0]), 1)) {
                    j["count"] = e.inputs[0];
                }
                return j;
            } else {
                json j{{"id", e.id}, {"kind", e.kind == ProbeKind::Voltage ? "voltage" : "current"},
                    {"target", anchor_json(e.target)}, {"color", e.color}};
                if (e.reference) {
                    j["reference"] = anchor_json(*e.reference);
                }
                return j;
            }
        },
        element);
}

Component component_from(const json& j)
{
    Component c;
    c.id = id_from(j.value("id", json()), "component id");
    c.type = string_from(j, "type");
    c.designator = string_from(j, "designator");
    c.position = point_from(j.value("at", json()), "component position");
    const int rotation = j.value("rotation", 0);
    if (rotation % 90 != 0) {
        throw std::runtime_error("rotation must be a multiple of 90 degrees");
    }
    c.orientation = {normalized_rotation(rotation), j.value("mirrored", false)};
    if (j.contains("parameters")) {
        if (!j["parameters"].is_object()) {
            throw std::runtime_error("parameters must be an object");
        }
        for (const auto& [name, value] : j["parameters"].items()) {
            if (!value.is_string()) {
                throw std::runtime_error("parameter " + name + " must be text");
            }
            c.parameters[name] = value.get<std::string>();
        }
    }
    if (j.contains("designator_offset")) {
        c.designator_offset = point_from(j["designator_offset"], "designator offset");
    }
    if (j.contains("value_offset")) {
        c.value_offset = point_from(j["value_offset"], "value offset");
    }
    if (j.contains("models")) {
        if (!j["models"].is_object()) {
            throw std::runtime_error("models must be an object");
        }
        for (const auto& [name, value] : j["models"].items()) {
            if (!value.is_string()) {
                throw std::runtime_error("model " + name + " must be text");
            }
            c.models[name] = value.get<std::string>();
        }
    }
    return c;
}

Wire wire_from(const json& j)
{
    Wire w;
    w.id = id_from(j.value("id", json()), "wire id");
    const auto points = j.value("points", json::array());
    if (!points.is_array() || points.size() < 2) {
        throw std::runtime_error("a wire needs at least two points");
    }
    for (const auto& p : points) {
        w.points.push_back(point_from(p, "wire point"));
    }
    return w;
}

NetLabel label_from(const json& j)
{
    NetLabel l;
    l.id = id_from(j.value("id", json()), "label id");
    l.position = point_from(j.value("at", json()), "label position");
    l.name = string_from(j, "name");
    if (l.name.empty()) {
        throw std::runtime_error("a net label needs a name");
    }
    l.mirrored = j.value("mirrored", false);
    return l;
}

Bus bus_from(const json& j)
{
    Bus b;
    b.id = id_from(j.value("id", json()), "bus id");
    const auto points = j.value("points", json());
    if (!points.is_array() || points.size() < 2) {
        throw std::runtime_error("a bus needs at least two points");
    }
    for (const auto& p : points) {
        b.points.push_back(point_from(p, "bus point"));
    }
    b.name = string_from(j, "name");
    return b;
}

Splitter splitter_from(const json& j)
{
    Splitter s;
    s.id = id_from(j.value("id", json()), "splitter id");
    s.position = point_from(j.value("at", json()), "splitter position");
    const int rotation = j.value("rotation", 0);
    if (rotation % 90 != 0) {
        throw std::runtime_error("rotation must be a multiple of 90 degrees");
    }
    s.orientation = {normalized_rotation(rotation), j.value("mirrored", false)};
    s.bus = string_from(j, "bus");
    s.first = j.value("first", 0);
    if (s.first < 0 || s.first > 63) {
        throw std::runtime_error("a splitter's first bit must be 0 to 63");
    }
    auto widths = [&](const char* key) {
        const auto list = j.value(key, json());
        std::vector<int> result;
        int total = 0;
        if (!list.is_array() || list.empty()) {
            throw std::runtime_error(std::string("a splitter needs a list of ") + key);
        }
        for (const auto& w : list) {
            if (!w.is_number_integer() || w.get<int>() < 1) {
                throw std::runtime_error("splitter widths must be positive integers");
            }
            result.push_back(w.get<int>());
            total += result.back();
        }
        if (total > 64) {
            throw std::runtime_error("a splitter has at most 64 bits");
        }
        return result;
    };
    if (j.contains("inputs") || j.contains("outputs")) {
        s.inputs = widths("inputs");
        s.outputs = widths("outputs");
    } else {
        // Before 0.4: a bus point and `count` single bits.
        const int count = j.value("count", 8);
        if (count < 1 || count > 64) {
            throw std::runtime_error("a splitter needs 1 to 64 bits");
        }
        s.inputs = {count};
        s.outputs = std::vector<int>(static_cast<std::size_t>(count), 1);
    }
    return s;
}

TextNote note_from(const json& j)
{
    TextNote n;
    n.id = id_from(j.value("id", json()), "note id");
    n.position = point_from(j.value("at", json()), "note position");
    n.text = string_from(j, "text");
    return n;
}

Probe probe_from(const json& j)
{
    Probe p;
    p.id = id_from(j.value("id", json()), "probe id");
    const std::string kind = string_from(j, "kind");
    if (kind != "voltage" && kind != "current") {
        throw std::runtime_error("probe kind must be voltage or current");
    }
    p.kind = kind == "voltage" ? ProbeKind::Voltage : ProbeKind::Current;
    p.target = anchor_from(j.value("target", json()));
    if (j.contains("reference")) {
        p.reference = anchor_from(j["reference"]);
    }
    p.color = string_from(j, "color", false);
    return p;
}

json settings_json(const SimulationSettings& s)
{
    return json{{"mode", s.mode == SimulationMode::Interactive ? "interactive" : "transient"}, {"speed", s.speed},
        {"max_step", s.max_step}, {"segment", s.segment}, {"transient_stop", s.transient_stop},
        {"transient_max_step", s.transient_max_step}, {"mcu_quantum", s.mcu_quantum}, {"exact_edges", s.exact_edges}};
}

SimulationSettings settings_from(const json& j, std::vector<std::string>& warnings)
{
    SimulationSettings s;
    if (!j.is_object()) {
        return s;
    }
    const std::string mode = j.value("mode", std::string("interactive"));
    s.mode = mode == "transient" ? SimulationMode::Transient : SimulationMode::Interactive;
    auto positive = [&](const char* key, double& target, bool zero_ok) {
        if (!j.contains(key)) {
            return;
        }
        if (!j[key].is_number()) {
            warnings.push_back(std::string("setting ") + key + " is not a number; default used");
            return;
        }
        const double v = j[key].get<double>();
        if (v < 0 || (!zero_ok && v == 0)) {
            warnings.push_back(std::string("setting ") + key + " out of range; default used");
            return;
        }
        target = v;
    };
    positive("speed", s.speed, true);
    positive("max_step", s.max_step, false);
    positive("segment", s.segment, false);
    positive("transient_stop", s.transient_stop, false);
    positive("transient_max_step", s.transient_max_step, false);
    positive("mcu_quantum", s.mcu_quantum, false);
    if (s.mcu_quantum < 1e-4 || s.mcu_quantum > 0.02) {
        warnings.push_back("setting mcu_quantum out of range (0.1 ms to 20 ms); default used");
        s.mcu_quantum = SimulationSettings{}.mcu_quantum;
    }
    if (j.contains("exact_edges")) {
        if (j["exact_edges"].is_boolean()) {
            s.exact_edges = j["exact_edges"].get<bool>();
        } else {
            warnings.push_back("setting exact_edges is not true or false; default used");
        }
    }
    return s;
}

template <typename F>
void each(const json& root, const char* key, F f)
{
    if (!root.contains(key)) {
        return;
    }
    if (!root[key].is_array()) {
        throw std::runtime_error(std::string("\"") + key + "\" must be a list");
    }
    std::size_t index = 0;
    for (const auto& item : root[key]) {
        try {
            if (!item.is_object()) {
                throw std::runtime_error("entries must be objects");
            }
            f(item);
        } catch (const std::exception& error) {
            throw std::runtime_error(std::string(key) + " entry " + std::to_string(index + 1) + ": " + error.what());
        }
        ++index;
    }
}

std::vector<Element> read_elements(const json& root)
{
    std::vector<Element> elements;
    each(root, "components", [&](const json& j) { elements.emplace_back(component_from(j)); });
    each(root, "wires", [&](const json& j) { elements.emplace_back(wire_from(j)); });
    each(root, "labels", [&](const json& j) { elements.emplace_back(label_from(j)); });
    each(root, "probes", [&](const json& j) { elements.emplace_back(probe_from(j)); });
    each(root, "notes", [&](const json& j) { elements.emplace_back(note_from(j)); });
    each(root, "buses", [&](const json& j) { elements.emplace_back(bus_from(j)); });
    each(root, "splitters", [&](const json& j) { elements.emplace_back(splitter_from(j)); });
    return elements;
}

json write_elements(json root, const std::vector<Element>& elements)
{
    root["components"] = json::array();
    root["wires"] = json::array();
    root["labels"] = json::array();
    root["probes"] = json::array();
    root["notes"] = json::array();
    root["buses"] = json::array();
    root["splitters"] = json::array();
    for (const auto& e : elements) {
        const char* key = std::holds_alternative<Component>(e) ? "components"
            : std::holds_alternative<Wire>(e)                  ? "wires"
            : std::holds_alternative<NetLabel>(e)              ? "labels"
            : std::holds_alternative<TextNote>(e)              ? "notes"
            : std::holds_alternative<Bus>(e)                   ? "buses"
            : std::holds_alternative<Splitter>(e)              ? "splitters"
                                                               : "probes";
        root[key].push_back(element_json(e));
    }
    return root;
}

std::vector<Element> all_elements(const Schematic& s)
{
    std::vector<Element> result;
    for (const auto& c : s.components()) {
        result.emplace_back(c);
    }
    for (const auto& w : s.wires()) {
        result.emplace_back(w);
    }
    for (const auto& l : s.labels()) {
        result.emplace_back(l);
    }
    for (const auto& p : s.probes()) {
        result.emplace_back(p);
    }
    for (const auto& n : s.notes()) {
        result.emplace_back(n);
    }
    for (const auto& b : s.buses()) {
        result.emplace_back(b);
    }
    for (const auto& sp : s.splitters()) {
        result.emplace_back(sp);
    }
    return result;
}

json blocks_json(const std::map<std::string, Block>& blocks);

json project_json(const Schematic& schematic)
{
    // Format 2 only adds blocks; a project without them stays readable by 0.3.
    json root{{"format", format_name}, {"version", schematic.blocks().empty() ? 1 : project_format_version},
        {"next_id", schematic.next_id()}, {"settings", settings_json(schematic.settings())}};
    root = write_elements(root, all_elements(schematic));
    if (!schematic.blocks().empty()) {
        root["blocks"] = blocks_json(schematic.blocks());
    }
    return root;
}

json blocks_json(const std::map<std::string, Block>& blocks)
{
    json list = json::array();
    for (const auto& [name, block] : blocks) {
        list.push_back(json{{"name", name}, {"source", block.source}, {"project", project_json(*block.schematic)}});
    }
    return list;
}

// Blocks inside blocks are read to this depth; deeper files are refused.
constexpr int max_block_depth = 8;

Schematic schematic_from(const json& root, std::vector<std::string>& warnings, int depth);

std::map<std::string, Block> blocks_from(const json& root, std::vector<std::string>& warnings, int depth)
{
    std::map<std::string, Block> blocks;
    each(root, "blocks", [&](const json& j) {
        if (depth >= max_block_depth) {
            throw std::runtime_error("blocks are nested more than " + std::to_string(max_block_depth) + " levels deep");
        }
        Block b;
        b.name = string_from(j, "name");
        if (b.name.empty()) {
            throw std::runtime_error("a block needs a name");
        }
        b.source = string_from(j, "source", false);
        const auto project = j.value("project", json());
        if (!project.is_object()) {
            throw std::runtime_error("block " + b.name + " has no project");
        }
        b.schematic = std::make_shared<const Schematic>(schematic_from(project, warnings, depth + 1));
        if (!blocks.emplace(b.name, b).second) {
            throw std::runtime_error("two blocks are named " + b.name);
        }
    });
    return blocks;
}

Schematic schematic_from(const json& root, std::vector<std::string>& warnings, int depth)
{
    Schematic schematic;
    std::set<Id> ids;
    for (const auto& e : read_elements(root)) {
        if (!ids.insert(element_id(e)).second) {
            throw std::runtime_error("duplicate element ID " + std::to_string(element_id(e)));
        }
        schematic.put(e);
    }
    schematic.set_settings(settings_from(root.value("settings", json::object()), warnings));
    schematic.set_blocks(blocks_from(root, warnings, depth));
    const auto next = root.value("next_id", json(1));
    schematic.set_next_id(next.is_number_unsigned() ? next.get<Id>() : 1);
    return schematic;
}

} // namespace

std::string to_json_text(const Schematic& schematic)
{
    return project_json(schematic).dump(2) + "\n";
}

LoadResult from_json_text(const std::string& text)
{
    LoadResult result;
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error& error) {
        result.errors.push_back(std::string("The file is not valid JSON: ") + error.what());
        return result;
    }
    if (!root.is_object() || root.value("format", std::string()) != format_name) {
        result.errors.push_back("This is not a SimNodus project file.");
        return result;
    }
    const auto version = root.value("version", 0);
    if (version < 1) {
        result.errors.push_back("The project file has no valid format version.");
        return result;
    }
    if (version > project_format_version) {
        result.errors.push_back("The project was written by a newer SimNodus (format " + std::to_string(version)
            + "); this version reads format " + std::to_string(project_format_version) + ".");
        return result;
    }
    try {
        result.schematic = schematic_from(root, result.warnings, 0);
    } catch (const std::exception& error) {
        result.errors.push_back(error.what());
    }
    return result;
}

std::string elements_to_json_text(const std::vector<Element>& elements, const std::map<std::string, Block>& blocks)
{
    json root = write_elements(json{{"format", clipboard_name}, {"version", project_format_version}}, elements);
    if (!blocks.empty()) {
        root["blocks"] = blocks_json(blocks);
    }
    return root.dump();
}

std::optional<std::vector<Element>> elements_from_json_text(const std::string& text, std::string& error,
    std::map<std::string, Block>* blocks)
{
    try {
        const json root = json::parse(text);
        if (!root.is_object() || root.value("format", std::string()) != clipboard_name) {
            error = "the clipboard does not contain SimNodus elements";
            return std::nullopt;
        }
        if (blocks) {
            std::vector<std::string> ignored;
            *blocks = blocks_from(root, ignored, 0);
        }
        return read_elements(root);
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

LoadResult load_project(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        LoadResult result;
        result.errors.push_back("Cannot open " + file.string());
        return result;
    }
    std::ostringstream content;
    content << input.rdbuf();
    return from_json_text(content.str());
}

SaveResult save_project(const Schematic& schematic, const std::filesystem::path& file)
{
    SaveResult result;
    namespace fs = std::filesystem;
    std::mt19937_64 random(static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path temporary = file.parent_path()
        / (file.filename().string() + ".tmp-" + std::to_string(random() % 1000000000ULL));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            result.error = "Cannot write in " + file.parent_path().string();
            return result;
        }
        output << to_json_text(schematic);
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            fs::remove(temporary, ignored);
            result.error = "Writing " + temporary.string() + " failed (disk full?)";
            return result;
        }
    }
    std::error_code ec;
    if (fs::exists(file, ec)) {
        const fs::path backup = file.string() + ".bak";
        fs::copy_file(file, backup, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.backup = backup;
        }
    }
    fs::rename(temporary, file, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        result.error = "Cannot replace " + file.string() + ": " + ec.message();
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace simnodus::io
