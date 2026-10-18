// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "component_library.hpp"

#include "spice_template.hpp"

#include "model/engineering.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace simnodus::library {
namespace {

using nlohmann::json;

const std::vector<std::string> category_order{
    "Passive", "Sources", "Switches & Controls", "Semiconductors", "Integrated circuits", "Electromechanical",
    "Instruments", "Microcontrollers", "Logic"};

[[noreturn]] void invalid(const std::string& message)
{
    throw std::runtime_error(message);
}

double number(const json& value, const char* what)
{
    if (!value.is_number()) {
        invalid(std::string(what) + ": expected a number");
    }
    return value.get<double>();
}

std::vector<double> numbers(const json& value, std::size_t minimum, const char* what)
{
    if (!value.is_array() || value.size() < minimum) {
        invalid(std::string(what) + ": expected at least " + std::to_string(minimum) + " numbers");
    }
    std::vector<double> result;
    for (const auto& v : value) {
        result.push_back(number(v, what));
    }
    return result;
}

std::string text(const json& object, const char* key, bool required = false)
{
    const auto it = object.find(key);
    if (it == object.end()) {
        if (required) {
            invalid(std::string("missing \"") + key + "\"");
        }
        return {};
    }
    if (!it->is_string()) {
        invalid(std::string("\"") + key + "\" must be a string");
    }
    return it->get<std::string>();
}

std::vector<std::string> strings(const json& object, const char* key)
{
    std::vector<std::string> result;
    const auto it = object.find(key);
    if (it == object.end()) {
        return result;
    }
    if (!it->is_array()) {
        invalid(std::string("\"") + key + "\" must be a list of strings");
    }
    for (const auto& v : *it) {
        if (!v.is_string()) {
            invalid(std::string("\"") + key + "\" must be a list of strings");
        }
        result.push_back(v.get<std::string>());
    }
    return result;
}

model::Point grid_point(const json& value, const char* what)
{
    const auto v = numbers(value, 2, what);
    if (v[0] != static_cast<int>(v[0]) || v[1] != static_cast<int>(v[1])) {
        invalid(std::string(what) + ": pins and offsets must be on integer grid points");
    }
    return {static_cast<int>(v[0]), static_cast<int>(v[1])};
}

Graphic parse_graphic(const json& g)
{
    Graphic result;
    if (!g.is_object()) {
        invalid("graphic entries must be objects");
    }
    result.filled = g.value("fill", false);
    if (g.contains("line")) {
        const auto v = numbers(g["line"], 4, "line");
        result.kind = Graphic::Kind::Line;
        result.points = {{v[0], v[1]}, {v[2], v[3]}};
    } else if (g.contains("rect")) {
        const auto v = numbers(g["rect"], 4, "rect");
        result.kind = Graphic::Kind::Rect;
        result.points = {{v[0], v[1]}, {v[2], v[3]}};
    } else if (g.contains("circle")) {
        const auto v = numbers(g["circle"], 3, "circle");
        result.kind = Graphic::Kind::Circle;
        result.points = {{v[0], v[1]}};
        result.radius = v[2];
    } else if (g.contains("arc")) {
        const auto v = numbers(g["arc"], 5, "arc");
        result.kind = Graphic::Kind::Arc;
        result.points = {{v[0], v[1]}};
        result.radius = v[2];
        result.start_degrees = v[3];
        result.span_degrees = v[4];
    } else if (g.contains("polyline") || g.contains("polygon")) {
        const bool polygon = g.contains("polygon");
        const auto v = numbers(polygon ? g["polygon"] : g["polyline"], 4, "polyline");
        if (v.size() % 2 != 0) {
            invalid("polyline: odd number of coordinates");
        }
        result.kind = polygon ? Graphic::Kind::Polygon : Graphic::Kind::Polyline;
        for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
            result.points.push_back({v[i], v[i + 1]});
        }
    } else if (g.contains("text")) {
        result.kind = Graphic::Kind::Text;
        result.text = text(g, "text", true);
        const auto at = numbers(g.value("at", json::array({0, 0})), 2, "text.at");
        result.points = {{at[0], at[1]}};
        result.text_size = g.value("size", 1.0);
    } else {
        invalid("unknown graphic (expected line, rect, circle, arc, polyline, polygon or text)");
    }
    return result;
}

std::vector<Graphic> parse_graphics(const json& list)
{
    if (!list.is_array()) {
        invalid("graphics must be a list");
    }
    std::vector<Graphic> result;
    for (const auto& g : list) {
        result.push_back(parse_graphic(g));
    }
    return result;
}

PinDirection parse_direction(const std::string& d)
{
    if (d == "left") {
        return PinDirection::Left;
    }
    if (d == "right") {
        return PinDirection::Right;
    }
    if (d == "up") {
        return PinDirection::Up;
    }
    if (d == "down") {
        return PinDirection::Down;
    }
    invalid("pin direction must be left, right, up or down");
}

