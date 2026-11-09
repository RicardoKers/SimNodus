// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu_machine.hpp"

#include "emu/atmega.hpp"

#include "emu/elf.hpp"
#include "emu/esp32c3.hpp"
#include "emu/gdb_server.hpp"
#include "emu/stm32f103.hpp"
#include "emu/wifi_air.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <utility>

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
            if (!image || !machine_.load(*image, error)) {
                return false;
            }
        }
        // The circuit's level on each watched pin starts low, as the runner
        // assumes, and only its changes come later: a pull-up enabled against
        // a net the circuit holds low (an open-collector Hall sensor that is
        // on from the start, E-11) must read the circuit's low, not the pull.
        for (const auto& pin : options.pins) {
            if (const int id = stm32_pin(pin); id >= 0) {
                machine_.set_pin_input(id / 16, id % 16, false);
            }
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
    emu::GdbTarget* gdb_target() override
    {
        if (!gdb_target_) {
            gdb_target_ = emu::make_gdb_target(machine_);
        }
        return gdb_target_.get();
    }

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
    std::unique_ptr<emu::GdbTarget> gdb_target_;
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
        if (air_ && on_air_) {
            air_->detach(machine_);
        }
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
        if (options.rom.empty()) {
            error = "The ESP32-C3's ROM file (esp32c3_rev3_rom.elf) was not found beside SimNodus.";
            return false;
        }
        const auto rom = emu::read_elf(options.rom, error, emu::ElfMachine::Riscv);
        if (!rom || !machine_.load_rom(*rom, error)) {
            error = "The ESP32-C3's ROM file could not be read: " + error;
            return false;
        }
        // Without a firmware the flash stays erased (a blank module, waiting for an upload).
        if (!options.firmware.empty()) {
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
            std::vector<std::uint8_t> flash = bytes;
            if (application_image(bytes) && !assemble_from_flash_args(options.firmware, flash, error)) {
                return false;
            }
            if (!machine_.load_flash(0, flash, error)) {
                return false;
            }
        }
        // UART RX from the pin (SN-254): GPIO20 on a net with a device has
        // exact edges; a port that gets another board's bytes does not decode.
        bool rx_exact = false;
        for (const auto& net : options.nets) {
            if (!net.device_pins.empty()
                && std::find(net.board_pins.begin(), net.board_pins.end(), "GPIO20") != net.board_pins.end()) {
                rx_exact = true;
            }
        }
        machine_.set_uart_pin_rx(0, !options.uart_rx_byte_links.count("uart0"), rx_exact ? 0 : options.uart_rx_min_bit_ns);
        machine_.set_uart_pin_rx(1, !options.uart_rx_byte_links.count("uart1"), options.uart_rx_min_bit_ns);
        for (const auto& [name, value] : options.parameters) {
            set_parameter(name, value);
        }
        // 34:85:18:00:C3:01 for the first board, then 05, 09...: the station,
        // access point and Bluetooth addresses derive from the base (+0..+2).
        machine_.set_mac_last(static_cast<std::uint8_t>(0x01 + 4 * (options.board_index % 63)));
        machine_.reset();
        if (options.wifi) {
            // The run's Wi-Fi network (SN-269), joined when the firmware starts
            // its Wi-Fi: boards that never do pay nothing for it.
            air_ = options.wifi;
            machine_.set_wifi_start([this] {
                if (!on_air_) {
                    on_air_ = true;
                    air_->attach(machine_);
                }
            });
        }
        // The circuit's level on each watched pad starts low, as the runner
        // assumes, and only its changes come later: a pull-up enabled against
        // a resistor to ground must read the circuit's low, not the pull.
        for (const auto& pin : options.pins) {
            if (const int id = c3_pin(pin); id >= 0) {
                machine_.set_pin_input(id, false);
            }
        }
        return true;
    }
    std::uint64_t time_ns() const override { return machine_.time_ns(); }
    void run_until_ns(std::uint64_t ns) override
    {
        if (release_ns_ != no_release && release_ns_ <= ns) {
            machine_.run_until_ns(release_ns_);
            release_ns_ = no_release;
            machine_.set_chip_enable(true);
        }
        machine_.run_until_ns(ns);
    }
    void press_reset() override { machine_.press_reset(); }
    void host_lines(int uart, const std::string& circuit, bool dtr, bool rts) override
    {
        (void)uart;
        if (circuit == "auto_reset") {
            // DTR low and RTS high pull EN low; DTR high and RTS low pull GPIO9
            // low. EN rises through its RC (10 kΩ, 1 µF on the DevKit) and the
            // chip starts later, so a program that sets DTR and RTS one after
            // the other (esptool; over RFC 2217 some 25 ms apart) has GPIO9 low
            // before it starts.
            machine_.set_boot_low(dtr && !rts);
            if (rts && !dtr) {
                release_ns_ = no_release;
                machine_.set_chip_enable(false);
            } else if (!machine_.chip_enabled() && release_ns_ == no_release) {
                release_ns_ = machine_.time_ns() + en_rise_ns;
            }
        } else if (circuit == "usb_jtag") {
            // RTS high with DTR low holds the chip in reset; DTR high with RTS
            // low before it chooses download mode for the restart (esptool's
            // USB reset sequence), both low forgets it; leaving the reset with
            // DTR high chooses it too (the classic DTR/RTS sequence, which
            // esptool and the Arduino IDE send when they cannot tell the port
            // is the chip's USB, as through a virtual port pair or RFC 2217).
            if (rts && !dtr) {
                machine_.set_chip_enable(false);
            } else {
                if (!machine_.chip_enabled()) {
                    machine_.set_download_once(download_latch_ || dtr);
                    machine_.set_chip_enable(true);
                    download_latch_ = false;
                } else if (!dtr && !rts) {
                    download_latch_ = false;
                }
                if (dtr && !rts) {
                    download_latch_ = true;
                }
            }
        }
    }
    void set_parameter(const std::string& name, const std::string& value) override
    {
        char* end = nullptr;
        const double number = std::strtod(value.c_str(), &end);
        if (name == "chip_temperature" && end != value.c_str()) {
            machine_.set_chip_celsius(std::clamp(number, -40.0, 125.0)); // the sensor's range
        }
    }
    std::uint64_t flash_version() const override { return machine_.flash_writes(); }
    bool flash_downloaded() const override { return machine_.download_boots() > 0 && machine_.flash_writes() > 0; }
    std::vector<std::uint8_t> flash() const override { return machine_.flash(); }
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
    emu::GdbTarget* gdb_target() override
    {
        if (!gdb_target_) {
            gdb_target_ = emu::make_gdb_target(machine_);
        }
        return gdb_target_.get();
    }

