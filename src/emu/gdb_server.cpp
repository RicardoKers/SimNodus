// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "gdb_server.hpp"

#include "atmega.hpp"
#include "esp32c3.hpp"
#include "stm32f103.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace simnodus::emu {

namespace {

// The Cortex-M3: r0-r15, xpsr, msp, psp, primask, control (numbers of the description).
const char* const cortex_m_xml =
    "<?xml version=\"1.0\"?>"
    "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
    "<target version=\"1.0\">"
    "<architecture>arm</architecture>"
    "<feature name=\"org.gnu.gdb.arm.m-profile\">"
    "<reg name=\"r0\" bitsize=\"32\" regnum=\"0\"/>"
    "<reg name=\"r1\" bitsize=\"32\"/>"
    "<reg name=\"r2\" bitsize=\"32\"/>"
    "<reg name=\"r3\" bitsize=\"32\"/>"
    "<reg name=\"r4\" bitsize=\"32\"/>"
    "<reg name=\"r5\" bitsize=\"32\"/>"
    "<reg name=\"r6\" bitsize=\"32\"/>"
    "<reg name=\"r7\" bitsize=\"32\"/>"
    "<reg name=\"r8\" bitsize=\"32\"/>"
    "<reg name=\"r9\" bitsize=\"32\"/>"
    "<reg name=\"r10\" bitsize=\"32\"/>"
    "<reg name=\"r11\" bitsize=\"32\"/>"
    "<reg name=\"r12\" bitsize=\"32\"/>"
    "<reg name=\"sp\" bitsize=\"32\" type=\"data_ptr\"/>"
    "<reg name=\"lr\" bitsize=\"32\"/>"
    "<reg name=\"pc\" bitsize=\"32\" type=\"code_ptr\"/>"
    "<reg name=\"xpsr\" bitsize=\"32\" regnum=\"16\"/>"
    "</feature>"
    "<feature name=\"org.gnu.gdb.arm.m-system\">"
    "<reg name=\"msp\" bitsize=\"32\" regnum=\"17\" type=\"data_ptr\"/>"
    "<reg name=\"psp\" bitsize=\"32\" regnum=\"18\" type=\"data_ptr\"/>"
    "<reg name=\"primask\" bitsize=\"32\" regnum=\"19\"/>"
    "<reg name=\"control\" bitsize=\"32\" regnum=\"20\"/>"
    "</feature>"
    "</target>";

// RISC-V RV32 (the ESP32-C3): x0-x31 and pc (0-32), and the machine CSRs as
// gdb numbers them (65 + the CSR number).
const char* const riscv_xml =
    "<?xml version=\"1.0\"?>"
    "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
    "<target version=\"1.0\">"
    "<architecture>riscv:rv32</architecture>"
    "<feature name=\"org.gnu.gdb.riscv.cpu\">"
    "<reg name=\"zero\" bitsize=\"32\" regnum=\"0\" type=\"int\"/>"
    "<reg name=\"ra\" bitsize=\"32\" regnum=\"1\" type=\"code_ptr\"/>"
    "<reg name=\"sp\" bitsize=\"32\" regnum=\"2\" type=\"data_ptr\"/>"
    "<reg name=\"gp\" bitsize=\"32\" regnum=\"3\" type=\"data_ptr\"/>"
    "<reg name=\"tp\" bitsize=\"32\" regnum=\"4\" type=\"data_ptr\"/>"
    "<reg name=\"t0\" bitsize=\"32\" regnum=\"5\" type=\"int\"/>"
    "<reg name=\"t1\" bitsize=\"32\" regnum=\"6\" type=\"int\"/>"
    "<reg name=\"t2\" bitsize=\"32\" regnum=\"7\" type=\"int\"/>"
    "<reg name=\"fp\" bitsize=\"32\" regnum=\"8\" type=\"data_ptr\"/>"
    "<reg name=\"s1\" bitsize=\"32\" regnum=\"9\" type=\"int\"/>"
    "<reg name=\"a0\" bitsize=\"32\" regnum=\"10\" type=\"int\"/>"
    "<reg name=\"a1\" bitsize=\"32\" regnum=\"11\" type=\"int\"/>"
    "<reg name=\"a2\" bitsize=\"32\" regnum=\"12\" type=\"int\"/>"
    "<reg name=\"a3\" bitsize=\"32\" regnum=\"13\" type=\"int\"/>"
    "<reg name=\"a4\" bitsize=\"32\" regnum=\"14\" type=\"int\"/>"
    "<reg name=\"a5\" bitsize=\"32\" regnum=\"15\" type=\"int\"/>"
    "<reg name=\"a6\" bitsize=\"32\" regnum=\"16\" type=\"int\"/>"
    "<reg name=\"a7\" bitsize=\"32\" regnum=\"17\" type=\"int\"/>"
    "<reg name=\"s2\" bitsize=\"32\" regnum=\"18\" type=\"int\"/>"
    "<reg name=\"s3\" bitsize=\"32\" regnum=\"19\" type=\"int\"/>"
    "<reg name=\"s4\" bitsize=\"32\" regnum=\"20\" type=\"int\"/>"
    "<reg name=\"s5\" bitsize=\"32\" regnum=\"21\" type=\"int\"/>"
    "<reg name=\"s6\" bitsize=\"32\" regnum=\"22\" type=\"int\"/>"
    "<reg name=\"s7\" bitsize=\"32\" regnum=\"23\" type=\"int\"/>"
    "<reg name=\"s8\" bitsize=\"32\" regnum=\"24\" type=\"int\"/>"
    "<reg name=\"s9\" bitsize=\"32\" regnum=\"25\" type=\"int\"/>"
    "<reg name=\"s10\" bitsize=\"32\" regnum=\"26\" type=\"int\"/>"
    "<reg name=\"s11\" bitsize=\"32\" regnum=\"27\" type=\"int\"/>"
    "<reg name=\"t3\" bitsize=\"32\" regnum=\"28\" type=\"int\"/>"
    "<reg name=\"t4\" bitsize=\"32\" regnum=\"29\" type=\"int\"/>"
    "<reg name=\"t5\" bitsize=\"32\" regnum=\"30\" type=\"int\"/>"
    "<reg name=\"t6\" bitsize=\"32\" regnum=\"31\" type=\"int\"/>"
    "<reg name=\"pc\" bitsize=\"32\" regnum=\"32\" type=\"code_ptr\"/>"
    "</feature>"
    "<feature name=\"org.gnu.gdb.riscv.csr\">"
    "<reg name=\"mstatus\" bitsize=\"32\" regnum=\"833\" group=\"csr\"/>"
    "<reg name=\"misa\" bitsize=\"32\" regnum=\"834\" group=\"csr\"/>"
    "<reg name=\"mie\" bitsize=\"32\" regnum=\"837\" group=\"csr\"/>"
    "<reg name=\"mtvec\" bitsize=\"32\" regnum=\"838\" group=\"csr\"/>"
    "<reg name=\"mscratch\" bitsize=\"32\" regnum=\"897\" group=\"csr\"/>"
    "<reg name=\"mepc\" bitsize=\"32\" regnum=\"898\" group=\"csr\"/>"
    "<reg name=\"mcause\" bitsize=\"32\" regnum=\"899\" group=\"csr\"/>"
    "<reg name=\"mtval\" bitsize=\"32\" regnum=\"900\" group=\"csr\"/>"
    "<reg name=\"mip\" bitsize=\"32\" regnum=\"901\" group=\"csr\"/>"
    "</feature>"
    "</target>";

std::string hex_byte(unsigned value)
{
    char text[3];
    std::snprintf(text, sizeof text, "%02x", value & 0xFFu);
    return text;
}

std::string hex_word_le(std::uint32_t value, int bytes = 4)
{
    std::string out;
    for (int i = 0; i < bytes; ++i) {
        out += hex_byte(value >> (8 * i));
    }
    return out;
}

int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::string from_hex(const std::string& hex)
{
    std::string out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out += static_cast<char>(hex_digit(hex[i]) * 16 + hex_digit(hex[i + 1]));
    }
    return out;
}

