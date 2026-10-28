// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu_machine.hpp"

#include "emu/elf.hpp"
#include "emu/esp32c3.hpp"
#include "emu/stm32f103.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace simnodus::mcu {

namespace {

// ---------------------------------------------------------------------------
// STM32F103 (Blue Pill).

// "PA9" -> 0 * 16 + 9; -1 for anything else.
int stm32_pin(const std::string& name)
{
    if (name.size() < 3 || name[0] != 'P' || name[1] < 'A' || name[1] > 'E') {
        return -1;
    }
    const int pin = std::atoi(name.c_str() + 2);
    return pin >= 0 && pin < 16 ? (name[1] - 'A') * 16 + pin : -1;
}

// `level`: the level on the pin, shown for inputs (outputs show what they drive).
PinState stm32_state(std::uint8_t mode, std::uint8_t cnf, bool drive_high, bool pull_up, bool level)
{
    PinState s;
    if (mode == 0) {
        switch (cnf) {
        case 0: s.mode = PinMode::Analog; break;
        case 2: s.mode = PinMode::Input; s.pull = pull_up ? Pull::Up : Pull::Down; break;
        default: s.mode = PinMode::Input; break;
        }
        s.level = level;
        return s;
    }
    s.mode = cnf >= 2 ? PinMode::Alternate : PinMode::Output;
    s.open_drain = (cnf & 1u) != 0;
    s.level = drive_high;
    return s;
}

class Stm32Machine : public McuMachine {
public:
    explicit Stm32Machine(const McuRunOptions& options)
        : owned_(std::make_unique<emu::Stm32F103>(machine_options(options))), machine_(*owned_)
    {
    }
    explicit Stm32Machine(emu::Stm32F103& machine) : machine_(machine) {}

    int pin_id(const std::string& name) const override { return stm32_pin(name); }
    int uart_id(const std::string& name) const override
    {
        return name.size() == 6 && name.rfind("usart", 0) == 0 && name[5] >= '1' && name[5] <= '3' ? name[5] - '0' : -1;
    }
    bool load(const McuRunOptions& options, std::string& error) override
    {
        if (options.chips_only) {
            // Chips without a board (SN-212): a core that sleeps (WFI) until the devices' next event.
            emu::ElfImage image;
            std::vector<std::uint8_t> bytes(0x24, 0);
            const std::uint32_t vectors[2] = {0x20005000u, 0x08000021u}; // stack, reset handler
            std::memcpy(bytes.data(), vectors, sizeof vectors);
            bytes[0x20] = 0x30; // wfi
            bytes[0x21] = 0xBF;
            bytes[0x22] = 0xFD; // b wfi
            bytes[0x23] = 0xE7;
            image.entry = 0x08000021;
            image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
            return machine_.load(image, error);
        }
        // Without a firmware file (debugging only) the flash stays empty until
        // the debugger's `load` writes it.
        if (!options.firmware.empty() || options.gdb_port < 0) {
            const auto image = emu::read_elf(options.firmware, error);
            return image && machine_.load(*image, error);
        }
        return true;
    }
    std::uint64_t time_ns() const override { return machine_.time_ns(); }
    void run_until_ns(std::uint64_t ns) override { machine_.run_until_ns(ns); }
    void press_reset() override { machine_.reset(emu::ResetKind::Pin); } // RESET pulls NRST low
    PinState pin_state(int pin) const override
    {
        const auto c = machine_.pin_config(pin / 16, pin % 16);
        return stm32_state(c.mode, c.cnf, c.drive_high, c.pull_up, c.output_level);
    }
    bool pin_level(int pin) const override { return machine_.pin_level(pin / 16, pin % 16); }
    void set_pin_input(int pin, bool level, std::uint64_t changed_ns) override
    {
        machine_.set_pin_input(pin / 16, pin % 16, level, changed_ns);
    }
    void set_pin_input(int pin, bool level) override { machine_.set_pin_input(pin / 16, pin % 16, level); }
    void set_analog_source(std::function<double(int, std::uint64_t)> source) override
    {
        machine_.set_analog_source([source = std::move(source)](int port, int pin, std::uint64_t time_ns) {
            return source(port * 16 + pin, time_ns);
        });
    }
    void uart_receive(int uart, std::uint8_t byte) override { machine_.uart_receive(uart, byte); }
    std::vector<PinEvent> take_pin_events() override
    {
        std::vector<PinEvent> events;
        for (const auto& e : machine_.take_pin_events()) {
            events.push_back({e.time_ns, e.port * 16 + e.pin, stm32_state(e.mode, e.cnf, e.drive_high, e.pull_up, e.level),
                e.level});
        }
        return events;
    }
    std::vector<UartByte> take_uart_bytes() override
    {
        std::vector<UartByte> bytes;
        for (const auto& b : machine_.take_uart_bytes()) {
            bytes.push_back({b.time_ns, b.usart, b.value});
        }
        return bytes;
    }
    std::vector<std::string> take_diagnostics() override
    {
        std::vector<std::string> texts;
        for (auto& d : machine_.take_diagnostics()) {
            texts.push_back(std::move(d.text));
        }
        return texts;
    }
    void set_pin_hook(std::function<void(int, std::uint64_t)> hook) override
    {
        if (!hook) {
            machine_.set_pin_hook(nullptr);
            return;
        }
        machine_.set_pin_hook([hook = std::move(hook)](int port, int pin, std::uint64_t time_ns) {
            hook(port * 16 + pin, time_ns);
        });
    }
    void add_timed(emu::TimedDevice* device) override { machine_.add_timed(device); }
    void remove_timed(emu::TimedDevice* device) override { machine_.remove_timed(device); }
    void timed_schedule_changed() override { machine_.timed_schedule_changed(); }
    std::uint64_t cycles_at(std::uint64_t ns) const override { return machine_.cycles_at(ns); }
    std::uint64_t ns_at(std::uint64_t cycle) const override { return machine_.ns_at(cycle); }
    emu::Stm32F103* stm32() override { return &machine_; }

private:
    static emu::MachineOptions machine_options(const McuRunOptions& options)
    {
        emu::MachineOptions m;
        // A frame on the RX pin is decided once every edge up to its stop bit has
        // arrived: the ADC's lag plus two windows (an input is applied at a window
        // boundary, while the firmware may be a window further on).
        m.uart_rx_lag_ns = options.analog_lag_ns + 2 * std::max<std::uint64_t>(options.chunk_us, 1) * 1000;
        m.uart_rx_min_bit_ns = options.uart_rx_min_bit_ns;
        m.cortex_m3_timing = options.cortex_m3_timing;
        // Pins on a net with a device: the device bus gives their edges at once.
        for (const auto& net : options.nets) {
            if (net.device_pins.empty()) {
                continue;
            }
            for (const auto& pin : net.board_pins) {
                const int id = stm32_pin(pin);
                if (id >= 0 && id < 64) {
                    m.exact_input_pins |= std::uint64_t{1} << id;
                }
            }
        }
        for (const auto& uart : options.uart_rx_byte_links) {
            if (!uart.empty() && uart.back() >= '1' && uart.back() <= '3') {
                m.uart_rx_pins &= ~(1u << (uart.back() - '1'));
            }
        }
        return m;
    }