private:
    // An ESP-IDF or Arduino application image (build/app.bin, the .ino.bin):
    // its first segment loads into the flash's address windows, where a
    // bootloader's never does. It belongs at 0x10000, after the bootloader.
    static bool application_image(const std::vector<std::uint8_t>& bytes)
    {
        if (bytes.size() < 32 || bytes[0] != 0xE9) {
            return false;
        }
        const std::uint32_t load = bytes[24] | (bytes[25] << 8) | (bytes[26] << 16) | (std::uint32_t{bytes[27]} << 24);
        return (load >= 0x3C000000u && load < 0x3C800000u) || (load >= 0x42000000u && load < 0x42800000u);
    }
    // The flash as `esptool write-flash @flash_args` writes it: the build's
    // flash_args beside the application (ESP-IDF's build folder, Arduino's
    // exported binaries) lists each file and its address.
    static bool assemble_from_flash_args(const std::filesystem::path& application, std::vector<std::uint8_t>& flash,
        std::string& error)
    {
        const auto folder = application.parent_path();
        std::ifstream list(folder / "flash_args");
        const std::string name = application.filename().string();
        const std::string advice = name + " is an application image, which belongs at address 0x10000 after the "
                                          "bootloader and the partition table. Choose the merged image (idf.py merge-bin, "
                                          "or the Arduino IDE's .merged.bin), or keep the build's flash_args file beside "
                                          "it so SimNodus can place each part.";
        if (!list) {
            error = advice;
            return false;
        }
        std::vector<std::uint8_t> assembled;
        bool has_application = false;
        for (std::string line; std::getline(list, line);) {
            std::istringstream words(line);
            std::string offset_text;
            std::string file;
            if (!(words >> offset_text >> file) || offset_text.rfind("--", 0) == 0) {
                continue; // the flash options line
            }
            const std::size_t offset = std::strtoul(offset_text.c_str(), nullptr, 0);
            const auto path = folder / std::filesystem::path(std::u8string(file.begin(), file.end()));
            std::error_code ec;
            has_application = has_application || std::filesystem::equivalent(path, application, ec);
            std::ifstream part(path, std::ios::binary);
            if (!part) {
                error = "flash_args beside " + name + " names " + file + ", which cannot be opened.";
                return false;
            }
            const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(part)), std::istreambuf_iterator<char>());
            if (assembled.size() < offset + bytes.size()) {
                assembled.resize(offset + bytes.size(), 0xFF); // erased flash between the parts
            }
            std::copy(bytes.begin(), bytes.end(), assembled.begin() + static_cast<std::ptrdiff_t>(offset));
        }
        if (!has_application) {
            error = advice + " (The flash_args there does not name " + name + ".)";
            return false;
        }
        flash = std::move(assembled);
        return true;
    }

    // A device timed in "cycles" (here nanoseconds) as the machine's timed event.
    struct TimedAdapter : emu::Esp32C3::Timed {
        explicit TimedAdapter(emu::TimedDevice* d) : device(d) {}
        std::uint64_t next_ns() const override { return device->next_event(); }
        void on_ns(std::uint64_t now) override { device->on_time(now); }
        emu::TimedDevice* device;
    };

    static constexpr std::uint64_t no_release = ~0ull;
    static constexpr std::uint64_t en_rise_ns = 50'000'000; // EN's rise, with room for the program's two calls
    std::shared_ptr<emu::WifiAir> air_; // declared before the machine: detached first
    bool on_air_ = false;
    emu::Esp32C3 machine_;
    std::uint64_t release_ns_ = no_release; // EN rises then
    bool download_latch_ = false;
    std::vector<std::unique_ptr<TimedAdapter>> adapters_;
    std::unique_ptr<emu::GdbTarget> gdb_target_;
};

