// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "sim/engine_protocol.hpp"

#include "io/project_file.hpp"

namespace simnodus::sim::protocol {

namespace {

std::string utf8(const std::filesystem::path& path)
{
    const auto s = path.u8string();
    return {s.begin(), s.end()};
}

std::filesystem::path from_utf8(const std::string& s)
{
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

void write_strings(Writer& w, const std::vector<std::string>& list)
{
    w.size(list.size());
    for (const auto& s : list) {
        w.str(s);
    }
}

std::vector<std::string> read_strings(Reader& r)
{
    std::vector<std::string> list(r.size(4));
    for (auto& s : list) {
        s = r.str();
    }
    return list;
}

void write_pin_model(Writer& w, const mcu::PinModel& p)
{
    w.f64(p.supply);
    w.f64(p.output_resistance);
    w.f64(p.pull_resistance);
    w.f64(p.input_high);
    w.f64(p.input_low);
}

mcu::PinModel read_pin_model(Reader& r)
{
    mcu::PinModel p;
    p.supply = r.f64();
    p.output_resistance = r.f64();
    p.pull_resistance = r.f64();
    p.input_high = r.f64();
    p.input_low = r.f64();
    return p;
}

void write_mcu_request(Writer& w, const McuRequest& m)
{
    w.size(m.uart_pins.size());
    for (const auto& [uart, pins] : m.uart_pins) {
        w.str(uart);
        w.str(pins.first);
        w.str(pins.second);
    }
    w.u64(m.component);
    const auto& o = m.options;
    w.str(o.chip);
    w.str(utf8(o.firmware));
    write_strings(w, o.pins);
    write_strings(w, o.uarts);
    write_pin_model(w, o.pin_model);
    w.u64(o.chunk_us);
    w.u64(o.analog_lag_ns);
    write_strings(w, {o.uart_rx_byte_links.begin(), o.uart_rx_byte_links.end()});
    w.u64(o.uart_rx_min_bit_ns);
    w.i32(o.gdb_port);
    w.boolean(o.chips_only);
    w.boolean(o.cortex_m3_timing);
    w.size(o.uart_links.size());
    for (const auto& [uart, link] : o.uart_links) {
        w.str(uart);
        w.str(link.spec);
        w.str(link.circuit);
    }
    w.size(o.devices.size());
    for (const auto& d : o.devices) {
        w.u64(d.component);
        w.str(d.designator);
        w.str(d.kind);
        w.size(d.parameters.size());
        for (const auto& [name, value] : d.parameters) {
            w.str(name);
            w.str(value);
        }
        write_strings(w, d.pins);
        w.f64(d.supply);
        write_pin_model(w, d.pin_model);
        w.size(d.tied.size());
        for (const auto& [pin, level] : d.tied) {
            w.str(pin);
            w.boolean(level);
        }
        w.str(std::string(d.memory.begin(), d.memory.end()));
    }
    w.size(o.nets.size());
    for (const auto& n : o.nets) {
        w.str(n.name);
        write_strings(w, n.board_pins);
        w.size(n.device_pins.size());
        for (const auto& [device, pin] : n.device_pins) {
            w.u64(device);
            w.str(pin);
        }
        w.i32(n.pull);
        w.boolean(n.circuit);
    }
    w.str(m.ground_pin);
}

McuRequest read_mcu_request(Reader& r)
{
    McuRequest m;
    for (std::size_t n = r.size(12); n > 0; --n) {
        auto uart = r.str();
        auto tx = r.str();
        auto rx = r.str();
        m.uart_pins[uart] = {tx, rx};
    }
    m.component = r.u64();
    auto& o = m.options;
    o.chip = r.str();
    o.firmware = from_utf8(r.str());
    o.pins = read_strings(r);
    o.uarts = read_strings(r);
    o.pin_model = read_pin_model(r);
    o.chunk_us = r.u64();
    o.analog_lag_ns = r.u64();
    for (auto& s : read_strings(r)) {
        o.uart_rx_byte_links.insert(std::move(s));
    }
    o.uart_rx_min_bit_ns = r.u64();
    o.gdb_port = r.i32();
    o.chips_only = r.boolean();
    o.cortex_m3_timing = r.boolean();
    for (std::size_t n = r.size(8); n > 0; --n) {
        auto uart = r.str();
        auto spec = r.str();
        o.uart_links[uart] = {spec, r.str()};
    }
    for (std::size_t n = r.size(8); n > 0 && r.ok(); --n) {
        mcu::DeviceRequest d;
        d.component = r.u64();
        d.designator = r.str();
        d.kind = r.str();
        for (std::size_t p = r.size(8); p > 0 && r.ok(); --p) {
            auto name = r.str();
            d.parameters[name] = r.str();
        }
        d.pins = read_strings(r);
        d.supply = r.f64();
        d.pin_model = read_pin_model(r);
        for (std::size_t p = r.size(5); p > 0 && r.ok(); --p) {
            auto pin = r.str();
            d.tied[pin] = r.boolean();
        }
        const auto memory = r.str();
        d.memory.assign(memory.begin(), memory.end());
        o.devices.push_back(std::move(d));
    }
    for (std::size_t n = r.size(16); n > 0 && r.ok(); --n) {
        mcu::DigitalNet net;
        net.name = r.str();
        net.board_pins = read_strings(r);
        for (std::size_t p = r.size(12); p > 0 && r.ok(); --p) {
            const auto device = static_cast<std::size_t>(r.u64());
            net.device_pins.emplace_back(device, r.str());
        }
        net.pull = r.i32();
        net.circuit = r.boolean();
        o.nets.push_back(std::move(net));
    }
    m.ground_pin = r.str();
    return m;
}

} // namespace

std::string frame(const std::string& payload)
{
    const auto n = static_cast<std::uint32_t>(payload.size());
    std::string result(sizeof n, '\0');
    std::memcpy(result.data(), &n, sizeof n);
    result += payload;
    return result;
}

std::optional<std::string> FrameReader::next()
{
    if (invalid_ || buffer_.size() - pos_ < sizeof(std::uint32_t)) {
        return std::nullopt;
    }
    std::uint32_t n = 0;
    std::memcpy(&n, buffer_.data() + pos_, sizeof n);
    if (n > max_frame) {
        invalid_ = true;
        return std::nullopt;
    }
    if (buffer_.size() - pos_ - sizeof n < n) {
        return std::nullopt;
    }
    std::string payload = buffer_.substr(pos_ + sizeof n, n);
    pos_ += sizeof n + n;
    // Drop consumed bytes now and then, not after every frame.
    if (pos_ > (1u << 16) && pos_ * 2 > buffer_.size()) {
        buffer_.erase(0, pos_);
        pos_ = 0;
    }
    return payload;
}

void write_status(Writer& w, const RunStatus& s)
{
    w.u8(static_cast<std::uint8_t>(s.state));
    w.u8(static_cast<std::uint8_t>(s.mode));
    w.f64(s.sim_time);
    w.f64(s.achieved_factor);
    w.f64(s.speed);
    w.i32(s.segments);
    w.f64(s.progress);
    w.boolean(s.starting_mcu);
    w.boolean(s.debugger_waiting);
    w.boolean(s.debugger_halted);
    w.i32(s.gdb_port);
    w.str(s.message);
    write_strings(w, s.engine_output);
}

RunStatus read_status(Reader& r)
{
    RunStatus s;
    s.state = static_cast<RunState>(r.u8());
    s.mode = static_cast<model::SimulationMode>(r.u8());
    s.sim_time = r.f64();
    s.achieved_factor = r.f64();
    s.speed = r.f64();
    s.segments = r.i32();
    s.progress = r.f64();
    s.starting_mcu = r.boolean();
    s.debugger_waiting = r.boolean();
    s.debugger_halted = r.boolean();
    s.gdb_port = r.i32();
    s.message = r.str();
    s.engine_output = read_strings(r);
    return s;
}

void write_mcu_status(Writer& w, const mcu::McuStatus& s)
{
    w.u8(static_cast<std::uint8_t>(s.state));
    w.u64(s.time_ns);
    w.f64(s.startup_seconds);
    w.i32(s.resets);
    w.u64(s.max_input_delay_ns);
    w.u64(s.late_analog_samples);
    w.i32(s.gdb_port);
    w.boolean(s.debug_waiting);
    w.boolean(s.debug_halted);
    w.i32(s.debug_resumes);
    w.str(s.message);
}

mcu::McuStatus read_mcu_status(Reader& r)
{
    mcu::McuStatus s;
    s.state = static_cast<mcu::McuState>(r.u8());
    s.time_ns = r.u64();
    s.startup_seconds = r.f64();
    s.resets = r.i32();
    s.max_input_delay_ns = r.u64();
    s.late_analog_samples = r.u64();
    s.gdb_port = r.i32();
    s.debug_waiting = r.boolean();
    s.debug_halted = r.boolean();
    s.debug_resumes = r.i32();
    s.message = r.str();
    return s;
}

void write_pin_state(Writer& w, const mcu::PinState& s)
{
    w.boolean(s.level);
    w.u8(static_cast<std::uint8_t>(s.mode));
    w.boolean(s.open_drain);
    w.u8(static_cast<std::uint8_t>(s.pull));
}

mcu::PinState read_pin_state(Reader& r)
{
    mcu::PinState s;
    s.level = r.boolean();
    s.mode = static_cast<mcu::PinMode>(r.u8());
    s.open_drain = r.boolean();
    s.pull = static_cast<mcu::Pull>(r.u8());
    return s;
}

void write_samples(Writer& w, const std::vector<Sample>& samples)
{
    w.size(samples.size());
    for (const auto& s : samples) {
        w.f64(s.time);
        w.f64(s.value);
    }
}

std::vector<Sample> read_samples(Reader& r)
{
    std::vector<Sample> samples(r.size(16));
    for (auto& s : samples) {
        s.time = r.f64();
        s.value = r.f64();
    }
    return samples;
}

void write_transient(Writer& w, const TransientResult& t)
{
    w.size(t.time.size());
    for (double v : t.time) {
        w.f64(v);
    }
    w.size(t.vectors.size());
    for (const auto& [name, values] : t.vectors) {
        w.str(name);
        w.size(values.size());
        for (double v : values) {
            w.f64(v);
        }
    }
    w.boolean(t.truncated);
}

TransientResult read_transient(Reader& r)
{
    TransientResult t;
    t.time.resize(r.size(8));
    for (double& v : t.time) {
        v = r.f64();
    }
    for (std::size_t n = r.size(8); n > 0 && r.ok(); --n) {
        auto name = r.str();
        std::vector<double> values(r.size(8));
        for (double& v : values) {
            v = r.f64();
        }
        t.vectors[name] = std::move(values);
    }
    t.truncated = r.boolean();
    return t;
}

std::string start_message(const RunRequest& request, const EngineSetup& setup)
{
    Writer w(Type::Start);
    w.u32(version);
    w.str(utf8(setup.library_dir));
    w.str(utf8(setup.application_dir));
    w.boolean(setup.ngspice.has_value());
    w.str(setup.ngspice ? utf8(setup.ngspice->library) : std::string());
    w.str(setup.ngspice ? utf8(setup.ngspice->scripts_dir) : std::string());
    w.str(io::to_json_text(request.schematic));
    write_strings(w, request.vectors);
    w.size(request.current_probes.size());
    for (const auto& [id, pin] : request.current_probes) {
        w.u64(id);
        w.str(pin);
    }
    w.u64(request.ring_capacity);
    w.u64(request.transient_limit);
    w.size(request.mcus.size());
    for (const auto& m : request.mcus) {
        write_mcu_request(w, m);
    }
    w.u64(request.mcu_lead_us);
    w.boolean(request.debugging);
    return w.payload();
}

bool read_start(Reader& r, RunRequest& request, EngineSetup& setup, std::string& error)
{
    if (r.u32() != version) {
        error = "The simulation engine belongs to another SimNodus version; reinstall SimNodus.";
        return false;
    }
    setup.library_dir = from_utf8(r.str());
    setup.application_dir = from_utf8(r.str());
    const bool explicit_ngspice = r.boolean();
    const auto ngspice_library = from_utf8(r.str());
    const auto ngspice_scripts = from_utf8(r.str());
    if (explicit_ngspice) {
        setup.ngspice = NgspiceLocation{ngspice_library, ngspice_scripts};
    }
    auto loaded = io::from_json_text(r.str());
    request.vectors = read_strings(r);
    for (std::size_t n = r.size(12); n > 0; --n) {
        const model::Id id = r.u64();
        request.current_probes.emplace_back(id, r.str());
    }
    request.ring_capacity = r.u64();
    request.transient_limit = r.u64();
    for (std::size_t n = r.size(8); n > 0 && r.ok(); --n) {
        request.mcus.push_back(read_mcu_request(r));
    }
    request.mcu_lead_us = r.u64();
    request.debugging = r.boolean();
    if (!r.ok() || !loaded.schematic) {
        error = loaded.errors.empty() ? "The simulation engine received an incomplete request."
                                      : "The simulation engine could not read the circuit: " + loaded.errors.front();
        return false;
    }
    request.schematic = std::move(*loaded.schematic);
    return true;
}

} // namespace simnodus::sim::protocol
