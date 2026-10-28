// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "device_bus.hpp"


#include <algorithm>

namespace simnodus::mcu {

namespace {

// More rounds than this to settle the nets means the devices oscillate.
constexpr int max_rounds = 10000;

bool same(const devices::Output& a, const devices::Output& b)
{
    return a.driven == b.driven
        && (!a.driven || (a.high == b.high && a.open_drain == b.open_drain && a.strength == b.strength))
        && (a.driven || a.pull_up == b.pull_up);
}

} // namespace

DeviceBus::~DeviceBus()
{
    if (machine_) {
        machine_->remove_timed(this);
    }
}

bool DeviceBus::init(McuMachine& machine, const std::vector<DeviceRequest>& devices,
    const std::vector<DigitalNet>& nets, std::string& error)
{
    machine_ = &machine;
    machine.add_timed(this);
    for (const auto& request : devices) {
        std::unique_ptr<devices::Device> device;
        if (request.supply > 0) {
            device = devices::make_device(request.kind, request.pins, request.parameters, error);
            if (!device) {
                error = request.designator + ": " + error;
                return false;
            }
            if (!request.memory.empty()) {
                device->load_memory(request.memory); // its contents file (SN-208)
            }
        }
        devices_.push_back(std::move(device));
        designators_.push_back(request.designator);
        pin_names_.push_back(request.pins);
        pin_net_.emplace_back(request.pins.size(), -1);
        outputs_.emplace_back(request.pins.size());
    }
    for (const auto& n : nets) {
        Net net;
        net.name = n.name;
        net.pull = n.pull;
        net.circuit = n.circuit;
        for (const auto& board_pin : n.board_pins) {
            if (const int pin = machine.pin_id(board_pin); pin >= 0) {
                net.board.push_back(pin);
                board_net_[pin] = nets_.size();
            }
        }
        for (const auto& [device, pin] : n.device_pins) {
            if (device >= devices_.size()) {
                continue;
            }
            const auto& names = pin_names_[device];
            const auto it = std::find(names.begin(), names.end(), pin);
            if (it != names.end()) {
                const auto index = static_cast<std::size_t>(it - names.begin());
                net.pins.push_back({device, index});
                pin_net_[device][index] = static_cast<int>(nets_.size());
            }
        }
        nets_.push_back(std::move(net));
    }
    queued_.assign(nets_.size(), false);
    // Tied pins (ground, a supply, open) keep their level for the whole run.
    for (std::size_t d = 0; d < devices.size(); ++d) {
        if (!devices_[d]) {
            continue;
        }
        for (const auto& [pin, level] : devices[d].tied) {
            const auto& names = pin_names_[d];
            const auto it = std::find(names.begin(), names.end(), pin);
            if (it != names.end()) {
                devices_[d]->initial_input(static_cast<std::size_t>(it - names.begin()), level);
            }
        }
    }
    return true;
}

void DeviceBus::start(std::uint64_t time_ns)
{
    // Starting levels without edges: nets from the board and the pulls only,
    // then the devices' first outputs, then everything once more as usual.
    for (std::size_t n = 0; n < nets_.size(); ++n) {
        resolve(n, time_ns);
    }
    for (std::size_t d = 0; d < devices_.size(); ++d) {
        if (!devices_[d]) {
            continue;
        }
        for (std::size_t p = 0; p < pin_net_[d].size(); ++p) {
            if (pin_net_[d][p] >= 0) {
                devices_[d]->initial_input(p, nets_[static_cast<std::size_t>(pin_net_[d][p])].level);
            }
        }
    }
    for (std::size_t d = 0; d < devices_.size(); ++d) {
        collect_outputs(d, time_ns);
    }
    settle(time_ns);
}

void DeviceBus::set_analog_source(
    const std::function<double(std::size_t device, std::size_t pin, std::uint64_t time_ns)>& source)
{
    for (std::size_t d = 0; d < devices_.size(); ++d) {
        if (devices_[d]) {
            devices_[d]->set_analog_source([source, d](std::size_t pin, std::uint64_t time_ns) { return source(d, pin, time_ns); });
        }
    }
}

void DeviceBus::set_parameter(std::size_t device, const std::string& name, const std::string& value,
    std::uint64_t time_ns)
{
    if (device >= devices_.size() || !devices_[device]) {
        return;
    }
    devices_[device]->set_parameter(name, value);
    collect_outputs(device, time_ns); // an LM75's OS output may change
    if (!busy_) {
        settle(time_ns);
    }
}

void DeviceBus::set_circuit_level(std::size_t device, std::size_t pin, bool level, std::uint64_t time_ns)
{
    if (device >= pin_net_.size() || pin >= pin_net_[device].size() || pin_net_[device][pin] < 0) {
        return;
    }
    const auto net = static_cast<std::size_t>(pin_net_[device][pin]);
    nets_[net].circuit_known = true;
    nets_[net].circuit_level = level;
    mark(net);
    if (!busy_) {
        settle(time_ns);
    }
}

void DeviceBus::board_pin_changed(int pin, std::uint64_t time_ns)
{
    const auto it = board_net_.find(pin);
    if (it == board_net_.end()) {
        return;
    }
    mark(it->second);
    if (!busy_) {
        settle(time_ns);
    }
}

void DeviceBus::mark(std::size_t net)
{
    if (!queued_[net]) {
        queued_[net] = true;
        dirty_.push_back(net);
    }
}

void DeviceBus::settle(std::uint64_t time_ns)
{
    // An input may give a device an earlier event (a 1-Wire presence pulse 30 us
    // after a reset, SN-225): the machine must stop for it, not at its old target.
    const std::uint64_t before = next_event_ns();
    busy_ = true;
    int rounds = 0;
    while (!dirty_.empty()) {
        const std::size_t net = dirty_.front();
        dirty_.pop_front();
        queued_[net] = false;
        if (++rounds > max_rounds) {
            messages_.push_back("The devices on net " + nets_[net].name
                + " keep changing each other without settling; SimNodus stops following them for now.");
            dirty_.clear();
            std::fill(queued_.begin(), queued_.end(), false);
            break;
        }
        resolve(net, time_ns);
    }
    busy_ = false;
    if (machine_ && next_event_ns() < before) {
        machine_->timed_schedule_changed();
    }
}

void DeviceBus::resolve(std::size_t index, std::uint64_t time_ns)
{
    Net& net = nets_[index];
    bool strong_low = false;
    bool strong_high = false;
    bool weak_low = false;
    bool weak_high = false;
    for (const int pin : net.board) {
        const PinState s = machine_->pin_state(pin);
        if (s.mode == PinMode::Output || s.mode == PinMode::Alternate) {
            if (!s.level) {
                strong_low = true;
            } else if (!s.open_drain) {
                strong_high = true;
            }
        } else if (s.pull != Pull::None) {
            (s.pull == Pull::Up ? weak_high : weak_low) = true; // internal pull-up or pull-down
        }
    }
    for (const auto& p : net.pins) {
        const auto& out = outputs_[p.device][p.pin];
        if (!out.driven) {
            weak_high = weak_high || out.pull_up;
            continue;
        }
        if (!out.high) {
            strong_low = true;
        } else if (!out.open_drain) {
            strong_high = true;
        }
    }
    if (strong_low && strong_high && !net.warned_short) {
        net.warned_short = true;
        messages_.push_back("Net " + net.name + " is driven high and low at the same time (a short circuit on a "
            "real board); SimNodus takes it as low.");
    }
    bool level = net.level;
    if (strong_low) {
        level = false;
    } else if (strong_high) {
        level = true;
    } else if (net.circuit && net.circuit_known) {
        level = net.circuit_level; // a button or a resistor in the circuit
    } else if (weak_high != weak_low) {
        level = weak_high;
    } else if (net.pull != 0) {
        level = net.pull > 0;
    } // else floating: it keeps its last level
    const bool first = !net.known;
    if (!first && level == net.level) {
        return;
    }
    net.known = true;
    net.level = level;
    for (const int pin : net.board) {
        machine_->set_pin_input(pin, level);
    }
    if (first) {
        return; // start(): the devices get initial inputs instead of edges
    }
    for (const auto& p : net.pins) {
        if (devices_[p.device]) {
            devices_[p.device]->set_input(p.pin, level, time_ns);
            collect_outputs(p.device, time_ns);
        }
    }
}

void DeviceBus::collect_outputs(std::size_t device, std::uint64_t time_ns)
{
    if (!devices_[device]) {
        return;
    }
    for (std::size_t p = 0; p < outputs_[device].size(); ++p) {
        const devices::Output now = devices_[device]->output(p);
        if (same(now, outputs_[device][p])) {
            continue;
        }
        outputs_[device][p] = now;
        changes_.push_back({device, p, now, time_ns});
        if (pin_net_[device][p] >= 0) {
            mark(static_cast<std::size_t>(pin_net_[device][p]));
        }
    }
}

void DeviceBus::advance(std::uint64_t time_ns)
{
    for (;;) {
        std::size_t first = devices_.size();
        std::uint64_t at = time_ns;
        for (std::size_t d = 0; d < devices_.size(); ++d) {
            if (devices_[d] && devices_[d]->next_event_ns() <= at) {
                at = devices_[d]->next_event_ns();
                first = d;
            }
        }
        if (first == devices_.size()) {
            return;
        }
        devices_[first]->advance(at);
        collect_outputs(first, at);
        settle(at);
    }
}

std::uint64_t DeviceBus::next_event_ns() const
{
    std::uint64_t next = devices::no_event;
    for (const auto& device : devices_) {
        if (device) {
            next = std::min(next, device->next_event_ns());
        }
    }
    return next;
}

std::uint64_t DeviceBus::next_event() const
{
    const std::uint64_t ns = next_event_ns();
    return ns == devices::no_event ? emu::no_event : machine_->cycles_at(ns);
}

void DeviceBus::on_time(std::uint64_t cycle)
{
    advance(machine_->ns_at(cycle));
}

std::vector<std::string> DeviceBus::take_messages()
{
    for (std::size_t d = 0; d < devices_.size(); ++d) {
        if (devices_[d]) {
            for (auto& text : devices_[d]->take_messages()) {
                messages_.push_back(designators_[d] + ": " + text);
            }
        }
    }
    return std::exchange(messages_, {});
}

} // namespace simnodus::mcu