std::string to_hex(const std::string& text)
{
    std::string out;
    for (const char c : text) {
        out += hex_byte(static_cast<unsigned char>(c));
    }
    return out;
}

std::uint32_t word_le(const std::string& hex, int bytes = 4)
{
    std::uint32_t value = 0;
    for (int i = bytes - 1; i >= 0; --i) {
        const auto at = static_cast<std::size_t>(2 * i);
        value = (value << 8) | static_cast<std::uint32_t>(hex_digit(hex[at]) * 16 + hex_digit(hex[at + 1]));
    }
    return value;
}

std::uint32_t parse_hex(const std::string& text, std::size_t& at)
{
    std::uint32_t value = 0;
    while (at < text.size() && hex_digit(text[at]) >= 0) {
        value = value * 16 + static_cast<std::uint32_t>(hex_digit(text[at]));
        ++at;
    }
    return value;
}

std::string unescape(const std::string& data)
{
    std::string out;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (data[i] == '}' && i + 1 < data.size()) {
            out += static_cast<char>(data[++i] ^ 0x20);
        } else {
            out += data[i];
        }
    }
    return out;
}

bool starts_with(const std::string& text, const char* prefix)
{
    return text.rfind(prefix, 0) == 0;
}

// The STM32F103's Cortex-M3.
class Stm32Target : public GdbTarget {
public:
    explicit Stm32Target(Stm32F103& machine) : machine_(machine) {}

