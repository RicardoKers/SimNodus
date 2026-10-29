// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "devices/host_link.hpp"

#include "devices/serial_port.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace simnodus::devices {

namespace {

// Telnet (RFC 854) and its COM port option (RFC 2217).
constexpr std::uint8_t iac = 255, dont = 254, do_ = 253, wont = 252, will = 251, sb = 250, se = 240;
constexpr std::uint8_t binary = 0, sga = 3, com_port = 44;
constexpr std::uint8_t set_baudrate = 1, set_datasize = 2, set_parity = 3, set_stopsize = 4, set_control = 5,
                       purge_data = 12, signature = 0;
constexpr std::intptr_t no_socket = -1;

#ifdef _WIN32
struct Winsock {
    Winsock()
    {
        WSADATA data;
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    ~Winsock()
    {
        if (ok) {
            WSACleanup();
        }
    }
    bool ok = false;
};
void close_socket(std::intptr_t s) { closesocket(static_cast<SOCKET>(s)); }
std::string socket_error() { return "socket error " + std::to_string(WSAGetLastError()); }
void set_nonblocking(std::intptr_t s)
{
    u_long on = 1;
    ioctlsocket(static_cast<SOCKET>(s), FIONBIO, &on);
}
#else
struct Winsock {
    bool ok = true;
};
void close_socket(std::intptr_t s) { ::close(static_cast<int>(s)); }
std::string socket_error() { return std::strerror(errno); }
void set_nonblocking(std::intptr_t s) { ::fcntl(static_cast<int>(s), F_SETFL, ::fcntl(static_cast<int>(s), F_GETFL) | O_NONBLOCK); }
#endif

bool readable(std::intptr_t s, int timeout_ms)
{
    fd_set set;
    FD_ZERO(&set);
#ifdef _WIN32
    FD_SET(static_cast<SOCKET>(s), &set);
#else
    FD_SET(static_cast<int>(s), &set);
#endif
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    return ::select(static_cast<int>(s + 1), &set, nullptr, nullptr, &tv) > 0;
}

void send_all(std::intptr_t s, const std::string& bytes)
{
    std::size_t done = 0;
    for (int tries = 0; done < bytes.size() && tries < 200; ++tries) {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL; // a program that went away is no signal for the engine
#else
        const int flags = 0;
#endif
        const auto n = ::send(s, bytes.data() + done, static_cast<int>(bytes.size() - done), flags);
        if (n > 0) {
            done += static_cast<std::size_t>(n);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // the buffer is full
        }
    }
}

// A serial port of the computer; the program at the other end of a virtual
// pair raises its DTR and RTS, which arrive here as DSR and CTS.
class PortLink : public HostLink {
public:
    PortLink(std::unique_ptr<SerialPort> port, std::string name) : port_(std::move(port)), name_(std::move(name)) {}
    std::size_t read(std::uint8_t* buffer, std::size_t size) override { return port_->read(buffer, size); }
    void write(const std::uint8_t* bytes, std::size_t size) override { port_->write(bytes, size); }
    HostLines lines() override
    {
        bool dsr = false;
        bool cts = false;
        if (!port_->modem_status(dsr, cts)) {
            return {};
        }
        return {dsr, cts};
    }
    std::string description() const override { return name_; }

private:
    std::unique_ptr<SerialPort> port_;
    std::string name_;
};

// One program at a time on 127.0.0.1:port. A program that starts with a
// Telnet command (pyserial's rfc2217://) gets Telnet framing and the COM port
// option; one that sends data first (socket://, a raw TCP terminal) gets the
// bytes as they are.
class TcpLink : public HostLink {
public:
    explicit TcpLink(std::uint16_t port) : port_(port) {}
    ~TcpLink() override
    {
        drop_client();
        if (listener_ != no_socket) {
            close_socket(listener_);
        }
    }

