// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Component library (PRJ-04): one JSON file per component type, describing
// the symbol (graphics and pins, in grid units), the parameters, the SPICE
// template and optional interactive behavior. Nothing in C++ is specific to
// one component type; a future Symbol Editor writes the same files.
// Format: docs/architecture/COMPONENT_LIBRARY.md.
#pragma once

#include "model/geometry.hpp"
#include "model/schematic.hpp"
#include "spice_model.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::library {

enum class PinDirection { Left, Right, Up, Down };

struct Pin {
    std::string name;
    model::Point at; // grid units, relative to the symbol origin
    PinDirection direction = PinDirection::Up;
    bool optional = false; // may stay unconnected without a warning (an unused Q-bar or clear input)
};

struct Graphic {
    enum class Kind { Line, Rect, Circle, Arc, Polyline, Polygon, Text };
    Kind kind = Kind::Line;
    std::vector<model::PointF> points; // line: 2; rect: top-left + size; circle/arc: center
    double radius = 0.0;
    double start_degrees = 0.0; // arc: counter-clockwise from 3 o'clock, as drawn on screen
    double span_degrees = 0.0;
    bool filled = false;
    std::string text;
    double text_size = 1.0; // grid units
};

struct Parameter {
    // Number (engineering notation), Choice (one of `choices`), File (a path,
    // stored relative to the project when possible; `filter` like "*.elf"),
    // Model (the name of a SPICE model of type `model_type`, PRJ-07) or Text
    // (any words, a block port's name).
    enum class Kind { Number, Choice, File, Model, Text };
    Kind kind = Kind::Number;
    std::string name;
    std::string label;
    std::string unit; // "Ohm", "F", "H", "V", "A", "Hz", "s", "%", "deg" or empty
    std::string default_value;
    std::optional<double> min;
    std::optional<double> max;
    std::vector<std::string> choices; // non-empty: the value is one of these words
    std::string description;
    std::string filter;
    std::string symbol;     // shown before the value on the schematic ("β" gives "β=100"); empty: the value alone
    std::string model_type; // Model: "NPN", "PNP", "D", "NMOS" or "PMOS"; empty: subcircuits only
    // A chip's parameter that may change while running (an LM75's temperature):
    // the editor sends a new value to the running chip.
    bool live = false;
    // Text: the name of a serial port of the computer (SN-237); the editor offers the ports it finds.
    bool serial_port = false;
    // A board's link to the computer (SN-252): the serial ports, "tcp:4000"
    // and none (empty) to choose from.
    bool host_link = false;
    // Model: the part's pins in the usual port order of vendor subcircuits
    // ("IN+", "IN-", "V+", "V-", "OUT"); empty: no subcircuits (ADR 0097).
    std::vector<std::string> subcircuit;
    // Model: the "type=" values of the models this part offers ("schottky"); an
    // empty string stands for models without one. Empty list: any model of the type.
    std::vector<std::string> model_kinds;
    // Choice: the part's orientation for some values (a block port's side turns
    // the port so that its wire attaches on that side).
    std::map<std::string, model::Orientation> orientations;
};

// Microcontroller board (MCU-01, INT-06): runs firmware in SimNodus's own
// emulator (ADR 0089, 0094, 0106). The chip's GPIO pins are named as on the
// chip ("PA0" on the STM32F103, "GPIO0" on the ESP32-C3); the others are power
// or control pins.
struct McuBoard {
    std::string chip = "stm32f103";       // or "esp32c3" (experimental, ADR 0106)
    std::string firmware_parameter;       // a File parameter
    std::string ground_pin = "G1";        // reference of the pin voltages
    // Electrical GPIO model (MCU-03): driven outputs through a resistance to the
    // supply or ground, internal pull resistors of inputs.
    struct PinModel {
        double supply = 3.3;              // V, relative to the board ground pin
        double output_resistance = 40.0;  // Ohm
        double pull_resistance = 40000.0; // Ohm
        double input_high = 1.83;         // V: an input reads 1 above this (Schmitt, rising)
        double input_low = 1.16;          // V: and 0 below this (falling)
    };
    PinModel pin_model;
    struct Led {
        std::string pin;         // GPIO driving the on-board LED
        bool active_low = false; // lit while the pin is low
        model::PointF at;        // symbol coordinates
        double radius = 0.6;
    };
    std::optional<Led> led;
    std::optional<std::vector<model::PointF>> reset_button; // top-left and size, symbol coordinates
    struct Uart {
        std::string name;  // peripheral ("usart1")
        std::string label; // terminal text ("USART1 (PA9 TX, PA10 RX)")
        std::string tx_pin; // "PA9": held high while the transmitter is on; empty if unknown
        std::string rx_pin; // "PA10": a board's TX on the same net sends it bytes (SN-175)
        // The board's USB connector (SN-252): the parameter holding the link
        // to the computer ("COM7", "tcp:4000", empty: none), and the board's
        // circuit from the link's DTR and RTS ("auto_reset", "usb_jtag", empty).
        std::string link_parameter;
        std::string reset_circuit;
    };
    std::vector<Uart> uarts; // serial ports for the terminal (MCU-05)
};