std::optional<double> bound(const json& p, const char* key, const std::string& unit)
{
    const std::string value = text(p, key);
    if (value.empty()) {
        return std::nullopt;
    }
    auto parsed = model::parse_engineering(value, unit);
    if (!parsed) {
        invalid(std::string("parameter ") + key + " \"" + value + "\" is not a number");
    }
    return parsed;
}

void validate_templates(const ComponentDefinition& d)
{
    for (const auto& line : d.spice) {
        std::string error;
        const auto tokens = tokenize_template(line, error);
        if (!tokens) {
            invalid("spice template \"" + line + "\": " + error);
        }
        for (const auto& t : *tokens) {
            if (t.kind == TemplateToken::Kind::Node && !d.pin(t.text)) {
                invalid("spice template uses unknown pin \"" + t.text + "\"");
            }
            if (t.kind == TemplateToken::Kind::Parameter && !d.parameter(t.text)) {
                invalid("spice template uses unknown parameter \"" + t.text + "\"");
            }
            if (t.kind == TemplateToken::Kind::Control && !d.interactive) {
                invalid("spice template uses {control} but the component is not interactive");
            }
            if (t.kind == TemplateToken::Kind::Expression) {
                for (const auto& name : expression_names(t.text)) {
                    if (name != "t0" && !d.parameter(name)) {
                        invalid("expression \"" + t.text + "\" uses unknown parameter \"" + name + "\"");
                    }
                }
            }
        }
    }
}