    bool listen(std::string& error)
    {
        if (!winsock_.ok) {
            error = "the network could not be started";
            return false;
        }
        listener_ = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (listener_ == no_socket) {
            error = socket_error();
            return false;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port_);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // never other interfaces
        if (::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
            || ::listen(listener_, 1) != 0) {
            error = "TCP port " + std::to_string(port_) + " is in use or not allowed (" + socket_error() + ")";
            close_socket(listener_);
            listener_ = no_socket;
            return false;
        }
        set_nonblocking(listener_);
        return true;
    }

    std::size_t read(std::uint8_t* buffer, std::size_t size) override
    {
        service();
        const std::size_t count = std::min(size, data_.size());
        std::memcpy(buffer, data_.data(), count);
        data_.erase(0, count);
        return count;
    }

    void write(const std::uint8_t* bytes, std::size_t size) override
    {
        if (client_ == no_socket) {
            return;
        }
        std::string out;
        out.reserve(size);
        for (std::size_t i = 0; i < size; ++i) {
            out += static_cast<char>(bytes[i]);
            if (bytes[i] == iac && telnet_) {
                out += static_cast<char>(iac);
            }
        }
        send_all(client_, out);
    }

    HostLines lines() override
    {
        service();
        return client_ == no_socket ? HostLines{} : lines_;
    }

    std::string description() const override { return "TCP port " + std::to_string(port_) + " (RFC 2217)"; }

private:
    void service()
    {
        if (client_ == no_socket && readable(listener_, 0)) {
            const auto s = static_cast<std::intptr_t>(::accept(listener_, nullptr, nullptr));
            if (s != no_socket) {
                client_ = s;
                set_nonblocking(client_);
                int on = 1;
                ::setsockopt(client_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
                decided_ = telnet_ = false;
                lines_ = {};
                state_ = State::Data;
                we_.fill(false);
                they_.fill(false);
            }
        }
        if (client_ == no_socket) {
            return;
        }
        char buffer[4096];
        for (;;) {
            const auto n = ::recv(client_, buffer, sizeof(buffer), 0);
            if (n == 0) {
                drop_client();
                return;
            }
            if (n < 0) {
#ifdef _WIN32
                if (WSAGetLastError() != WSAEWOULDBLOCK) {
                    drop_client();
                }
#else
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    drop_client();
                }
#endif
                return;
            }
            for (int i = 0; i < n; ++i) {
                take(static_cast<std::uint8_t>(buffer[i]));
            }
        }
    }

    void drop_client()
    {
        if (client_ != no_socket) {
            close_socket(client_);
            client_ = no_socket;
        }
        lines_ = {};
    }

    void take(std::uint8_t b)
    {
        if (!decided_) {
            decided_ = true;
            telnet_ = b == iac;
        }
        if (!telnet_) {
            data_ += static_cast<char>(b);
            return;
        }
        switch (state_) {
        case State::Data:
            if (b == iac) {
                state_ = State::Command;
            } else {
                data_ += static_cast<char>(b);
            }
            break;
        case State::Command:
            if (b == iac) {
                data_ += static_cast<char>(iac);
                state_ = State::Data;
            } else if (b == will || b == wont || b == do_ || b == dont) {
                verb_ = b;
                state_ = State::Option;
            } else if (b == sb) {
                sub_.clear();
                state_ = State::Sub;
            } else {
                state_ = State::Data; // NOP, GA and the others
            }
            break;
        case State::Option:
            negotiate(verb_, b);
            state_ = State::Data;
            break;
        case State::Sub:
            if (b == iac) {
                state_ = State::SubIac;
            } else {
                sub_ += static_cast<char>(b);
            }
            break;
        case State::SubIac:
            if (b == se) {
                subnegotiation();
                state_ = State::Data;
            } else {
                sub_ += static_cast<char>(b); // IAC IAC: a 255 inside
                state_ = State::Sub;
            }
            break;
        }
    }

    static bool accepted(std::uint8_t option) { return option == binary || option == sga || option == com_port; }

    void reply(std::uint8_t verb, std::uint8_t option)
    {
        send_all(client_, std::string{static_cast<char>(iac), static_cast<char>(verb), static_cast<char>(option)});
    }