    const char* target_xml() const override { return cortex_m_xml; }
    int general_registers() const override { return 21; }
    bool read_register(int n, std::uint32_t& value) override
    {
        const Cpu& cpu = machine_.cpu();
        if (n < 16) {
            value = cpu.reg(n);
            return true;
        }
        switch (n) {
        case 16: value = cpu.xpsr(); return true;
        case 17: value = cpu.msp(); return true;
        case 18: value = cpu.psp(); return true;
        case 19: value = cpu.primask(); return true;
        case 20: value = cpu.control(); return true;
        default: return false;
        }
    }
    bool write_register(int n, std::uint32_t value) override
    {
        Cpu& cpu = machine_.cpu();
        if (n < 16) {
            cpu.set_reg(n, value);
            return true;
        }
        if (n == 16) {
            cpu.set_xpsr(value);
            return true;
        }
        return n < 21; // MSP, PSP, PRIMASK, CONTROL: accepted, not changed
    }
    bool read_byte(std::uint32_t address, std::uint8_t& value) override
    {
        std::uint32_t word = 0;
        if (!machine_.bus().read(address, 1, word)) {
            return false;
        }
        value = static_cast<std::uint8_t>(word);
        return true;
    }
    bool write_memory(std::uint32_t address, const std::string& bytes, bool& loaded) override
    {
        Bus& bus = machine_.bus();
        bool code = false;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const auto at = address + static_cast<std::uint32_t>(i);
            const auto value = static_cast<std::uint8_t>(bytes[i]);
            if (!bus.write(at, 1, value)) {
                // Flash is read-only for the CPU; the debugger writes it as `load` would.
                if (!bus.load(at, &value, 1)) {
                    return false;
                }
                code = true;
                loaded = true;
            }
        }
        if (code || address < 0x20000000u) {
            machine_.cpu().invalidate_code_cache();
        }
        return true;
    }
    void set_debugger_attached(bool attached) override { machine_.cpu().set_debugger_attached(attached); }
    void set_breakpoints(std::vector<std::uint32_t> addresses) override
    {
        machine_.cpu().set_breakpoints(std::move(addresses));
    }
    void set_watchpoints(const std::vector<Watchpoint>& watchpoints) override
    {
        std::vector<Cpu::Watchpoint> list;
        for (const auto& w : watchpoints) {
            list.push_back({w.address, w.length, static_cast<Cpu::WatchKind>(w.kind)});
        }
        machine_.cpu().set_watchpoints(std::move(list));
    }
    WatchHit last_watch() const override
    {
        const auto& hit = machine_.cpu().last_watch();
        return {hit.hit, hit.address, static_cast<WatchKind>(hit.kind)};
    }
    bool halted() const override { return machine_.halted(); }
    void resume() override { machine_.cpu().resume_from_halt(); }
    void step() override { machine_.step_instruction(); }
    void reset() override { machine_.reset(ResetKind::Pin); }

