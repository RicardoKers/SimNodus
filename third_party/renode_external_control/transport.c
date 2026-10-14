// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Native IPv4 loopback only, with one deadline per complete client operation.
// Derived from the SN-019 experiment transport (Windows), extended to POSIX.
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
typedef SOCKET sn_handle;
#define SN_INVALID_HANDLE INVALID_SOCKET
#define sn_closesocket closesocket
static uint64_t now_ms(void) { return GetTickCount64(); }
static int would_block(void) { return WSAGetLastError() == WSAEWOULDBLOCK; }
static int startup(void)
{
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}
static void cleanup(void) { WSACleanup(); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef int sn_handle;
#define SN_INVALID_HANDLE (-1)
#define sn_closesocket close
static uint64_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}
static int would_block(void) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
static int startup(void) { return 1; }
static void cleanup(void) {}
#endif

#include <limits.h>
#include <stdlib.h>
#include "transport.h"

struct sn_socket {
    sn_handle handle;
    uint64_t deadline;
    uint32_t timeout_ms;
};

void sn_abort(sn_socket *socket)
{
    if (socket && socket->handle != SN_INVALID_HANDLE) {
        sn_closesocket(socket->handle);
        socket->handle = SN_INVALID_HANDLE;
    }
}

void sn_close(sn_socket *socket)
{
    if (socket) {
        sn_abort(socket);
        free(socket);
        cleanup();
    }
}

void sn_set_timeout(sn_socket *socket, uint32_t milliseconds)
{
    if (socket) {
        socket->timeout_ms = milliseconds ? milliseconds : SN_DEFAULT_TIMEOUT_MS;
    }
}

sn_result sn_begin(sn_socket *socket)
{
    if (!socket || socket->handle == SN_INVALID_HANDLE) { return SN_NOT_CONNECTED; }
    socket->deadline = now_ms() + socket->timeout_ms;
    return SN_OK;
}

static sn_result wait_socket(sn_socket *socket, int writing)
{
    if (!socket || socket->handle == SN_INVALID_HANDLE) { return SN_NOT_CONNECTED; }
    const uint64_t now = now_ms();
    if (now >= socket->deadline) { sn_abort(socket); return SN_TIMEOUT; }
    const uint64_t remaining = socket->deadline - now;
#ifdef _WIN32
    struct timeval timeout = { (long)(remaining / 1000), (long)(remaining % 1000) * 1000 };
    fd_set ready, errors;
    FD_ZERO(&ready); FD_ZERO(&errors);
    FD_SET(socket->handle, &ready); FD_SET(socket->handle, &errors);
    const int count = select(0, writing ? NULL : &ready, writing ? &ready : NULL, &errors, &timeout);
    if (count == 0) { sn_abort(socket); return SN_TIMEOUT; }
    if (count == SOCKET_ERROR || FD_ISSET(socket->handle, &errors)) { sn_abort(socket); return SN_CONNECTION; }
#else
    struct pollfd entry = { socket->handle, (short)(writing ? POLLOUT : POLLIN), 0 };
    const int count = poll(&entry, 1, remaining > INT_MAX ? INT_MAX : (int)remaining);
    if (count == 0) { sn_abort(socket); return SN_TIMEOUT; }
    if (count < 0) {
        if (errno == EINTR) { return SN_OK; }
        sn_abort(socket); return SN_CONNECTION;
    }
    // POLLHUP with pending data still lets recv() return it; recv() reports the closure.
    if ((entry.revents & (POLLERR | POLLNVAL)) != 0) { sn_abort(socket); return SN_CONNECTION; }
#endif
    return SN_OK;
}

static int parse_port(const char *port, unsigned *number)
{
    *number = 0;
    if (!port || !*port) { return 0; }
    for (const char *p = port; *p; ++p) {
        if (*p < '0' || *p > '9' || *number > 6553) { return 0; }
        *number = *number * 10 + (unsigned)(*p - '0');
        if (*number > 65535) { return 0; }
    }
    return *number != 0;
}

