// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "gdb_socket.hpp"

#include <chrono>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using socklen_type = socklen_t;
#endif

namespace simnodus::emu {

namespace {

constexpr std::intptr_t invalid = -1;

#ifdef _WIN32
using native_socket = SOCKET;
native_socket native(std::intptr_t s)
{
    return static_cast<SOCKET>(s);
}
void close_socket(std::intptr_t s)
{
    closesocket(native(s));
}
struct WinsockInit {
    WinsockInit()
    {
        WSADATA data;
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    ~WinsockInit()
    {
        if (ok) {
            WSACleanup();
        }
    }
    bool ok = false;
};
bool network_ready()
{
    static WinsockInit init;
    return init.ok;
}
#else
using native_socket = int;
native_socket native(std::intptr_t s)
{
    return static_cast<int>(s);
}
void close_socket(std::intptr_t s)
{
    ::close(native(s));
}
bool network_ready()
{
    return true;
}
#endif

void no_delay(std::intptr_t s)
{
    int one = 1;
    setsockopt(native(s), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
}

} // namespace

GdbSocket::GdbSocket() = default;

GdbSocket::~GdbSocket()
{
    close_client();
    if (listener_ != invalid) {
        close_socket(listener_);
    }
}

bool GdbSocket::listen(std::uint16_t port, std::string& error)
{
    if (!network_ready()) {
        error = "The network layer (Winsock) could not start.";
        return false;
    }
    const native_socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (static_cast<std::intptr_t>(s) == invalid) {
        error = "Cannot create a socket for the GDB server.";
        return false;
    }
#ifndef _WIN32
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // loopback only
    if (::bind(s, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0 || ::listen(s, 1) != 0) {
        close_socket(static_cast<std::intptr_t>(s));
        error = "Cannot listen on 127.0.0.1:" + std::to_string(port) + " (is the port in use?).";
        return false;
    }
    socklen_type length = sizeof address;
    getsockname(s, reinterpret_cast<sockaddr*>(&address), &length);
    port_ = ntohs(address.sin_port);
    listener_ = static_cast<std::intptr_t>(s);
    return true;
}

bool GdbSocket::wait_readable(std::intptr_t socket, int timeout_ms) const
{
#ifdef _WIN32
    WSAPOLLFD fd{native(socket), POLLRDNORM, 0};
    return WSAPoll(&fd, 1, timeout_ms) > 0;
#else
    pollfd fd{native(socket), POLLIN, 0};
    return ::poll(&fd, 1, timeout_ms) > 0;
#endif
}

bool GdbSocket::accept(int timeout_ms)
{
    if (client_ != invalid) {
        return true;
    }
    if (listener_ == invalid || !wait_readable(listener_, timeout_ms)) {
        return false;
    }
    const native_socket s = ::accept(native(listener_), nullptr, nullptr);
    if (static_cast<std::intptr_t>(s) == invalid) {
        return false;
    }
    client_ = static_cast<std::intptr_t>(s);
    no_delay(client_);
    no_ack_ = false;
    buffer_.clear();
    return true;
}

bool GdbSocket::connected() const
{
    return client_ != invalid;
}

void GdbSocket::close_client()
{
    if (client_ != invalid) {
        close_socket(client_);
        client_ = invalid;
    }
    buffer_.clear();
}

std::string GdbSocket::frame(const std::string& payload)
{
    unsigned sum = 0;
    for (const char c : payload) {
        sum += static_cast<unsigned char>(c);
    }
    char tail[4];
    std::snprintf(tail, sizeof tail, "#%02x", sum & 0xFFu);
    return "$" + payload + tail;
}

std::string GdbSocket::escape(const std::string& data)
{
    std::string out;
    out.reserve(data.size());
    for (const char c : data) {
        if (c == '#' || c == '$' || c == '}' || c == '*') {
            out += '}';
            out += static_cast<char>(c ^ 0x20);
        } else {
            out += c;
        }
    }
    return out;
}

void GdbSocket::extract_packets(std::vector<std::string>& packets, bool& interrupt)
{
    std::size_t at = 0;
    while (at < buffer_.size()) {
        const char c = buffer_[at];
        if (c == '\x03') {
            interrupt = true;
            ++at;
        } else if (c == '$') {
            const auto hash = buffer_.find('#', at);
            if (hash == std::string::npos || hash + 2 >= buffer_.size()) {
                break; // incomplete
            }
            packets.push_back(buffer_.substr(at + 1, hash - at - 1));
            at = hash + 3;
            if (!no_ack_) {
                send_raw("+");
            }
        } else {
            ++at; // '+', '-' and noise between packets
        }
    }
    buffer_.erase(0, at);
}

bool GdbSocket::poll(int timeout_ms, std::vector<std::string>& packets, bool& interrupt)
{
    if (client_ == invalid) {
        return false;
    }
    if (wait_readable(client_, timeout_ms)) {
        char data[4096];
        const auto n = ::recv(native(client_), data, sizeof data, 0);
        if (n <= 0) {
            close_client();
            return false;
        }
        buffer_.append(data, static_cast<std::size_t>(n));
    }
    extract_packets(packets, interrupt);
    return true;
}

bool GdbSocket::send_raw(const std::string& bytes)
{
    if (client_ == invalid) {
        return false;
    }
    std::size_t sent = 0;
    while (sent < bytes.size()) {
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL; // a closed client must not raise SIGPIPE
#else
        constexpr int flags = 0;
#endif
        const auto n = ::send(native(client_), bytes.data() + sent, static_cast<int>(bytes.size() - sent), flags);
        if (n <= 0) {
            close_client();
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

void GdbSocket::send(const std::string& payload)
{
    send_raw(frame(payload));
}

bool GdbSocket::connect_client(std::uint16_t port, std::string& error)
{
    if (!network_ready()) {
        error = "The network layer (Winsock) could not start.";
        return false;
    }
    const native_socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (static_cast<std::intptr_t>(s) == invalid
        || ::connect(s, reinterpret_cast<sockaddr*>(&address), sizeof address) != 0) {
        error = "Cannot connect to 127.0.0.1:" + std::to_string(port) + ".";
        return false;
    }
    client_ = static_cast<std::intptr_t>(s);
    no_delay(client_);
    no_ack_ = true; // the test client does not acknowledge
    return true;
}

bool GdbSocket::receive_packet(int timeout_ms, std::string& payload)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        const auto start = buffer_.find('$');
        if (start != std::string::npos) {
            const auto hash = buffer_.find('#', start);
            if (hash != std::string::npos && hash + 2 < buffer_.size()) {
                payload = buffer_.substr(start + 1, hash - start - 1);
                buffer_.erase(0, hash + 3);
                return true;
            }
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now());
        if (left.count() <= 0 || client_ == invalid || !wait_readable(client_, static_cast<int>(left.count()))) {
            return false;
        }
        char data[4096];
        const auto n = ::recv(native(client_), data, sizeof data, 0);
        if (n <= 0) {
            close_client();
            return false;
        }
        buffer_.append(data, static_cast<std::size_t>(n));
    }
}

} // namespace simnodus::emu