private:
    Stm32F103& machine_;
};

// The ESP32-C3's RISC-V core. Its flash is written with esptool or the
// board's Firmware: writes to the flash's address windows fail (gdb `load`
// is not supported); RAM and registers can be written.
class Esp32C3Target : public GdbTarget {
public:
    explicit Esp32C3Target(Esp32C3& machine) : machine_(machine) {}

    const char* target_xml() const override { return riscv_xml; }
    int general_registers() const override { return 33; }
    bool read_register(int n, std::uint32_t& value) override
    {
        RiscvCpu& cpu = machine_.cpu();
        if (n < 32) {
            value = cpu.reg(n);
            return true;
        }
        if (n == 32) {
            value = cpu.pc();
            return true;
        }
        return n >= 65 && n < 65 + 4096 && cpu.read_csr(static_cast<std::uint16_t>(n - 65), value);
    }
    bool write_register(int n, std::uint32_t value) override
    {
        RiscvCpu& cpu = machine_.cpu();
        if (n < 32) {
            cpu.set_reg(n, value);
            return true;
        }
        if (n == 32) {
            cpu.set_pc(value);
            return true;
        }
        return n >= 65 && n < 65 + 4096 && cpu.write_csr(static_cast<std::uint16_t>(n - 65), value);
    }
    bool read_byte(std::uint32_t address, std::uint8_t& value) override
    {
        std::uint32_t word = 0;
        if (!machine_.bus().read(address, 1, word)) {
            return false;
        }
        value = static_cast<std::uint8_t>(word);
        return true;
    }
    bool write_memory(std::uint32_t address, const std::string& bytes, bool& loaded) override
    {
        (void)loaded;
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            if (!machine_.bus().write(address + static_cast<std::uint32_t>(i), 1, static_cast<std::uint8_t>(bytes[i]))) {
                return false;
            }
        }
        machine_.cpu().invalidate_code_cache(); // code in SRAM may have changed
        return true;
    }
    void set_debugger_attached(bool attached) override { machine_.cpu().set_debugger_attached(attached); }
    void set_breakpoints(std::vector<std::uint32_t> addresses) override
    {
        machine_.cpu().set_breakpoints(std::move(addresses));
    }
    void set_watchpoints(const std::vector<Watchpoint>& watchpoints) override
    {
        std::vector<RiscvCpu::Watchpoint> list;
        for (const auto& w : watchpoints) {
            list.push_back({w.address, w.length, static_cast<RiscvCpu::WatchKind>(w.kind)});
        }
        machine_.cpu().set_watchpoints(std::move(list));
    }
    WatchHit last_watch() const override
    {
        const auto& hit = machine_.cpu().last_watch();
        return {hit.hit, hit.address, static_cast<WatchKind>(hit.kind)};
    }
    bool halted() const override { return machine_.halted(); }
    void resume() override { machine_.cpu().resume_from_halt(); }
    void step() override { machine_.step_instruction(); }
    void reset() override { machine_.reset(); } // as the EN button: the ROM starts again

private:
    Esp32C3& machine_;
};