// ATmega8 / ATmega8A (experiment E-10, SN-258) and ATmega328 / ATmega328P
// (SN-260): a bare chip in the circuit. Its pins are named as on the chip
// ("PB0".."PD7"), its USART "usart". The fuses, the crystal's frequency (from
// a Crystal part on XTAL1 and XTAL2) and the EEPROM kept in the project come
// in McuRunOptions::parameters: "fuse_low", "fuse_high", "fuse_extended",
// "lock" (hex), "crystal_hz", "eeprom_file".
bool is_avr_chip(const std::string& chip)
{
    return chip == "atmega8" || chip == "atmega8a" || chip == "atmega328" || chip == "atmega328p" || chip == "atmega32"
           || chip == "atmega32a";
}

int avr_pin(const std::string& name, const std::string& chip = {})
{
    if (chip == "atmega32" || chip == "atmega32a") { // port A, PC7 and a RESET pin of its own (PDIP-40)
        if (name == "RESET") {
            return emu::Atmega::reset_pin;
        }
        if (name.size() == 3 && name[0] == 'P' && name[1] >= 'A' && name[1] <= 'D' && name[2] >= '0' && name[2] <= '7') {
            return emu::Atmega::pin_index(name[1], name[2] - '0');
        }
        return -1;
    }
    // The Arduino UNO's header names (SN-261): D0..D13, A0..A5, RESET.
    static const std::map<std::string, std::string> arduino = {{"D0", "PD0"}, {"D1", "PD1"}, {"D2", "PD2"},
        {"D3", "PD3"}, {"D4", "PD4"}, {"D5", "PD5"}, {"D6", "PD6"}, {"D7", "PD7"}, {"D8", "PB0"}, {"D9", "PB1"},
        {"D10", "PB2"}, {"D11", "PB3"}, {"D12", "PB4"}, {"D13", "PB5"}, {"A0", "PC0"}, {"A1", "PC1"}, {"A2", "PC2"},
        {"A3", "PC3"}, {"A4", "PC4"}, {"A5", "PC5"}, {"RESET", "PC6"}};
    if (const auto it = arduino.find(name); it != arduino.end()) {
        return avr_pin(it->second);
    }
    if (name.size() != 3 || name[0] != 'P' || name[1] < 'B' || name[1] > 'D' || name[2] < '0' || name[2] > '7'
        || name == "PC7") {
        return -1;
    }
    return emu::Atmega::pin_index(name[1], name[2] - '0');
}