ComponentDefinition parse_definition(const json& j)
{
    if (!j.is_object()) {
        invalid("a component definition must be a JSON object");
    }
    ComponentDefinition d;
    d.type = text(j, "type", true);
    d.name = text(j, "name", true);
    d.category = text(j, "category", true);
    d.prefix = text(j, "prefix", true);
    d.description = text(j, "description");
    d.value_parameter = text(j, "value_parameter");
    d.ties_net = text(j, "ties_net");
    d.show_designator = j.value("show_designator", true);
    if (j.contains("designator_offset")) {
        d.designator_offset = grid_point(j["designator_offset"], "designator_offset");
    }
    if (j.contains("value_offset")) {
        d.value_offset = grid_point(j["value_offset"], "value_offset");
    }

    const auto symbol = j.find("symbol");
    if (symbol == j.end() || !symbol->is_object()) {
        invalid("missing \"symbol\"");
    }
    d.graphics = parse_graphics(symbol->value("graphics", json::array()));
    if (symbol->contains("states")) {
        const auto& states = (*symbol)["states"];
        if (!states.is_object()) {
            invalid("symbol.states must map a state to graphics");
        }
        for (const auto& [state, list] : states.items()) {
            d.state_graphics[state] = parse_graphics(list);
        }
    }
    const auto pins = symbol->find("pins");
    if (pins == symbol->end() || !pins->is_array() || pins->empty()) {
        invalid("a symbol needs at least one pin");
    }
    std::set<std::string> pin_names;
    std::set<model::Point> pin_points;
    for (const auto& p : *pins) {
        Pin pin;
        pin.name = text(p, "name", true);
        pin.at = grid_point(p.value("at", json()), "pin.at");
        pin.direction = parse_direction(text(p, "dir", true));
        if (!pin_names.insert(pin.name).second) {
            invalid("duplicate pin \"" + pin.name + "\"");
        }
        if (!pin_points.insert(pin.at).second) {
            invalid("two pins at the same point");
        }
        d.pins.push_back(pin);
    }

    if (j.contains("parameters")) {
        if (!j["parameters"].is_array()) {
            invalid("\"parameters\" must be a list");
        }
        for (const auto& p : j["parameters"]) {
            Parameter parameter;
            parameter.name = text(p, "name", true);
            parameter.label = text(p, "label");
            if (parameter.label.empty()) {
                parameter.label = parameter.name;
            }
            parameter.unit = text(p, "unit");
            parameter.default_value = text(p, "default", true);
            parameter.description = text(p, "description");
            parameter.choices = strings(p, "choices");
            parameter.filter = text(p, "filter");
            const std::string kind = text(p, "kind");
            if (kind == "file") {
                parameter.kind = Parameter::Kind::File;
            } else if (!kind.empty() && kind != "number" && kind != "choice") {
                invalid("parameter kind must be number, choice or file");
            } else if (!parameter.choices.empty()) {
                parameter.kind = Parameter::Kind::Choice;
            }
            parameter.min = bound(p, "min", parameter.unit);
            parameter.max = bound(p, "max", parameter.unit);
            if (parameter.kind == Parameter::Kind::File) {
                // any text, including empty
            } else if (parameter.choices.empty()) {
                if (!model::parse_engineering(parameter.default_value, parameter.unit)) {
                    invalid("default of \"" + parameter.name + "\" is not a number");
                }
            } else if (std::find(parameter.choices.begin(), parameter.choices.end(), parameter.default_value)
                == parameter.choices.end()) {
                invalid("default of \"" + parameter.name + "\" is not one of its choices");
            }
            if (d.parameter(parameter.name)) {
                invalid("duplicate parameter \"" + parameter.name + "\"");
            }
            d.parameters.push_back(parameter);
        }
    }
    if (!d.value_parameter.empty() && !d.parameter(d.value_parameter)) {
        invalid("value_parameter \"" + d.value_parameter + "\" is not a parameter");
    }

    if (j.contains("interactive") && !j["interactive"].is_null()) {
        const auto& i = j["interactive"];
        Interactive interactive;
        const std::string kind = text(i, "kind", true);
        if (kind == "toggle") {
            interactive.kind = Interactive::Kind::Toggle;
        } else if (kind == "button") {
            interactive.kind = Interactive::Kind::Button;
        } else if (kind == "potentiometer") {
            interactive.kind = Interactive::Kind::Potentiometer;
        } else {
            invalid("interactive.kind must be toggle, button or potentiometer");
        }
        interactive.parameter = text(i, "parameter");
        if (!interactive.parameter.empty() && !d.parameter(interactive.parameter)) {
            invalid("interactive.parameter is not a parameter");
        }
        d.interactive = interactive;
    }
    if (j.contains("readout") && !j["readout"].is_null()) {
        const auto& r = j["readout"];
        Readout readout;
        const std::string kind = text(r, "kind", true);
        if (kind == "voltage") {
            readout.kind = Readout::Kind::Voltage;
            readout.pins = strings(r, "pins");
            if (readout.pins.size() != 2 || !d.pin(readout.pins[0]) || !d.pin(readout.pins[1])) {
                invalid("a voltage readout needs two existing pins");
            }
        } else if (kind == "current") {
            readout.kind = Readout::Kind::Current;
            readout.element = text(r, "element", true);
            if (r.contains("glow")) {
                const auto& g = r["glow"];
                if (g.contains("full")) {
                    const auto full = model::parse_engineering(text(g, "full"), "A");
                    if (!full || *full <= 0) {
                        invalid("readout.glow.full must be a positive current");
                    }
                    readout.glow_full = *full;
                }
                readout.glow_full_parameter = text(g, "full_parameter");
                readout.glow_color = text(g, "color");
                readout.glow_color_parameter = text(g, "color_parameter");
                if (!readout.glows()) {
                    invalid("readout.glow needs full or full_parameter");
                }
            }
        } else if (kind == "node") {
            readout.kind = Readout::Kind::Node;
            readout.element = text(r, "element", true);
            readout.unit = text(r, "unit");
            if (r.contains("scale")) {
                readout.scale = number(r["scale"], "readout.scale");
            }
        } else {
            invalid("readout.kind must be voltage, current or node");
        }
        d.readout = readout;
    }

    if (j.contains("mcu") && !j["mcu"].is_null()) {
        const auto& m = j["mcu"];
        McuBoard board;
        board.platform = text(m, "platform", true);
        if (const std::string engine = text(m, "engine"); !engine.empty()) {
            if (engine != "renode" && engine != "native") {
                invalid("mcu.engine must be \"renode\" or \"native\"");
            }
            board.engine = engine;
        }
        board.extensions = strings(m, "extensions");
        board.before_firmware = strings(m, "before_firmware");
        board.after_firmware = strings(m, "after_firmware");
        board.firmware_parameter = text(m, "firmware_parameter", true);
        const auto* firmware = d.parameter(board.firmware_parameter);
        if (!firmware || firmware->kind != Parameter::Kind::File) {
            invalid("mcu.firmware_parameter must name a file parameter");
        }
        if (m.contains("ground_pin")) {
            board.ground_pin = text(m, "ground_pin", true);
        }
        if (!d.pin(board.ground_pin)) {
            invalid("mcu.ground_pin is not a pin of the board");
        }
        if (m.contains("pin_model")) {
            const auto& pm = m["pin_model"];
            board.pin_model.supply = number(pm.value("supply", json(3.3)), "mcu.pin_model.supply");
            board.pin_model.output_resistance =
                number(pm.value("output_resistance", json(40.0)), "mcu.pin_model.output_resistance");
            board.pin_model.pull_resistance =
                number(pm.value("pull_resistance", json(40000.0)), "mcu.pin_model.pull_resistance");
            board.pin_model.input_high = number(pm.value("input_high", json(1.83)), "mcu.pin_model.input_high");
            board.pin_model.input_low = number(pm.value("input_low", json(1.16)), "mcu.pin_model.input_low");
            if (board.pin_model.supply <= 0 || board.pin_model.output_resistance <= 0
                || board.pin_model.pull_resistance <= 0) {
                invalid("mcu.pin_model values must be positive");
            }
            if (!(board.pin_model.input_low < board.pin_model.input_high)
                || board.pin_model.input_high > board.pin_model.supply) {
                invalid("mcu.pin_model needs input_low < input_high <= supply");
            }
        }
        if (m.contains("led")) {
            McuBoard::Led led;
            led.pin = text(m["led"], "pin", true);
            led.active_low = m["led"].value("active_low", false);
            const auto at = numbers(m["led"]["at"], 2, "mcu.led.at");
            led.at = {at[0], at[1]};
            if (m["led"].contains("radius")) {
                led.radius = number(m["led"]["radius"], "mcu.led.radius");
            }
            if (led.pin.size() < 3 || led.pin[0] != 'P') {
                invalid("mcu.led.pin must be a GPIO pin name such as PC13");
            }
            board.led = led;
        }
        if (m.contains("uarts")) {
            for (const auto& u : m["uarts"]) {
                McuBoard::Uart uart{text(u, "name", true), text(u, "label"), text(u, "tx_pin"), text(u, "rx_pin")};
                if (uart.label.empty()) {
                    uart.label = uart.name;
                }
                board.uarts.push_back(uart);
            }
        }
        if (m.contains("reset_button")) {
            const auto r = numbers(m["reset_button"], 4, "mcu.reset_button");
            board.reset_button = std::vector<model::PointF>{{r[0], r[1]}, {r[2], r[3]}};
        }
        d.mcu = board;
    }

    d.spice = strings(j, "spice");
    d.models = strings(j, "models");
    if (d.spice.empty() && d.ties_net.empty()) {
        invalid("a component needs a spice template or ties_net");
    }
    validate_templates(d);
    return d;
}

} // namespace