// An ATmega's AVR core (SN-262), as avr-gdb sees it: r0-r31 (one byte each),
// SREG (one byte), SP (two bytes) and PC (four bytes, a byte address);
// avr-gdb knows them without a target description. Its addresses: the flash
// from 0, the data space (registers, I/O, SRAM) from 0x800000, the EEPROM
// from 0x810000. Writes to the flash are a programmer's (gdb `load`): the
// next run or step starts from a reset. The fuse, lock and signature spaces
// (0x820000 and above) accept writes and keep the part's fuses.
class AvrTarget : public GdbTarget {
public:
    static constexpr std::uint32_t data_base = 0x800000;
    static constexpr std::uint32_t eeprom_base = 0x810000;
    static constexpr std::uint32_t fuse_base = 0x820000;
    static constexpr std::uint32_t space_end = 0x900000;

    explicit AvrTarget(Atmega& machine) : machine_(machine) {}

    const char* target_xml() const override { return nullptr; }
    int general_registers() const override { return 35; }
    int register_size(int n) const override { return n < 33 ? 1 : (n == 33 ? 2 : 4); }
    bool read_register(int n, std::uint32_t& value) override
    {
        const AvrCpu& cpu = machine_.cpu();
        if (n < 32) {
            value = cpu.reg(n);
        } else if (n == 32) {
            value = cpu.sreg();
        } else if (n == 33) {
            value = cpu.sp();
        } else if (n == 34) {
            value = cpu.pc() * 2;
        } else {
            return false;
        }
        return true;
    }
    bool write_register(int n, std::uint32_t value) override
    {
        AvrCpu& cpu = machine_.cpu();
        if (n < 32) {
            cpu.set_reg(n, static_cast<std::uint8_t>(value));
        } else if (n == 32) {
            cpu.set_sreg(static_cast<std::uint8_t>(value));
        } else if (n == 33) {
            cpu.set_sp(static_cast<std::uint16_t>(value));
        } else if (n == 34) {
            cpu.set_pc(value / 2);
        } else {
            return false;
        }
        return true;
    }
    bool read_byte(std::uint32_t address, std::uint8_t& value) override
    {
        if (address < data_base) {
            if (address >= machine_.flash_size()) {
                return false;
            }
            value = machine_.flash_byte(address);
            return true;
        }
        if (address < eeprom_base) {
            return address - data_base < 0x10000
                && machine_.debug_read(static_cast<std::uint16_t>(address - data_base), value);
        }
        if (address < fuse_base) {
            if (address - eeprom_base >= machine_.eeprom_size()) {
                return false;
            }
            value = machine_.eeprom()[address - eeprom_base];
            return true;
        }
        return false;
    }
    bool write_memory(std::uint32_t address, const std::string& bytes, bool& loaded) override
    {
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const std::uint32_t at = address + static_cast<std::uint32_t>(i);
            const auto value = static_cast<std::uint8_t>(bytes[i]);
            bool done = false;
            if (at < data_base) {
                done = machine_.debug_write_flash(at, value);
                loaded = loaded || done;
            } else if (at < eeprom_base) {
                done = at - data_base < 0x10000
                    && machine_.debug_write(static_cast<std::uint16_t>(at - data_base), value);
            } else if (at < fuse_base) {
                done = machine_.debug_write_eeprom(at - eeprom_base, value);
            } else {
                done = at < space_end; // fuses, lock bits, signature: the part's stay
            }
            if (!done) {
                return false;
            }
        }
        return true;
    }
    void set_debugger_attached(bool attached) override { machine_.cpu().set_debugger_attached(attached); }
    void set_breakpoints(std::vector<std::uint32_t> addresses) override
    {
        for (auto& a : addresses) {
            a /= 2; // the core counts words
        }
        machine_.cpu().set_breakpoints(std::move(addresses));
    }
    bool can_break(std::uint32_t address) const override { return address < machine_.flash_size(); }
    bool can_watch(const Watchpoint& w) const override
    {
        return w.address >= data_base && w.address + w.length <= data_base + machine_.data_size();
    }
    void set_watchpoints(const std::vector<Watchpoint>& watchpoints) override
    {
        std::vector<AvrCpu::Watchpoint> list;
        for (const auto& w : watchpoints) {
            if (can_watch(w)) {
                list.push_back({w.address - data_base, w.length, static_cast<AvrCpu::WatchKind>(w.kind)});
            }
        }
        machine_.cpu().set_watchpoints(std::move(list));
    }
    WatchHit last_watch() const override
    {
        const auto& hit = machine_.cpu().last_watch();
        return {hit.hit, hit.address + data_base, static_cast<WatchKind>(hit.kind)};
    }
    bool halted() const override { return machine_.halted(); }
    void resume() override { machine_.cpu().resume_from_halt(); }
    void step() override { machine_.step_instruction(); }
    // As at power-on (MCUSR's PORF): a bootloader such as the UNO's Optiboot
    // starts the application at once.
    void reset() override { machine_.reset(Atmega::ResetKind::PowerOn); }