// A character display area of a symbol (SN-206): the editor draws the text of
// the part's display chip there, in the rows and columns its parameters give.
struct Screen {
    model::PointF at;   // top-left, symbol coordinates
    model::PointF size;
    std::string rows_parameter = "rows";
    std::string columns_parameter = "columns";
    // A graphic display (SN-223: SSD1306 OLED): the chip's text is "WxH" and one
    // row of hex digits per line (4 pixels each, the leftmost in the high bit);
    // the pixels are lit in the color of the parameter `color_parameter`.
    bool pixels = false;
    std::string color_parameter;
};

// A digital device on a microcontroller's nets (SN-203, ADR 0102): its logic
// runs in the microcontroller runner (src/devices); its template only gives
// each output pin a driver ({ext:<pin>_g}, {ext:<pin>_i}) like a board pin.
struct DigitalDevice {
    std::string kind;              // "74hc595"
    std::string supply_pin = "VCC";
    // A fixed supply instead of a supply pin (SN-237: a serial port's TTL level):
    // the volts at the start of this parameter's value ("3.3 V"); supply_pin is empty.
    std::string supply_parameter;
    std::string ground_pin = "GND";
    double output_resistance = 50.0; // Ohm, of driven outputs
    // Push-pull outputs that always drive: a net with one of them never needs
    // the circuit's view (SN-206).
    std::vector<std::string> driven_pins;
    // Pins whose voltage the chip reads (SN-224: an A/D converter's inputs and
    // reference); they reach the device as volts as well as levels.
    std::vector<std::string> analog_pins;
    // Outputs inside the part (SN-224: a digital potentiometer's wiper): device
    // pins with no pin on the symbol, whose drive the template reads through
    // {ext:<name>_g}, the output's strength divided by output_resistance.
    std::vector<std::string> internal_pins;
    // Memory chips (SN-208): their size in bytes and the File parameter with the
    // contents they start with (empty: erased). A parallel memory (SN-212) takes
    // its size from a parameter ("2K" words of a byte) and starts with `memory_fill`.
    std::size_t memory_size = 0;
    std::string memory_size_parameter;
    std::size_t memory_default = 0; // bytes at the size parameter's default
    std::uint8_t memory_fill = 0xFF;
    std::string contents_parameter;

    // The size of a placed memory with these parameters (0 for other chips).
    std::size_t memory_bytes(const std::map<std::string, std::string>& parameters) const;
};

// Interactive control (INT-02). The netlist generator creates an external
// voltage source on node "{control}" whose value is the control state:
// 0/1 for toggle and button, 0..1 for a potentiometer position, and for a
// keypad the number of the key held (1 for the first of `keys`), 0 for none.
struct Interactive {
    enum class Kind { Toggle, Button, Potentiometer, Keypad };
    Kind kind = Kind::Toggle;
    std::string parameter; // parameter holding the initial state ("closed", "position")
    // Potentiometer (UI-18): the wiper arrow's tip moves along this track, from
    // the first point (0 %) to the second (100 %), and joins the wiper pin.
    struct Wiper {
        model::PointF start;
        model::PointF end;
        std::string pin;
    };
    std::optional<Wiper> wiper;
    // Keypad (SN-210): the keys' rectangles in symbol coordinates (x, y, width, height).
    std::vector<std::array<double, 4>> keys;
    // Potentiometer on a quantity (SN-221: a sensor's temperature or light): a
    // parameter with a unit other than % and a min and max goes from min (0) to
    // max (1), on a logarithmic scale when `logarithmic` (light from 0.1 to 100k lux).
    bool logarithmic = false;
};

// The control value (0..1) of a potentiometer whose parameter has `value`.
double potentiometer_control(const Parameter& parameter, bool logarithmic, double value);