static int set_nonblocking(sn_handle handle)
{
#ifdef _WIN32
    u_long nonblocking = 1;
    return ioctlsocket(handle, FIONBIO, &nonblocking) != SOCKET_ERROR;
#else
    const int flags = fcntl(handle, F_GETFL, 0);
    return flags >= 0 && fcntl(handle, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

sn_result sn_open(const char *port, sn_socket **result)
{
    if (!result) { return SN_INVALID; }
    *result = NULL;
    unsigned number = 0;
    if (!parse_port(port, &number)) { return SN_INVALID; }
    if (!startup()) { return SN_CONNECTION; }
    sn_socket *connection = calloc(1, sizeof(*connection));
    if (!connection) { cleanup(); return SN_CONNECTION; }
    connection->timeout_ms = SN_DEFAULT_TIMEOUT_MS;
    connection->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection->handle == SN_INVALID_HANDLE) { sn_close(connection); return SN_CONNECTION; }
    const int no_delay = 1;
    if (!set_nonblocking(connection->handle)
        || setsockopt(connection->handle, IPPROTO_TCP, TCP_NODELAY, (const char *)&no_delay, sizeof(no_delay)) != 0) {
        sn_close(connection); return SN_CONNECTION;
    }
    sn_begin(connection);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)number);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sn_result status = SN_OK;
    if (connect(connection->handle, (const struct sockaddr *)&address, sizeof(address)) != 0) {
#ifdef _WIN32
        const int pending = would_block();
#else
        const int pending = errno == EINPROGRESS || errno == EINTR;
#endif
        status = pending ? wait_socket(connection, 1) : SN_CONNECTION;
    }
    if (status == SN_OK) {
        int error = 0;
#ifdef _WIN32
        int size = sizeof(error);
#else
        socklen_t size = sizeof(error);
#endif
        if (getsockopt(connection->handle, SOL_SOCKET, SO_ERROR, (char *)&error, &size) != 0 || error) {
            status = SN_CONNECTION;
        }
    }
    if (status != SN_OK) { sn_close(connection); return status; }
    *result = connection;
    return SN_OK;
}

static sn_result transfer(sn_socket *socket, uint8_t *buffer, size_t count, int writing)
{
    if (!socket || socket->handle == SN_INVALID_HANDLE) { return SN_NOT_CONNECTED; }
    while (count) {
        sn_result status = wait_socket(socket, writing);
        if (status != SN_OK) { return status; }
        const int requested = (int)(count > INT_MAX ? INT_MAX : count);
#ifdef _WIN32
        const int transferred = writing ? send(socket->handle, (const char *)buffer, requested, 0)
                                        : recv(socket->handle, (char *)buffer, requested, 0);
#else
        const int transferred = (int)(writing ? send(socket->handle, buffer, (size_t)requested, MSG_NOSIGNAL)
                                              : recv(socket->handle, buffer, (size_t)requested, 0));
#endif
        if (transferred < 0 && would_block()) { continue; }
        if (transferred <= 0) { sn_abort(socket); return SN_CONNECTION; }
        buffer += transferred;
        count -= (size_t)transferred;
    }
    return SN_OK;
}

sn_result sn_send_all(sn_socket *socket, const uint8_t *data, size_t count)
{
    // send() does not mutate its buffer; shared iteration keeps both paths identical.
    return transfer(socket, (uint8_t *)data, count, 1);
}

sn_result sn_receive_all(sn_socket *socket, uint8_t *data, size_t count)
{
    return transfer(socket, data, count, 0);
}

uint16_t sn_free_loopback_port(void)
{
    if (!startup()) { return 0; }
    uint16_t port = 0;
    sn_handle handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle != SN_INVALID_HANDLE) {
        struct sockaddr_in address = {0};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        if (bind(handle, (const struct sockaddr *)&address, sizeof(address)) == 0
            && getsockname(handle, (struct sockaddr *)&address, &size) == 0) {
            port = ntohs(address.sin_port);
        }
        sn_closesocket(handle);
    }
    cleanup();
    return port;
}