class AtmegaMachine : public McuMachine {
public:
    explicit AtmegaMachine(const std::string& chip)
        : machine_(emu::Atmega::factory(chip == "atmega8"      ? emu::Atmega::Model::Atmega8
                                        : chip == "atmega328"  ? emu::Atmega::Model::Atmega328
                                        : chip == "atmega328p" ? emu::Atmega::Model::Atmega328P
                                        : chip == "atmega32"   ? emu::Atmega::Model::Atmega32
                                        : chip == "atmega32a"  ? emu::Atmega::Model::Atmega32A
                                                               : emu::Atmega::Model::Atmega8A)),
          chip_(chip)
    {
    }

    int pin_id(const std::string& name) const override
    {
        // The supply pins are watched for their voltages (power, brown-out, the ADC's references).
        return name == "VCC" ? emu::Atmega::vcc_pin : name == "AVCC" ? emu::Atmega::avcc_pin
             : name == "AREF" ? emu::Atmega::aref_pin : avr_pin(name, chip_);
    }
    int uart_id(const std::string& name) const override { return name == "usart" ? 0 : -1; }
    bool load(const McuRunOptions& options, std::string& error) override
    {
        auto byte_of = [&](const char* name, std::uint8_t fallback) {
            const auto it = options.parameters.find(name);
            if (it == options.parameters.end() || it->second.empty()) {
                return fallback;
            }
            return static_cast<std::uint8_t>(std::strtoul(it->second.c_str(), nullptr, 16));
        };
        const emu::Atmega::Options factory = emu::Atmega::factory(machine_.options().model);
        machine_.set_fuses(byte_of("fuse_low", factory.fuse_low), byte_of("fuse_high", factory.fuse_high), byte_of("lock", 0xFF));
        machine_.set_extended_fuse(byte_of("fuse_extended", factory.fuse_extended));
        if (const auto it = options.parameters.find("tosc_hz"); it != options.parameters.end()) {
            machine_.set_tosc_hz(static_cast<std::uint32_t>(std::strtod(it->second.c_str(), nullptr)));
        }
        if (const auto it = options.parameters.find("crystal_hz"); it != options.parameters.end()) {
            machine_.set_external_hz(static_cast<std::uint32_t>(std::strtod(it->second.c_str(), nullptr)));
        }
        const bool has_bootloader = options.parameters.count("bootloader") != 0;
        sync_reset_ = false;
        for (const auto& [name, link] : options.uart_links) {
            sync_reset_ = sync_reset_ || (has_bootloader && link.circuit == "dtr_reset");
        }
        // With debugging (SN-262) the flash may start empty: avr-gdb's `load` writes it.
        if (options.firmware.empty() && !has_bootloader && options.gdb_port < 0) {
            error = std::string("The ") + machine_.chip_name() + " has no firmware file (a .hex or .elf).";
            return false;
        }
        // Programming a bare chip erases it; the EEPROM the project kept from
        // earlier runs is the chip's, so it comes back afterwards (and the
        // firmware's own EEPROM data only fills an EEPROM never kept).
        machine_.chip_erase();
        std::vector<std::uint8_t> kept;
        if (const auto it = options.parameters.find("eeprom_file"); it != options.parameters.end() && !it->second.empty()) {
            std::ifstream in(std::filesystem::path(std::u8string(it->second.begin(), it->second.end())), std::ios::binary);
            kept.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        // A board's bootloader (the Arduino UNO's Optiboot) in the boot section,
        // with the lock bits that protect it, as the factory burns it.
        if (has_bootloader) {
            std::ifstream boot(options.rom, std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(boot)), std::istreambuf_iterator<char>());
            if (options.rom.empty() || text.empty() || !machine_.load_hex(text, error)) {
                error = "The board's bootloader (" + options.parameters.at("bootloader") + ") could not be loaded"
                        + (error.empty() ? std::string(".") : ": " + error);
                return false;
            }
            machine_.set_fuses(machine_.fuse_low(), machine_.fuse_high(), byte_of("lock", 0xFF));
        }
        if (options.firmware.empty()) {
            finish_load(kept, options);
            return true;
        }
        std::ifstream stream(options.firmware, std::ios::binary);
        if (!stream) {
            error = "Cannot open the firmware file " + options.firmware.string() + ".";
            return false;
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        auto extension = options.firmware.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".bin") {
            // The whole flash as a run kept it after an upload (SN-261).
            if (!machine_.load_flash(bytes, error)) {
                error = options.firmware.filename().string() + ": " + error;
                return false;
            }
        } else if (bytes.size() >= 4 && bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F') {
            const auto image = emu::parse_elf(bytes, error, emu::ElfMachine::Avr);
            const std::uint8_t low = machine_.fuse_low(), high = machine_.fuse_high(), lock = machine_.lock_bits();
            bool fuses_set = false;
            if (!image || !machine_.load_elf(*image, error, &fuses_set)) {
                return false;
            }
            if (fuses_set) {
                char text[200];
                std::snprintf(text, sizeof text,
                    "The firmware's ELF sets the fuses (low 0x%02X, high 0x%02X) instead of the part's (low 0x%02X, high 0x%02X), "
                    "as a production programmer writes them",
                    machine_.fuse_low(), machine_.fuse_high(), low, high);
                notes_.push_back(text);
                (void)lock;
            }
        } else if (!machine_.load_hex(std::string(bytes.begin(), bytes.end()), error)) {
            error = options.firmware.filename().string() + ": " + error;
            return false;
        }
        finish_load(kept, options);
        return true;
    }
    void finish_load(const std::vector<std::uint8_t>& kept, const McuRunOptions& options)
    {
        // A board (the UNO) feeds its chip itself: its supply pins are not the circuit's.
        watched_supplies_.clear();
        for (const auto& pin : options.pins) {
            if (pin == "VCC" || pin == "AVCC" || pin == "AREF") {
                watched_supplies_.push_back(pin_id(pin));
            }
        }
        if (!kept.empty()) {
            machine_.set_eeprom(kept);
        }
        eeprom_writes_ = machine_.eeprom_writes();
        flash_writes_ = machine_.flash_writes();
        machine_.reset();
        // The circuit's level on each watched pin starts low, as the runner assumes.
        for (const auto& pin : options.pins) {
            if (const int id = avr_pin(pin, chip_); id >= 0) {
                machine_.set_pin_input(id, false);
            }
        }
    }
    // The Arduino UNO's auto-reset (SN-261): the USB bridge's DTR reaches
    // RESET through 100 nF, so DTR becoming active gives a short low pulse
    // and the chip starts again (the bootloader then waits for an upload).
    void host_lines(int uart, const std::string& circuit, bool dtr, bool rts) override
    {
        (void)uart;
        (void)rts;
        if (circuit == "dtr_reset" && dtr && !dtr_) {
            machine_.request_reset(emu::Atmega::ResetKind::External);
        }
        dtr_ = dtr;
    }
    std::uint64_t time_ns() const override { return machine_.time_ns(); }
    void run_until_ns(std::uint64_t ns) override { machine_.run_until_ns(ns); }
    void press_reset() override { machine_.request_reset(emu::Atmega::ResetKind::External); }
    PinState pin_state(int pin) const override { return state_of(machine_.pin(pin), machine_.pad_level(pin)); }
    bool pin_level(int pin) const override { return machine_.pad_level(pin); }
    void set_pin_input(int pin, bool level, std::uint64_t changed_ns) override
    {
        (void)changed_ns;
        machine_.set_pin_input(pin, level);
    }
    void set_pin_input(int pin, bool level) override { machine_.set_pin_input(pin, level); }
    void set_analog_source(std::function<double(int, std::uint64_t)> source) override
    {
        machine_.set_analog_source([this, source = std::move(source)](int pin, std::uint64_t time_ns) -> std::optional<double> {
            if (pin >= emu::Atmega::vcc_pin
                && std::find(watched_supplies_.begin(), watched_supplies_.end(), pin) == watched_supplies_.end()) {
                return std::nullopt; // 5 V
            }
            return source(pin, time_ns);
        });
    }
    void uart_receive(int uart, std::uint8_t byte) override
    {
        if (uart != 0) {
            return;
        }
        // The UNO's USB bridge also restarts the board when avrdude asks a
        // running sketch for the bootloader's sync (STK_GET_SYNC, "0 ") twice
        // within two seconds: some virtual port pairs do not pass avrdude's
        // DTR on, and without the reset the upload cannot start (SN-261).
        if (sync_reset_ && !machine_.in_boot_section()) {
            if (last_byte_ == 0x30 && byte == 0x20) {
                const std::uint64_t now = machine_.time_ns();
                if (sync_seen_ns_ != 0 && now - sync_seen_ns_ < 2'000'000'000ull) {
                    sync_seen_ns_ = 0;
                    last_byte_ = 0;
                    notes_.push_back("avrdude asked for the bootloader: the board restarts into Optiboot for the upload");
                    machine_.request_reset(emu::Atmega::ResetKind::External);
                    return;
                }
                sync_seen_ns_ = now;
            }
            last_byte_ = byte;
        }
        machine_.uart_receive(byte);
    }
    std::vector<PinEvent> take_pin_events() override
    {
        std::vector<PinEvent> events;
        for (const auto& e : machine_.take_pin_events()) {
            const bool pad = e.state.output ? e.state.level : machine_.pad_level(e.pin);
            events.push_back({e.time_ns, e.pin, state_of(e.state, pad), pad});
        }
        return events;
    }
    std::vector<UartByte> take_uart_bytes() override
    {
        std::vector<UartByte> bytes;
        for (const auto& b : machine_.take_uart_bytes()) {
            bytes.push_back({b.time_ns, 0, b.value});
        }
        return bytes;
    }
    std::vector<std::string> take_diagnostics() override
    {
        auto texts = std::exchange(notes_, {});
        for (auto& t : machine_.take_diagnostics()) {
            texts.push_back(std::move(t));
        }
        return texts;
    }
    void set_pin_hook(std::function<void(int, std::uint64_t)> hook) override { machine_.set_pin_hook(std::move(hook)); }
    void add_timed(emu::TimedDevice* device) override { machine_.add_timed(device); }
    void remove_timed(emu::TimedDevice* device) override { machine_.remove_timed(device); }
    void timed_schedule_changed() override { machine_.timed_changed(); }
    std::uint64_t cycles_at(std::uint64_t ns) const override { return machine_.cycles_at(ns); }
    std::uint64_t ns_at(std::uint64_t cycle) const override { return machine_.ns_at(cycle); }
    // The memories the project keeps from a run: the flash once a bootloader
    // (or the firmware) wrote it with SPM, and the EEPROM.
    std::uint64_t flash_version() const override
    {
        return machine_.flash_writes() > flash_writes_ ? machine_.flash_writes() : 0;
    }
    bool flash_downloaded() const override { return flash_version() != 0; }
    std::vector<std::uint8_t> flash() const override
    {
        std::vector<std::uint8_t> bytes(machine_.flash_size());
        for (std::uint32_t a = 0; a < bytes.size(); ++a) {
            bytes[a] = machine_.flash_byte(a);
        }
        return bytes;
    }
    std::uint64_t eeprom_version() const override
    {
        return machine_.eeprom_writes() > eeprom_writes_ ? machine_.eeprom_writes() : 0;
    }
    std::vector<std::uint8_t> eeprom() const override { return machine_.eeprom(); }
    emu::GdbTarget* gdb_target() override
    {
        if (!gdb_target_) {
            gdb_target_ = emu::make_gdb_target(machine_);
        }
        return gdb_target_.get();
    }

private:
    static PinState state_of(const emu::Atmega::PinState& s, bool pad)
    {
        PinState state;
        if (s.output) {
            state.mode = PinMode::Output;
            state.level = s.level;
            return state;
        }
        state.mode = PinMode::Input;
        state.pull = s.pull_up ? Pull::Up : Pull::None;
        state.level = pad;
        return state;
    }
    emu::Atmega machine_;
    std::string chip_;
    std::uint64_t eeprom_writes_ = 0;
    std::uint64_t flash_writes_ = 0;
    bool dtr_ = false;
    bool sync_reset_ = false;
    std::uint8_t last_byte_ = 0;
    std::uint64_t sync_seen_ns_ = 0;
    std::vector<int> watched_supplies_;
    std::vector<std::string> notes_;
    std::unique_ptr<emu::GdbTarget> gdb_target_; // after machine_: it refers to it
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
    if (is_avr_chip(options.chip)) {
        return std::make_unique<AtmegaMachine>(options.chip);
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
    if (is_avr_chip(chip)) {
        return avr_pin(name, chip) >= 0;
    }
    return chip == "esp32c3" ? c3_pin(name) >= 0 : stm32_pin(name) >= 0;
}

std::filesystem::path find_bootloader(const std::filesystem::path& application_dir, const std::string& name)
{
    std::vector<std::filesystem::path> candidates;
    if (!application_dir.empty()) {
        candidates.push_back(application_dir / "roms" / name);
        candidates.push_back(application_dir / ".." / "share" / "simnodus" / "roms" / name);
    }
#ifdef SIMNODUS_SOURCE_BOOTLOADER_DIR
    candidates.push_back(std::filesystem::path(SIMNODUS_SOURCE_BOOTLOADER_DIR) / name);
#endif
    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
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