// Value shown on the symbol while running (INT-03).
struct Readout {
    enum class Kind { Voltage, Current, Node };
    Kind kind = Kind::Voltage;
    std::vector<std::string> pins; // voltage: + and - pin
    std::string element;           // current: template element name, e.g. "V_{ref}"; node: internal node, "w_{ref}"
    double scale = 1.0;            // node: shown value = scale x node voltage
    std::string unit;              // node: unit of the shown value
    // Current readouts may make the symbol glow: full brightness at glow_full
    // amperes, or at the value of parameter glow_full_parameter; the color is
    // glow_color ("#rrggbb") or the value of parameter glow_color_parameter.
    double glow_full = 0.0;
    std::string glow_full_parameter;
    std::string glow_color;
    std::string glow_color_parameter;
    bool glows() const { return glow_full > 0 || !glow_full_parameter.empty(); }
    // Current readouts may switch the symbol's state graphics instead (a relay's
    // contact): state "1" above `switch_on` amperes, back to "0" below
    // `switch_off`; both are expressions of the parameters ("0.42 * 0.36 / Vcoil").
    std::string switch_on;
    std::string switch_off;
    bool switches() const { return !switch_on.empty(); }
};

// Shapes of the symbol that light with their own current (SN-202: the segments
// of a 7-segment display). Each graphic, by its index in `graphics`, is filled
// with the color at the brightness of the current through `element`.
struct Lights {
    struct Light {
        std::string element; // measuring source, "V_{ref}_sa"
        int graphic = 0;
    };
    std::vector<Light> lights;
    double full = 0.01;          // amperes for full brightness
    std::string color;           // "#rrggbb" or an LED color name
    std::string color_parameter; // or the value of this parameter
};

struct ComponentDefinition {
    std::string type;
    std::string name;
    std::string category;
    std::string prefix;
    std::string description;
    std::vector<Graphic> graphics;
    // Extra graphics drawn for a control state ("0", "1"); toggles and buttons.
    std::map<std::string, std::vector<Graphic>> state_graphics;
    std::vector<Pin> pins;
    model::Point designator_offset{2, -1};
    model::Point value_offset{2, 1};
    bool show_designator = true;
    std::string value_parameter; // shown next to the symbol
    std::vector<Parameter> parameters;
    std::vector<std::string> spice;  // template lines
    std::vector<std::string> models; // .model lines, emitted once per netlist
    // Kept in each placed component (`Component::models`): a vendor
    // subcircuit part (SN-214) brings its subcircuit text this way.
    std::map<std::string, std::string> initial_models;
    std::string ties_net;            // non-empty: all pins belong to this net ("0" for ground)
    std::string ties_net_parameter;  // non-empty: all pins belong to the net named by this parameter (block ports)
    std::optional<Interactive> interactive;
    std::optional<Readout> readout;
    std::optional<Lights> lights;
    std::optional<McuBoard> mcu;  // unconnected pins of a board are not reported
    std::optional<DigitalDevice> device;
    std::optional<Screen> screen;
    // A buzzer (SN-228): its node readout is the frequency it sounds at (0: silent).
    bool sound = false;
    std::filesystem::path source; // file it was loaded from
    // A user block (PRJ-06): the circuit behind the symbol, whose port parts are
    // the pins. It has no template; the netlist puts the circuit in its place.
    std::shared_ptr<const model::Schematic> block;

    const Pin* pin(const std::string& pin_name) const;
    const Parameter* parameter(const std::string& parameter_name) const;
    // Parameter values of a new instance.
    std::map<std::string, std::string> default_parameters() const;
};

// Whether a model fits a model parameter's kinds (see Parameter::model_kinds).
bool model_fits(const Parameter& parameter, const SpiceModel& model);

struct LoadMessage {
    std::filesystem::path file;
    std::string text;
};

class ComponentLibrary {
public:
    // Loads every *.json below `directory` and the SPICE models of every *.lib
    // file there. Invalid files are reported and skipped; the rest of the
    // library stays usable.
    std::vector<LoadMessage> load_directory(const std::filesystem::path& directory);
    // Parses one definition; returns an error message on failure.
    std::optional<std::string> add_from_json(const std::string& text, const std::filesystem::path& source = {});
    // Adds a definition made in code (a block's), replacing one of the same type.
    void add_definition(ComponentDefinition definition);

    const ComponentDefinition* find(const std::string& type) const;
    const std::vector<ComponentDefinition>& definitions() const { return definitions_; }
    // Categories in display order (fixed order first, then alphabetical).
    std::vector<std::string> categories() const;
    bool empty() const { return definitions_.empty(); }

    // Built-in SPICE models (PRJ-07), in file order.
    const std::vector<SpiceModel>& models() const { return models_; }
    const SpiceModel* model(const std::string& type, const std::string& name) const;
    void add_models(const std::vector<SpiceModel>& models);

private:
    std::vector<ComponentDefinition> definitions_;
    std::vector<SpiceModel> models_;
};

// Schematic position of a pin of a placed component.
model::Point pin_position(const ComponentDefinition& definition, const Pin& pin, const model::Component& component);

// Library folder candidates for an application in `application_dir`.
std::vector<std::filesystem::path> library_candidates(const std::filesystem::path& application_dir);

} // namespace simnodus::library