private:
    Atmega& machine_;
};

} // namespace

std::unique_ptr<GdbTarget> make_gdb_target(Stm32F103& machine)
{
    return std::make_unique<Stm32Target>(machine);
}

std::unique_ptr<GdbTarget> make_gdb_target(Esp32C3& machine)
{
    return std::make_unique<Esp32C3Target>(machine);
}

std::unique_ptr<GdbTarget> make_gdb_target(Atmega& machine)
{
    return std::make_unique<AvrTarget>(machine);
}

GdbServer::GdbServer(GdbTarget& target) : target_(target) {}

GdbServer::~GdbServer()
{
    if (state_ != State::NoClient) {
        detach();
    }
}

bool GdbServer::start(std::uint16_t port, std::string& error)
{
    return socket_.listen(port, error);
}

void GdbServer::attach()
{
    state_ = State::Halted; // a client finds the firmware stopped where it is
    breakpoints_.clear();
    target_.set_debugger_attached(true);
}

void GdbServer::release_target()
{
    target_.set_debugger_attached(false); // clears breakpoints and the halt
    target_.set_watchpoints({});
    watchpoints_.clear();
    breakpoints_.clear();
}

void GdbServer::detach()
{
    release_target();
    socket_.close_client();
    state_ = State::NoClient;
}

bool GdbServer::service(int wait_ms)
{
    if (state_ != State::NoClient && socket_.take_over()) {
        // A new client replaced the attached one, as when an IDE relaunches
        // its debug session while the previous one still holds the connection.
        release_target();
        attach();
    }
    if (state_ == State::NoClient) {
        if (!socket_.accept(wait_first_ ? wait_ms : 0)) {
            return !wait_first_;
        }
        wait_first_ = false; // after this client detaches, the firmware runs freely
        attach();
    }
    std::vector<std::string> packets;
    bool interrupt = false;
    if (!socket_.poll(state_ == State::Halted ? wait_ms : 0, packets, interrupt)) {
        detach(); // the client went away: the firmware runs on
        return true;
    }
    if (interrupt && state_ == State::Running) {
        state_ = State::Halted;
        reply("S02"); // SIGINT
    }
    for (const auto& packet : packets) {
        handle(packet);
        if (state_ == State::NoClient) {
            break;
        }
    }
    return state_ != State::Halted;
}

void GdbServer::after_run()
{
    if (state_ == State::Running && target_.halted()) {
        state_ = State::Halted;
        reply(stop_reply());
    }
}

std::string GdbServer::read_registers()
{
    std::string out;
    for (int n = 0; n < target_.general_registers(); ++n) {
        std::uint32_t value = 0;
        target_.read_register(n, value);
        out += hex_word_le(value, target_.register_size(n));
    }
    return out;
}