    // Answers only changes, so that both sides' requests settle.
    void negotiate(std::uint8_t verb, std::uint8_t option)
    {
        if (verb == will) {
            if (!accepted(option)) {
                reply(dont, option);
            } else if (!they_[option]) {
                they_[option] = true;
                reply(do_, option);
            }
        } else if (verb == do_) {
            if (!accepted(option)) {
                reply(wont, option);
            } else if (!we_[option]) {
                we_[option] = true;
                reply(will, option);
            }
        } else if (verb == wont && they_[option]) {
            they_[option] = false;
            reply(dont, option);
        } else if (verb == dont && we_[option]) {
            we_[option] = false;
            reply(wont, option);
        }
    }

    void subnegotiation()
    {
        if (sub_.size() < 2 || static_cast<std::uint8_t>(sub_[0]) != com_port) {
            return;
        }
        const auto command = static_cast<std::uint8_t>(sub_[1]);
        std::string value = sub_.substr(2);
        const auto byte = value.empty() ? 0 : static_cast<std::uint8_t>(value[0]);
        switch (command) {
        case signature: value = "SimNodus"; break;
        case set_baudrate:
            if (value.size() == 4) {
                std::uint32_t baud = 0;
                for (const char c : value) {
                    baud = baud << 8 | static_cast<std::uint8_t>(c);
                }
                if (baud != 0) {
                    baud_ = baud;
                }
                value.clear();
                for (int shift = 24; shift >= 0; shift -= 8) {
                    value += static_cast<char>((baud_ >> shift) & 0xFF);
                }
            }
            break;
        case set_datasize: value = std::string(1, static_cast<char>(byte ? byte : 8)); break;
        case set_parity: value = std::string(1, static_cast<char>(byte ? byte : 1)); break; // 1: none
        case set_stopsize: value = std::string(1, static_cast<char>(byte ? byte : 1)); break;
        case set_control:
            switch (byte) {
            case 0: value = "\x01"; break; // flow control: none
            case 4: value = "\x06"; break; // break: off
            case 7: value = lines_.dtr ? "\x08" : "\x09"; break;
            case 8: lines_.dtr = true; break;
            case 9: lines_.dtr = false; break;
            case 10: value = lines_.rts ? "\x0B" : "\x0C"; break;
            case 11: lines_.rts = true; break;
            case 12: lines_.rts = false; break;
            default: break;
            }
            break;
        case purge_data:
            // 1 is the buffer of the board's bytes for the program (none wait
            // here); 2 and 3 include the program's bytes the board has not read.
            if (byte == 2 || byte == 3) {
                data_.clear();
            }
            break;
        default: break; // masks, flow suspend and resume: acknowledged as they are
        }
        std::string out{static_cast<char>(iac), static_cast<char>(sb), static_cast<char>(com_port),
            static_cast<char>(command + 100)};
        for (const char c : value) {
            out += c;
            if (static_cast<std::uint8_t>(c) == iac) {
                out += c;
            }
        }
        out += static_cast<char>(iac);
        out += static_cast<char>(se);
        send_all(client_, out);
    }

    enum class State { Data, Command, Option, Sub, SubIac };

