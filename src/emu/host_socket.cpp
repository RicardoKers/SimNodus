// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "host_socket.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// After winsock2.h: the ICMP helper API (iphlpapi).
#include <iphlpapi.h>
#include <icmpapi.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#include <thread>

namespace simnodus::emu {

namespace {

#ifdef _WIN32
using Native = SOCKET;
constexpr Native invalid = INVALID_SOCKET;
bool would_block()
{
    const int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}
void close_native(Native s)
{
    closesocket(s);
}
void start_sockets()
{
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)started;
}
void make_non_blocking(Native s)
{
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
}
#else
using Native = int;
constexpr Native invalid = -1;
bool would_block()
{
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS;
}
void close_native(Native s)
{
    ::close(s);
}
void start_sockets() {}
void make_non_blocking(Native s)
{
    ::fcntl(s, F_SETFL, ::fcntl(s, F_GETFL, 0) | O_NONBLOCK);
}
#endif

Native native(std::intptr_t handle)
{
    return static_cast<Native>(handle);
}

sockaddr_in socket_address(const HostAddress& address, std::uint16_t port)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    std::memcpy(&a.sin_addr, address.data(), 4);
    return a;
}

} // namespace

HostSocket::HostSocket(std::intptr_t handle) : handle_(handle) {}

HostSocket::~HostSocket()
{
    close_native(native(handle_));
}

std::unique_ptr<HostSocket> HostSocket::connect_tcp(const HostAddress& address, std::uint16_t port)
{
    start_sockets();
    const Native s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == invalid) {
        return nullptr;
    }
    make_non_blocking(s);
    const sockaddr_in a = socket_address(address, port);
    if (::connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0 && !would_block()) {
        close_native(s);
        return nullptr;
    }
    return std::unique_ptr<HostSocket>(new HostSocket(static_cast<std::intptr_t>(s)));
}

std::unique_ptr<HostSocket> HostSocket::listen_tcp(std::uint16_t port, std::string& error)
{
    start_sockets();
    const Native s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == invalid) {
        error = "cannot open a socket";
        return nullptr;
    }
    const sockaddr_in a = socket_address({127, 0, 0, 1}, port);
    if (::bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0 || ::listen(s, 8) != 0) {
        close_native(s);
        error = "port " + std::to_string(port) + " on 127.0.0.1 is in use";
        return nullptr;
    }
    make_non_blocking(s);
    return std::unique_ptr<HostSocket>(new HostSocket(static_cast<std::intptr_t>(s)));
}

std::unique_ptr<HostSocket> HostSocket::open_udp(std::uint16_t port)
{
    start_sockets();
    const Native s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == invalid) {
        return nullptr;
    }
    const sockaddr_in a = port ? socket_address({127, 0, 0, 1}, port) : socket_address({0, 0, 0, 0}, 0);
    if (::bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0) {
        close_native(s);
        return nullptr;
    }
    make_non_blocking(s);
    return std::unique_ptr<HostSocket>(new HostSocket(static_cast<std::intptr_t>(s)));
}

std::unique_ptr<HostSocket> HostSocket::accept()
{
    const Native s = ::accept(native(handle_), nullptr, nullptr);
    if (s == invalid) {
        return nullptr;
    }
    make_non_blocking(s);
    return std::unique_ptr<HostSocket>(new HostSocket(static_cast<std::intptr_t>(s)));
}

int HostSocket::finished()
{
    fd_set write_set;
    fd_set error_set;
    FD_ZERO(&write_set);
    FD_ZERO(&error_set);
    FD_SET(native(handle_), &write_set);
    FD_SET(native(handle_), &error_set);
    timeval zero{};
    if (::select(static_cast<int>(native(handle_) + 1), nullptr, &write_set, &error_set, &zero) <= 0) {
        return 0;
    }
    int code = 0;
    socklen_t length = sizeof(code);
    ::getsockopt(native(handle_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&code), &length);
    return (code == 0 && !FD_ISSET(native(handle_), &error_set)) ? 1 : -1;
}

long HostSocket::send(const std::uint8_t* data, std::size_t count)
{
    const auto n = ::send(native(handle_), reinterpret_cast<const char*>(data), static_cast<int>(count), 0);
    if (n < 0) {
        return would_block() ? 0 : -1;
    }
    return static_cast<long>(n);
}

long HostSocket::receive(std::uint8_t* data, std::size_t count)
{
    const auto n = ::recv(native(handle_), reinterpret_cast<char*>(data), static_cast<int>(count), 0);
    if (n < 0) {
        return would_block() ? -1 : -2;
    }
    return static_cast<long>(n);
}