std::string GdbServer::read_memory(std::uint32_t address, std::uint32_t length)
{
    std::string out;
    for (std::uint32_t i = 0; i < length; ++i) {
        std::uint8_t value = 0;
        if (!target_.read_byte(address + i, value)) {
            break;
        }
        out += hex_byte(value);
    }
    return out.empty() && length > 0 ? std::string("E01") : out;
}

void GdbServer::start_after_load()
{
    // After `load` the vector table is new: reset so the core takes its stack
    // pointer and reset handler from it (gdb only sets the PC).
    if (flash_written_) {
        flash_written_ = false;
        target_.reset();
    }
}

std::string GdbServer::stop_reply() const
{
    const auto watch = target_.last_watch();
    if (!watch.hit) {
        return "S05"; // SIGTRAP: breakpoint, BKPT/EBREAK or step
    }
    const char* kind = watch.kind == GdbTarget::WatchKind::Write ? "watch"
        : (watch.kind == GdbTarget::WatchKind::Read ? "rwatch" : "awatch");
    char text[48];
    std::snprintf(text, sizeof text, "T05%s:%08x;", kind, static_cast<unsigned>(watch.address));
    return text;
}

void GdbServer::update_breakpoints()
{
    target_.set_breakpoints({breakpoints_.begin(), breakpoints_.end()});
}

void GdbServer::monitor(const std::string& command)
{
    std::string text;
    if (command == "reset" || command == "reset halt" || command == "reset init") {
        target_.reset();
        flash_written_ = false;
        text = "Reset: the firmware is halted at its reset handler.\n";
    } else if (command == "halt") {
        text = "Halted.\n";
    } else {
        text = "Unknown monitor command \"" + command + "\" (SimNodus knows: reset, reset halt, halt).\n";
    }
    reply("O" + to_hex(text));
    reply("OK");
}

