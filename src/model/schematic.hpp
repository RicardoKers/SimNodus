// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Schematic document (PRJ-01): components, wires, net labels, probes, text
// notes, buses, splitters, simulation settings and the user's blocks it uses. Every element has a stable 64-bit ID that survives
// save/load, rename and undo; IDs are never reused within a document.
// Labels such as designators are presentation, not identity.
#pragma once

#include "geometry.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace simnodus::model {

using Id = std::uint64_t;
constexpr Id no_id = 0;

struct Component {
    Id id = no_id;
    std::string type;       // library type key, e.g. "resistor"
    std::string designator; // R1, C2, ...
    Point position;
    Orientation orientation;
    std::map<std::string, std::string> parameters; // text as typed by the user
    // Positions of the designator and value texts relative to `position`;
    // empty means the library default.
    std::optional<Point> designator_offset;
    std::optional<Point> value_offset;
    // Model parameter -> `.model` statement of a model that is not built in
    // (from the user's models folder, PRJ-07): the project keeps it, so it
    // opens the same on another computer.
    std::map<std::string, std::string> models;

    friend bool operator==(const Component&, const Component&) = default;
};

struct Wire {
    Id id = no_id;
    std::vector<Point> points; // orthogonal polyline, at least two points

    friend bool operator==(const Wire&, const Wire&) = default;
};

struct NetLabel {
    Id id = no_id;
    Point position;
    std::string name;

    friend bool operator==(const NetLabel&, const NetLabel&) = default;
};

// Free text on the schematic (UI-17): instructions or explanations for the
// reader. Presentation only, never part of the netlist.
struct TextNote {
    Id id = no_id;
    Point position; // top-left corner of the text
    std::string text; // may span several lines

    friend bool operator==(const TextNote&, const TextNote&) = default;
};

// Bus (UI-19): a thick line standing for several wires, for the reader only.
// Signals join through splitters, by name: bit k of bus "D" is net "Dk".
struct Bus {
    Id id = no_id;
    std::vector<Point> points; // orthogonal polyline, at least two points
    std::string name;          // "D"

    friend bool operator==(const Bus&, const Bus&) = default;
};

// Splitter (UI-19): joins a bus to single wires, like hneemann's Digital. Its
// bus point is at `position`; bit pins `first` .. `first + count - 1` follow at
// local (2, 0), (2, 2), ... and each one acts as a net label "<bus><bit>".
struct Splitter {
    Id id = no_id;
    Point position;
    Orientation orientation;
    std::string bus; // bus name, "D"
    int first = 0;
    int count = 8;

    friend bool operator==(const Splitter&, const Splitter&) = default;
};

// Schematic points of a splitter's bit pins, in bit order.
std::vector<Point> splitter_pins(const Splitter& splitter);
// Net name of bit pin `k` (0 = the first pin): "D0", "D1", ...
std::string splitter_net(const Splitter& splitter, int k);

// A connection point of the schematic: a component pin, or a point on a wire.
struct Anchor {
    Id item = no_id;
    std::string pin; // component pin name; empty for a wire
    Point point;     // for a wire: the clicked point on it

    friend bool operator==(const Anchor&, const Anchor&) = default;
};

enum class ProbeKind { Voltage, Current };

struct Probe {
    Id id = no_id;
    ProbeKind kind = ProbeKind::Voltage;
    Anchor target;                   // voltage: the net; current: component + pin (current into it)
    std::optional<Anchor> reference; // voltage only; ground when empty
    std::string color;               // "#rrggbb"; trace color in the analyzer

    friend bool operator==(const Probe&, const Probe&) = default;
};

enum class SimulationMode { Interactive, Transient };