void HostSocket::shutdown_send()
{
#ifdef _WIN32
    ::shutdown(native(handle_), SD_SEND);
#else
    ::shutdown(native(handle_), SHUT_WR);
#endif
}

bool HostSocket::send_to(const HostAddress& address, std::uint16_t port, const std::uint8_t* data, std::size_t count)
{
    const sockaddr_in a = socket_address(address, port);
    return ::sendto(native(handle_), reinterpret_cast<const char*>(data), static_cast<int>(count), 0,
               reinterpret_cast<const sockaddr*>(&a), sizeof(a)) >= 0;
}

long HostSocket::receive_from(std::uint8_t* data, std::size_t count, HostAddress& address, std::uint16_t& port)
{
    sockaddr_in a{};
    socklen_t length = sizeof(a);
    const auto n = ::recvfrom(native(handle_), reinterpret_cast<char*>(data), static_cast<int>(count), 0,
        reinterpret_cast<sockaddr*>(&a), &length);
    if (n < 0) {
        return -1;
    }
    std::memcpy(address.data(), &a.sin_addr, 4);
    port = ntohs(a.sin_port);
    return static_cast<long>(n);
}

std::optional<HostAddress> host_resolve(const std::string& name)
{
    start_sockets();
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (::getaddrinfo(name.c_str(), nullptr, &hints, &result) != 0 || !result) {
        return std::nullopt;
    }
    HostAddress address{};
    std::memcpy(address.data(), &reinterpret_cast<const sockaddr_in*>(result->ai_addr)->sin_addr, 4);
    ::freeaddrinfo(result);
    return address;
}

namespace {

// One echo request from the computer; true if the answer came within a
// second.
bool ping_once(const HostAddress& address, const std::vector<std::uint8_t>& data)
{
#ifdef _WIN32
    const HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE) {
        return false;
    }
    IPAddr to = 0;
    std::memcpy(&to, address.data(), 4);
    std::vector<unsigned char> reply(sizeof(ICMP_ECHO_REPLY) + data.size() + 64);
    std::vector<unsigned char> payload(data.begin(), data.end());
    const DWORD answers = IcmpSendEcho(icmp, to, payload.empty() ? nullptr : payload.data(),
        static_cast<WORD>(payload.size()), nullptr, reply.data(), static_cast<DWORD>(reply.size()), 1000);
    const bool answered = answers > 0 && reinterpret_cast<const ICMP_ECHO_REPLY*>(reply.data())->Status == IP_SUCCESS;
    IcmpCloseHandle(icmp);
    return answered;
#else
    const int s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (s < 0) {
        return false;
    }
    // Type 8 (echo request); the kernel sets the identifier.
    std::vector<std::uint8_t> request{8, 0, 0, 0, 0, 0, 0, 1};
    request.insert(request.end(), data.begin(), data.end());
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i + 1 < request.size(); i += 2) {
        sum += static_cast<std::uint32_t>((request[i] << 8) | request[i + 1]);
    }
    if (request.size() & 1u) {
        sum += static_cast<std::uint32_t>(request.back() << 8);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    const std::uint16_t check = static_cast<std::uint16_t>(~sum);
    request[2] = static_cast<std::uint8_t>(check >> 8);
    request[3] = static_cast<std::uint8_t>(check);
    const sockaddr_in a = socket_address(address, 0);
    bool answered = false;
    if (::sendto(s, request.data(), request.size(), 0, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) >= 0) {
        pollfd wait{s, POLLIN, 0};
        std::uint8_t reply[1500];
        while (!answered && ::poll(&wait, 1, 1000) > 0) {
            const auto n = ::recv(s, reply, sizeof(reply), 0);
            answered = n >= 8 && reply[0] == 0; // echo reply
        }
    }
    ::close(s);
    return answered;
#endif
}

} // namespace

bool HostPinger::available()
{
#ifdef _WIN32
    const HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE) {
        return false;
    }
    IcmpCloseHandle(icmp);
    return true;
#else
    const int s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (s < 0) {
        return false;
    }
    ::close(s);
    return true;
#endif
}

void HostPinger::send(const HostAddress& address, const std::vector<std::uint8_t>& data, std::uint64_t tag)
{
    std::thread([shared = shared_, address, data, tag] {
        const bool answered = ping_once(address, data);
        std::lock_guard lock(shared->mutex);
        shared->done.push_back({tag, answered});
    }).detach();
}

std::vector<HostPinger::Result> HostPinger::take()
{
    std::vector<Result> results;
    std::lock_guard lock(shared_->mutex);
    results.swap(shared_->done);
    return results;
}

} // namespace simnodus::emu
