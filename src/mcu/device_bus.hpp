// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The nets between a board and its digital devices (SN-203, ADR 0102),
// resolved in step with the firmware: when a board pin or a device output
// changes, the net's level follows at the same instant from its drivers
// (push-pull outputs, open-drain outputs pulling low), else from a weak pull
// (the board pin's internal pull-up or pull-down), else from a resistor to a
// supply or to ground found in the circuit, else it keeps its last level. The
// board's inputs and the devices see the new level at once, so a device can
// answer within the same bus cycle (an I2C acknowledge, a MISO bit). The
// circuit gets the same edges through the drivers of the device pins.
#pragma once

#include "devices/device.hpp"
#include "emu/timed_device.hpp"
#include "mcu_runner.hpp"

#include <cstdint>
#include <functional>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace simnodus::emu {
class Stm32F103;
}

namespace simnodus::mcu {

// The devices' own events (scanning, write cycles) run inside the machine's
// run at their exact times, as a timed device of the machine.
class DeviceBus : public emu::TimedDevice {
public:
    ~DeviceBus() override;
    struct Change {
        std::size_t device = 0;
        std::size_t pin = 0; // index in the device's signal pins
        devices::Output output;
        std::uint64_t time_ns = 0;
    };

    // Creates the devices; false with `error` for an unknown kind.
    bool init(emu::Stm32F103& machine, const std::vector<DeviceRequest>& devices,
        const std::vector<DigitalNet>& nets, std::string& error);
    bool empty() const { return devices_.empty(); }

    // Resolves every net once and gives the devices their starting inputs.
    void start(std::uint64_t time_ns);
    // From the machine's pin hook.
    void board_pin_changed(int port, int pin, std::uint64_t time_ns);
    // Lets the devices change by themselves up to `time_ns`.
    void advance(std::uint64_t time_ns);
    std::uint64_t next_event_ns() const;
    // emu::TimedDevice: the next device event in machine cycles.
    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    // A live parameter of a chip (SN-207): the chip takes it now.
    void set_parameter(std::size_t device, const std::string& name, const std::string& value, std::uint64_t time_ns);
    // The circuit's voltages for the devices' analog pins (SN-224).
    void set_analog_source(const std::function<double(std::size_t device, std::size_t pin, std::uint64_t time_ns)>& source);
    // The circuit's level on the net of a chip pin (nets without a board pin).
    void set_circuit_level(std::size_t device, std::size_t pin, bool level, std::uint64_t time_ns);
    // The board pin is on a digital net: the bus sets its input, not the circuit.
    bool owns_board_pin(int port, int pin) const { return board_net_.count({port, pin}) > 0; }

    devices::Output output(std::size_t device, std::size_t pin) const { return outputs_[device][pin]; }
    std::size_t device_count() const { return devices_.size(); }
    std::string text(std::size_t device) const { return devices_[device] ? devices_[device]->text() : std::string(); }
    // Memory chips (SN-208).
    std::vector<std::uint8_t> memory(std::size_t device) const
    {
        return devices_[device] ? devices_[device]->memory() : std::vector<std::uint8_t>{};
    }
    std::uint64_t memory_version(std::size_t device) const
    {
        return devices_[device] ? devices_[device]->memory_version() : 0;
    }
    void load_memory(std::size_t device, const std::vector<std::uint8_t>& bytes)
    {
        if (devices_[device]) {
            devices_[device]->load_memory(bytes);
        }
    }
    std::vector<Change> take_changes() { return std::exchange(changes_, {}); }
    std::vector<std::string> take_messages();

private:
    struct DevicePin {
        std::size_t device = 0;
        std::size_t pin = 0;
    };
    struct Net {
        std::string name;
        std::vector<std::pair<int, int>> board; // port, pin
        std::vector<DevicePin> pins;
        int pull = 0;
        bool circuit = false;       // the circuit's level counts when nothing drives the net
        bool circuit_known = false;
        bool circuit_level = false;
        bool level = false;
        bool known = false;
        bool warned_short = false;
    };

    void resolve(std::size_t net, std::uint64_t time_ns);
    void settle(std::uint64_t time_ns);
    void collect_outputs(std::size_t device, std::uint64_t time_ns); // records changes, marks their nets
    void mark(std::size_t net);

    emu::Stm32F103* machine_ = nullptr;
    std::vector<std::unique_ptr<devices::Device>> devices_; // nullptr: unpowered
    std::vector<std::string> designators_;
    std::vector<std::vector<std::string>> pin_names_;
    std::vector<std::vector<int>> pin_net_; // device -> pin -> net, -1 if tied
    std::vector<std::vector<devices::Output>> outputs_;
    std::vector<Net> nets_;
    std::map<std::pair<int, int>, std::size_t> board_net_;
    std::deque<std::size_t> dirty_;
    std::vector<bool> queued_;
    bool busy_ = false;
    std::vector<Change> changes_;
    std::vector<std::string> messages_;
};

} // namespace simnodus::mcu
