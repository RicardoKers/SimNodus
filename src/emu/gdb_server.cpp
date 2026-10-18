// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "gdb_server.hpp"

#include "stm32f103.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace simnodus::emu {

namespace {

constexpr int register_count = 21; // r0-r15, xpsr, msp, psp, primask, control

// Register numbers are those of the target description below.
const char* const target_xml =
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

std::string hex_byte(unsigned value)
{
    char text[3];
    std::snprintf(text, sizeof text, "%02x", value & 0xFFu);
    return text;
}

std::string hex_word_le(std::uint32_t value)
{
    std::string out;
    for (int i = 0; i < 4; ++i) {
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

std::uint32_t word_le(const std::string& hex)
{
    std::uint32_t value = 0;
    for (int i = 3; i >= 0; --i) {
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

} // namespace

GdbServer::GdbServer(Stm32F103& machine) : machine_(machine) {}

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
    machine_.cpu().set_debugger_attached(true);
}

void GdbServer::detach()
{
    machine_.cpu().set_debugger_attached(false); // clears breakpoints and the halt
    machine_.cpu().set_watchpoints({});
    watchpoints_.clear();
    breakpoints_.clear();
    socket_.close_client();
    state_ = State::NoClient;
}

bool GdbServer::service(int wait_ms)
{
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
    if (state_ == State::Running && machine_.halted()) {
        state_ = State::Halted;
        reply(stop_reply());
    }
}

std::uint32_t GdbServer::read_register(int n) const
{
    const Cpu& cpu = const_cast<Stm32F103&>(machine_).cpu();
    if (n < 16) {
        return cpu.reg(n);
    }
    switch (n) {
    case 16: return cpu.xpsr();
    case 17: return cpu.msp();
    case 18: return cpu.psp();
    case 19: return cpu.primask();
    case 20: return cpu.control();
    default: return 0;
    }
}

bool GdbServer::write_register(int n, std::uint32_t value)
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
    return n < register_count; // MSP, PSP, PRIMASK, CONTROL: accepted, not changed
}

std::string GdbServer::read_registers() const
{
    std::string out;
    for (int n = 0; n < register_count; ++n) {
        out += hex_word_le(read_register(n));
    }
    return out;
}

std::string GdbServer::read_memory(std::uint32_t address, std::uint32_t length) const
{
    std::string out;
    Bus& bus = const_cast<Stm32F103&>(machine_).bus();
    for (std::uint32_t i = 0; i < length; ++i) {
        std::uint32_t value = 0;
        if (!bus.read(address + i, 1, value)) {
            break;
        }
        out += hex_byte(value);
    }
    return out.empty() && length > 0 ? std::string("E01") : out;
}

bool GdbServer::write_memory(std::uint32_t address, const std::string& bytes)
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
            flash_written_ = true;
        }
    }
    if (code || address < 0x20000000u) {
        machine_.cpu().invalidate_code_cache();
    }
    return true;
}

void GdbServer::start_after_load()
{
    // After `load` the vector table is new: reset so the core takes its stack
    // pointer and reset handler from it (gdb only sets the PC).
    if (flash_written_) {
        flash_written_ = false;
        machine_.reset(ResetKind::Pin);
    }
}

std::string GdbServer::stop_reply() const
{
    const auto& watch = const_cast<Stm32F103&>(machine_).cpu().last_watch();
    if (!watch.hit) {
        return "S05"; // SIGTRAP: breakpoint, BKPT or step
    }
    const char* kind = watch.kind == Cpu::WatchKind::Write ? "watch" : (watch.kind == Cpu::WatchKind::Read ? "rwatch" : "awatch");
    char text[48];
    std::snprintf(text, sizeof text, "T05%s:%08x;", kind, static_cast<unsigned>(watch.address));
    return text;
}

void GdbServer::update_breakpoints()
{
    machine_.cpu().set_breakpoints({breakpoints_.begin(), breakpoints_.end()});
}

void GdbServer::monitor(const std::string& command)
{
    std::string text;
    if (command == "reset" || command == "reset halt" || command == "reset init") {
        machine_.reset(ResetKind::Pin);
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
        for (int n = 0; n < register_count && 1 + 8 * static_cast<std::size_t>(n) + 8 <= packet.size(); ++n) {
            write_register(n, word_le(packet.substr(1 + 8 * static_cast<std::size_t>(n), 8)));
        }
        reply("OK");
        return;
    }
    case 'p': {
        const int n = static_cast<int>(parse_hex(packet, at));
        reply(n < register_count ? hex_word_le(read_register(n)) : std::string("E01"));
        return;
    }
    case 'P': {
        const int n = static_cast<int>(parse_hex(packet, at));
        reply(at + 9 <= packet.size() && write_register(n, word_le(packet.substr(at + 1, 8))) ? "OK" : "E01");
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
        reply(length == 0 || write_memory(address, bytes) ? "OK" : "E01");
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
            const auto watch_kind = static_cast<Cpu::WatchKind>(type - '0');
            auto same = [&](const Cpu::Watchpoint& w) {
                return w.address == address && w.length == length && w.kind == watch_kind;
            };
            watchpoints_.erase(std::remove_if(watchpoints_.begin(), watchpoints_.end(), same), watchpoints_.end());
            if (kind == 'Z') {
                watchpoints_.push_back({address, length, watch_kind});
            }
            machine_.cpu().set_watchpoints(watchpoints_);
            reply("OK");
            return;
        }
        if (type != '0' && type != '1') {
            reply("");
            return;
        }
        at = 3;
        const std::uint32_t address = parse_hex(packet, at) & ~1u;
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
        machine_.cpu().resume_from_halt();
        state_ = State::Running;
        return; // the stop reply comes when the firmware halts
    case 's':
    case 'S':
        start_after_load();
        machine_.step_instruction();
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
        machine_.reset(ResetKind::Pin);
        return;
    default:
        break;
    }
    if (starts_with(packet, "qSupported")) {
        reply("PacketSize=4000;qXfer:features:read+;QStartNoAckMode+;vContSupported+");
    } else if (packet == "QStartNoAckMode") {
        reply("OK");
        socket_.set_no_ack(true);
    } else if (starts_with(packet, "qXfer:features:read:target.xml:")) {
        std::size_t p = std::string("qXfer:features:read:target.xml:").size(); // offset,length
        const std::uint32_t offset = parse_hex(packet, p);
        ++p;
        const std::uint32_t length = parse_hex(packet, p);
        const std::string xml = target_xml;
        if (offset >= xml.size()) {
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