    std::unique_ptr<emu::Stm32F103> owned_;
    emu::Stm32F103& machine_;
};

// ---------------------------------------------------------------------------
// ESP32-C3 (ADR 0106): pins "GPIO0".."GPIO21", UARTs "uart0", "uart1" and
// "usb" (USB-Serial-JTAG). DeviceBus cycles are nanoseconds here (the CPU
// clock changes while the firmware runs).

int c3_pin(const std::string& name)
{
    if (name.size() < 5 || name.rfind("GPIO", 0) != 0) {
        return -1;
    }
    for (std::size_t i = 4; i < name.size(); ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return -1;
        }
    }
    const int pin = std::atoi(name.c_str() + 4);
    return pin < emu::Esp32C3::gpio_count ? pin : -1;
}

PinState c3_state(const emu::Esp32C3::PinState& s, bool pad)
{
    PinState state;
    if (s.output) {
        state.mode = s.peripheral ? PinMode::Alternate : PinMode::Output;
        state.open_drain = s.open_drain;
        state.level = s.level;
        return state;
    }
    state.mode = s.input || s.peripheral ? PinMode::Input : PinMode::Unknown;
    state.pull = s.pull_up && !s.pull_down ? Pull::Up : s.pull_down && !s.pull_up ? Pull::Down : Pull::None;
    state.level = pad;
    return state;
}

class Esp32C3Machine : public McuMachine {
public:
    Esp32C3Machine() = default;
    ~Esp32C3Machine() override
    {
        for (auto& adapter : adapters_) {
            machine_.remove_timed(adapter.get());
        }
    }