const Pin* ComponentDefinition::pin(const std::string& pin_name) const
{
    for (const auto& p : pins) {
        if (p.name == pin_name) {
            return &p;
        }
    }
    return nullptr;
}

const Parameter* ComponentDefinition::parameter(const std::string& parameter_name) const
{
    for (const auto& p : parameters) {
        if (p.name == parameter_name) {
            return &p;
        }
    }
    return nullptr;
}

std::map<std::string, std::string> ComponentDefinition::default_parameters() const
{
    std::map<std::string, std::string> result;
    for (const auto& p : parameters) {
        result[p.name] = p.default_value;
    }
    return result;
}

std::optional<std::string> ComponentLibrary::add_from_json(const std::string& text, const std::filesystem::path& source)
{
    try {
        auto definition = parse_definition(json::parse(text));
        if (find(definition.type)) {
            return "duplicate component type \"" + definition.type + "\"";
        }
        definition.source = source;
        definitions_.push_back(std::move(definition));
        return std::nullopt;
    } catch (const std::exception& error) {
        return std::string(error.what());
    }
}

std::vector<LoadMessage> ComponentLibrary::load_directory(const std::filesystem::path& directory)
{
    std::vector<LoadMessage> messages;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        messages.push_back({directory, "library folder not found"});
        return messages;
    }
    std::vector<std::filesystem::path> files;
    for (auto it = std::filesystem::recursive_directory_iterator(directory, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".json") {
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        std::ifstream input(file, std::ios::binary);
        std::ostringstream content;
        content << input.rdbuf();
        if (auto error = add_from_json(content.str(), file)) {
            messages.push_back({file, *error});
        }
    }
    return messages;
}

const ComponentDefinition* ComponentLibrary::find(const std::string& type) const
{
    for (const auto& d : definitions_) {
        if (d.type == type) {
            return &d;
        }
    }
    return nullptr;
}

std::vector<std::string> ComponentLibrary::categories() const
{
    std::vector<std::string> result;
    for (const auto& c : category_order) {
        if (std::any_of(definitions_.begin(), definitions_.end(), [&](const auto& d) { return d.category == c; })) {
            result.push_back(c);
        }
    }
    std::set<std::string> others;
    for (const auto& d : definitions_) {
        if (std::find(category_order.begin(), category_order.end(), d.category) == category_order.end()) {
            others.insert(d.category);
        }
    }
    result.insert(result.end(), others.begin(), others.end());
    return result;
}

model::Point pin_position(const ComponentDefinition&, const Pin& pin, const model::Component& component)
{
    return model::transform(pin.at, component.position, component.orientation);
}

std::vector<std::filesystem::path> library_candidates(const std::filesystem::path& application_dir)
{
    return {
        application_dir / "library",
        application_dir / ".." / "share" / "simnodus" / "library",
#ifdef SIMNODUS_SOURCE_LIBRARY_DIR
        std::filesystem::path(SIMNODUS_SOURCE_LIBRARY_DIR),
#endif
    };
}

} // namespace simnodus::library