    Winsock winsock_;
    std::uint16_t port_;
    std::intptr_t listener_ = no_socket;
    std::intptr_t client_ = no_socket;
    bool decided_ = false;
    bool telnet_ = false;
    State state_ = State::Data;
    std::uint8_t verb_ = 0;
    std::string sub_;
    std::string data_; // received, not yet read
    std::array<bool, 256> we_{};
    std::array<bool, 256> they_{};
    HostLines lines_;
    std::uint32_t baud_ = 115200;
};

} // namespace

std::uint16_t tcp_link_port(const std::string& spec)
{
    for (const char* prefix : {"tcp:", "rfc2217:"}) {
        const std::size_t length = std::strlen(prefix);
        if (spec.size() > length && spec.compare(0, length, prefix) == 0) {
            const unsigned long port = std::strtoul(spec.c_str() + length, nullptr, 10);
            return port > 0 && port < 65536 ? static_cast<std::uint16_t>(port) : 0;
        }
    }
    return 0;
}

std::unique_ptr<HostLink> open_host_link(const std::string& spec, std::string& error)
{
    if (const std::uint16_t port = tcp_link_port(spec)) {
        auto link = std::make_unique<TcpLink>(port);
        if (!link->listen(error)) {
            return nullptr;
        }
        return link;
    }
    if (spec.rfind("tcp:", 0) == 0 || spec.rfind("rfc2217:", 0) == 0) {
        error = "the TCP port must be a number from 1 to 65535";
        return nullptr;
    }
    SerialSettings settings;
    settings.name = spec;
    auto port = open_serial_port(settings, error);
    if (!port) {
        return nullptr;
    }
    return std::make_unique<PortLink>(std::move(port), spec);
}

// --- Test client ------------------------------------------------------------

Rfc2217TestClient::Rfc2217TestClient() = default;

Rfc2217TestClient::~Rfc2217TestClient()
{
    if (socket_ != no_socket) {
        close_socket(socket_);
    }
}

bool Rfc2217TestClient::connect(std::uint16_t port, std::string& error)
{
    static Winsock winsock;
    socket_ = static_cast<std::intptr_t>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (socket_ == no_socket || ::connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = socket_error();
        return false;
    }
    // As pyserial's rfc2217:// starts: the options it needs.
    raw(std::string{static_cast<char>(iac), static_cast<char>(will), static_cast<char>(com_port),
        static_cast<char>(iac), static_cast<char>(do_), static_cast<char>(binary)});
    return true;
}

void Rfc2217TestClient::raw(const std::string& bytes) { send_all(socket_, bytes); }

void Rfc2217TestClient::send(const std::string& bytes)
{
    std::string out;
    for (const char c : bytes) {
        out += c;
        if (static_cast<std::uint8_t>(c) == iac) {
            out += c;
        }
    }
    raw(out);
}

void Rfc2217TestClient::set_lines(bool dtr, bool rts)
{
    for (const char value : {static_cast<char>(dtr ? 8 : 9), static_cast<char>(rts ? 11 : 12)}) {
        raw(std::string{static_cast<char>(iac), static_cast<char>(sb), static_cast<char>(com_port),
            static_cast<char>(set_control), value, static_cast<char>(iac), static_cast<char>(se)});
    }
}

void Rfc2217TestClient::set_baud(std::uint32_t baud)
{
    std::string out{static_cast<char>(iac), static_cast<char>(sb), static_cast<char>(com_port),
        static_cast<char>(set_baudrate)};
    for (int shift = 24; shift >= 0; shift -= 8) {
        out += static_cast<char>((baud >> shift) & 0xFF);
    }
    out += static_cast<char>(iac);
    out += static_cast<char>(se);
    raw(out);
}

std::string Rfc2217TestClient::receive(int timeout_ms)
{
    std::string data;
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    char buffer[4096];
    while (std::chrono::steady_clock::now() < end) {
        if (!readable(socket_, 10)) {
            continue;
        }
        const auto n = ::recv(socket_, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }
        pending_.append(buffer, static_cast<std::size_t>(n));
    }
    // Telnet commands out, doubled 255s back to one.
    std::size_t i = 0;
    while (i < pending_.size()) {
        const auto b = static_cast<std::uint8_t>(pending_[i]);
        if (b != iac) {
            data += pending_[i++];
            continue;
        }
        if (i + 1 >= pending_.size()) {
            break;
        }
        const auto verb = static_cast<std::uint8_t>(pending_[i + 1]);
        if (verb == iac) {
            data += pending_[i];
            i += 2;
        } else if (verb == sb) {
            const auto end_sub = pending_.find(std::string{static_cast<char>(iac), static_cast<char>(se)}, i);
            if (end_sub == std::string::npos) {
                break;
            }
            if (i + 3 < pending_.size() && static_cast<std::uint8_t>(pending_[i + 3]) >= 100) {
                ++acks_;
            }
            i = end_sub + 2;
        } else if (verb >= will && verb <= dont) {
            if (i + 2 >= pending_.size()) {
                break;
            }
            i += 3;
        } else {
            i += 2;
        }
    }
    pending_.erase(0, i);
    return data;
}

} // namespace simnodus::devices