struct SimulationSettings {
    SimulationMode mode = SimulationMode::Interactive;
    double speed = 1.0;        // interactive: simulated seconds per wall second; 0 = maximum
    double max_step = 1e-4;    // seconds (ADR 0080: interactive default 100 us)
    double segment = 1.0;      // interactive segment length, seconds
    double transient_stop = 10e-3;
    double transient_max_step = 1e-6;
    // Microcontroller coupling quantum Q (MCU-02, ADR 0085): the firmware runs at
    // most Q ahead of the circuit, so circuit inputs reach it within about Q.
    double mcu_quantum = 1e-3; // inputs reach the firmware within two quanta (ADR 0085)
    // Interactive runs with the own emulator: a time point at every firmware edge
    // of a wired pin (ADR 0093), for square waveforms at a slower run. Off: each
    // step gets the pin's average drive (ADR 0091).
    bool exact_edges = false;

    friend bool operator==(const SimulationSettings&, const SimulationSettings&) = default;
};

class Schematic;

// A block drawn by the user (PRJ-06, ADR 0098): a schematic of its own whose
// port parts become the pins of a rectangular symbol. The project keeps a copy,
// so it opens the same on another computer; `source` is the file it came from.
// Components use it with the type "block:<name>".
struct Block {
    std::string name;   // the block file's name without extension, "Amplifier"
    std::string source; // the block's .snx, relative to the project folder when possible
    std::shared_ptr<const Schematic> schematic; // never null; shared between undo states
};
bool operator==(const Block& a, const Block& b);

// Component type of a block: "block:Amplifier"; and back (empty if not a block).
std::string block_type(const std::string& name);
std::string block_name(const std::string& type);

using Element = std::variant<Component, Wire, NetLabel, Probe, TextNote, Bus, Splitter>;

Id element_id(const Element& element);

class Schematic {
public:
    // Reserves a new, never used ID.
    Id allocate_id() { return next_id_++; }
    Id next_id() const { return next_id_; }
    // Used by file loading; never lowers the counter below a stored ID.
    void set_next_id(Id next);

    const std::vector<Component>& components() const { return components_; }
    const std::vector<Wire>& wires() const { return wires_; }
    const std::vector<NetLabel>& labels() const { return labels_; }
    const std::vector<Probe>& probes() const { return probes_; }
    const std::vector<TextNote>& notes() const { return notes_; }
    const std::vector<Bus>& buses() const { return buses_; }
    const std::vector<Splitter>& splitters() const { return splitters_; }
    const SimulationSettings& settings() const { return settings_; }
    void set_settings(const SimulationSettings& settings) { settings_ = settings; }
    // Blocks by name (PRJ-06).
    const std::map<std::string, Block>& blocks() const { return blocks_; }
    const Block* block(const std::string& name) const;
    void set_blocks(std::map<std::string, Block> blocks) { blocks_ = std::move(blocks); }

    const Component* component(Id id) const;
    const Wire* wire(Id id) const;
    const NetLabel* label(Id id) const;
    const Probe* probe(Id id) const;
    const TextNote* note(Id id) const;
    const Bus* bus(Id id) const;
    const Splitter* splitter(Id id) const;
    const Component* component_by_designator(const std::string& designator) const;

    std::optional<Element> get(Id id) const;
    // Inserts or replaces the element with the same ID (kept ordered by ID).
    void put(const Element& element);
    void erase(Id id);
    bool contains(Id id) const { return get(id).has_value(); }
    bool empty() const;

    friend bool operator==(const Schematic&, const Schematic&) = default;

private:
    Id next_id_ = 1;
    std::vector<Component> components_;
    std::vector<Wire> wires_;
    std::vector<NetLabel> labels_;
    std::vector<Probe> probes_;
    std::vector<TextNote> notes_;
    std::vector<Bus> buses_;
    std::vector<Splitter> splitters_;
    SimulationSettings settings_;
    std::map<std::string, Block> blocks_;
};

// Next free designator for a prefix: "R" -> "R3" when R1 and R2 exist.
std::string next_designator(const Schematic& schematic, const std::string& prefix);

} // namespace simnodus::model