    int pin_id(const std::string& name) const override { return c3_pin(name); }
    int uart_id(const std::string& name) const override
    {
        return name == "uart0" ? 0 : name == "uart1" ? 1 : name == "usb" ? 2 : -1;
    }
    bool load(const McuRunOptions& options, std::string& error) override
    {
        if (options.gdb_port >= 0) {
            error = "Debugging with GDB is not available for the ESP32-C3 yet.";
            return false;
        }
        if (options.rom.empty()) {
            error = "The ESP32-C3's ROM file (esp32c3_rev3_rom.elf) was not found beside SimNodus.";
            return false;
        }
        const auto rom = emu::read_elf(options.rom, error, emu::ElfMachine::Riscv);
        if (!rom || !machine_.load_rom(*rom, error)) {
            error = "The ESP32-C3's ROM file could not be read: " + error;
            return false;
        }
        if (options.firmware.empty()) {
            error = "The ESP32-C3 board has no firmware file (a .bin flash image).";
            return false;
        }
        std::ifstream stream(options.firmware, std::ios::binary);
        if (!stream) {
            error = "Cannot open the firmware file " + options.firmware.string() + ".";
            return false;
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (bytes.size() >= 4 && bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F') {
            error = "The ESP32-C3 runs a flash image (.bin, as written by esptool or the IDE's merged binary), "
                    "not an ELF file.";
            return false;
        }
        if (!machine_.load_flash(0, bytes, error)) {
            return false;
        }
        machine_.reset();
        return true;
    }
    std::uint64_t time_ns() const override { return machine_.time_ns(); }
    void run_until_ns(std::uint64_t ns) override { machine_.run_until_ns(ns); }
    void press_reset() override { machine_.press_reset(); }
    PinState pin_state(int pin) const override { return c3_state(machine_.pin(pin), machine_.pad_level(pin)); }
    bool pin_level(int pin) const override { return machine_.pad_level(pin); }
    void set_pin_input(int pin, bool level, std::uint64_t changed_ns) override
    {
        (void)changed_ns;
        machine_.set_pin_input(pin, level);
    }
    void set_pin_input(int pin, bool level) override { machine_.set_pin_input(pin, level); }
    void set_analog_source(std::function<double(int, std::uint64_t)> source) override
    {
        machine_.set_analog_source([source = std::move(source)](int gpio, std::uint64_t time_ns) -> std::optional<double> {
            return source(gpio, time_ns);
        });
    }
    void uart_receive(int uart, std::uint8_t byte) override
    {
        machine_.send(uart, std::string(1, static_cast<char>(byte)));
    }
    std::vector<PinEvent> take_pin_events() override
    {
        std::vector<PinEvent> events;
        for (const auto& e : machine_.take_pin_events()) {
            const bool pad = e.state.output ? e.state.level : machine_.pad_level(e.pin);
            events.push_back({e.time_ns, e.pin, c3_state(e.state, pad), pad});
        }
        return events;
    }
    std::vector<UartByte> take_uart_bytes() override
    {
        std::vector<UartByte> bytes;
        for (const auto& out : machine_.take_output()) {
            bytes.push_back({out.time_ns, out.port, out.value});
        }
        return bytes;
    }
    std::vector<std::string> take_diagnostics() override { return machine_.take_diagnostics(); }
    void set_pin_hook(std::function<void(int, std::uint64_t)> hook) override { machine_.set_pin_hook(std::move(hook)); }
    void add_timed(emu::TimedDevice* device) override
    {
        adapters_.push_back(std::make_unique<TimedAdapter>(device));
        machine_.add_timed(adapters_.back().get());
    }
    void remove_timed(emu::TimedDevice* device) override
    {
        for (auto it = adapters_.begin(); it != adapters_.end(); ++it) {
            if ((*it)->device == device) {
                machine_.remove_timed(it->get());
                adapters_.erase(it);
                return;
            }
        }
    }
    void timed_schedule_changed() override { machine_.timed_changed(); }
    std::uint64_t cycles_at(std::uint64_t ns) const override { return ns; }
    std::uint64_t ns_at(std::uint64_t cycle) const override { return cycle; }

private:
    // A device timed in "cycles" (here nanoseconds) as the machine's timed event.
    struct TimedAdapter : emu::Esp32C3::Timed {
        explicit TimedAdapter(emu::TimedDevice* d) : device(d) {}
        std::uint64_t next_ns() const override { return device->next_event(); }
        void on_ns(std::uint64_t now) override { device->on_time(now); }
        emu::TimedDevice* device;
    };

    emu::Esp32C3 machine_;
    std::vector<std::unique_ptr<TimedAdapter>> adapters_;
};

} // namespace

std::unique_ptr<McuMachine> make_machine(const McuRunOptions& options, std::string& error)
{
    if (options.chip.empty() || options.chip == "stm32f103") {
        return std::make_unique<Stm32Machine>(options);
    }
    if (options.chip == "esp32c3") {
        return std::make_unique<Esp32C3Machine>();
    }
    error = "SimNodus does not emulate the chip \"" + options.chip + "\".";
    return nullptr;
}

std::unique_ptr<McuMachine> wrap_stm32(emu::Stm32F103& machine)
{
    return std::make_unique<Stm32Machine>(machine);
}

bool chip_gpio(const std::string& chip, const std::string& name)
{
    return chip == "esp32c3" ? c3_pin(name) >= 0 : stm32_pin(name) >= 0;
}

std::filesystem::path find_esp32c3_rom(const std::filesystem::path& application_dir)
{
    const std::filesystem::path name = "esp32c3_rev3_rom.elf";
    std::vector<std::filesystem::path> candidates;
    if (!application_dir.empty()) {
        candidates.push_back(application_dir / "roms" / name);
        candidates.push_back(application_dir / ".." / "share" / "simnodus" / "roms" / name);
    }
#ifdef SIMNODUS_SOURCE_ROM_DIR
    candidates.push_back(std::filesystem::path(SIMNODUS_SOURCE_ROM_DIR) / name);
#endif
    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

} // namespace simnodus::mcu