void GdbServer::handle(const std::string& packet)
{
    if (packet.empty()) {
        reply("");
        return;
    }
    const char kind = packet[0];
    std::size_t at = 1;
    switch (kind) {
    case '?':
        reply("S05");
        return;
    case 'g':
        reply(read_registers());
        return;
    case 'G': {
        std::size_t p = 1;
        for (int n = 0; n < target_.general_registers(); ++n) {
            const auto size = static_cast<std::size_t>(target_.register_size(n));
            if (p + 2 * size > packet.size()) {
                break;
            }
            target_.write_register(n, word_le(packet.substr(p, 2 * size), static_cast<int>(size)));
            p += 2 * size;
        }
        reply("OK");
        return;
    }
    case 'p': {
        const int n = static_cast<int>(parse_hex(packet, at));
        std::uint32_t value = 0;
        reply(target_.read_register(n, value) ? hex_word_le(value, target_.register_size(n)) : std::string("E01"));
        return;
    }
    case 'P': {
        const int n = static_cast<int>(parse_hex(packet, at));
        const auto size = static_cast<std::size_t>(target_.register_size(n));
        reply(at + 1 + 2 * size <= packet.size()
                    && target_.write_register(n, word_le(packet.substr(at + 1, 2 * size), static_cast<int>(size)))
                ? "OK"
                : "E01");
        return;
    }
    case 'm': {
        const std::uint32_t address = parse_hex(packet, at);
        ++at;
        const std::uint32_t length = parse_hex(packet, at);
        reply(read_memory(address, std::min<std::uint32_t>(length, 0x1000)));
        return;
    }
    case 'M':
    case 'X': {
        const std::uint32_t address = parse_hex(packet, at);
        ++at;
        const std::uint32_t length = parse_hex(packet, at);
        const auto colon = packet.find(':', at);
        if (colon == std::string::npos) {
            reply("E01");
            return;
        }
        std::string bytes = kind == 'M' ? from_hex(packet.substr(colon + 1)) : unescape(packet.substr(colon + 1));
        bytes.resize(length);
        reply(length == 0 || target_.write_memory(address, bytes, flash_written_) ? "OK" : "E01");
        return;
    }
    case 'Z':
    case 'z': {
        const char type = packet.size() > 1 ? packet[1] : '?';
        if (type >= '2' && type <= '4') {
            at = 3;
            const std::uint32_t address = parse_hex(packet, at);
            ++at;
            const std::uint32_t length = std::max<std::uint32_t>(parse_hex(packet, at), 1);
            const auto watch_kind = static_cast<GdbTarget::WatchKind>(type - '0');
            auto same = [&](const GdbTarget::Watchpoint& w) {
                return w.address == address && w.length == length && w.kind == watch_kind;
            };
            if (kind == 'Z' && !target_.can_watch({address, length, watch_kind})) {
                reply("E01"); // gdb then watches by stepping
                return;
            }
            watchpoints_.erase(std::remove_if(watchpoints_.begin(), watchpoints_.end(), same), watchpoints_.end());
            if (kind == 'Z') {
                watchpoints_.push_back({address, length, watch_kind});
            }
            target_.set_watchpoints(watchpoints_);
            reply("OK");
            return;
        }
        if (type != '0' && type != '1') {
            reply("");
            return;
        }
        at = 3;
        const std::uint32_t address = parse_hex(packet, at) & ~1u;
        if (kind == 'Z' && !target_.can_break(address)) {
            reply("E01");
            return;
        }
        if (kind == 'Z') {
            breakpoints_.insert(address);
        } else {
            breakpoints_.erase(address);
        }
        update_breakpoints();
        reply("OK");
        return;
    }
    case 'c':
    case 'C':
        start_after_load();
        target_.resume();
        state_ = State::Running;
        return; // the stop reply comes when the firmware halts
    case 's':
    case 'S':
        start_after_load();
        target_.step();
        reply("S05");
        return;
    case 'D':
        reply("OK");
        detach();
        return;
    case 'k':
        detach();
        return;
    case 'H':
    case 'T':
    case '!':
        reply("OK");
        return;
    case 'R':
        target_.reset();
        return;
    default:
        break;
    }
    if (starts_with(packet, "qSupported")) {
        reply(target_.target_xml() ? "PacketSize=4000;qXfer:features:read+;QStartNoAckMode+;vContSupported+"
                                   : "PacketSize=4000;QStartNoAckMode+;vContSupported+");
    } else if (packet == "QStartNoAckMode") {
        reply("OK");
        socket_.set_no_ack(true);
    } else if (starts_with(packet, "qXfer:features:read:target.xml:")) {
        std::size_t p = std::string("qXfer:features:read:target.xml:").size(); // offset,length
        const std::uint32_t offset = parse_hex(packet, p);
        ++p;
        const std::uint32_t length = parse_hex(packet, p);
        const std::string xml = target_.target_xml() ? target_.target_xml() : "";
        if (xml.empty()) {
            reply("E01");
        } else if (offset >= xml.size()) {
            reply("l");
        } else {
            const std::string part = xml.substr(offset, length);
            reply((offset + part.size() >= xml.size() ? "l" : "m") + GdbSocket::escape(part));
        }
    } else if (packet == "qAttached") {
        reply("1");
    } else if (packet == "qC") {
        reply("QC1");
    } else if (packet == "qfThreadInfo") {
        reply("m1");
    } else if (packet == "qsThreadInfo") {
        reply("l");
    } else if (starts_with(packet, "qSymbol")) {
        reply("OK");
    } else if (starts_with(packet, "qRcmd,")) {
        monitor(from_hex(packet.substr(6)));
    } else if (packet == "vCont?") {
        reply("vCont;c;C;s;S");
    } else if (starts_with(packet, "vCont;")) {
        const char action = packet.size() > 6 ? packet[6] : 'c';
        handle(std::string(1, action == 'S' ? 's' : (action == 'C' ? 'c' : action)));
    } else if (starts_with(packet, "vKill")) {
        reply("OK");
        detach();
    } else {
        reply(""); // not supported
    }
}

} // namespace simnodus::emu
