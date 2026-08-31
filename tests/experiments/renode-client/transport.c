// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Native IPv4 loopback only, with one deadline per complete client operation.
#include <winsock2.h>
#include <windows.h>
#include <limits.h>
#include <stdlib.h>
#include "transport.h"

struct sn_socket {
    SOCKET handle;
    ULONGLONG deadline;
};

void sn_abort(sn_socket *socket)
{
    if (socket && socket->handle != INVALID_SOCKET) {
        closesocket(socket->handle);
        socket->handle = INVALID_SOCKET;
    }
}

void sn_close(sn_socket *socket)
{
    if (socket) {
        sn_abort(socket);
        free(socket);
        WSACleanup();
    }
}

sn_result sn_begin(sn_socket *socket)
{
    if (!socket || socket->handle == INVALID_SOCKET) { return SN_NOT_CONNECTED; }
    socket->deadline = GetTickCount64() + SN_TIMEOUT_MS;
    return SN_OK;
}

static sn_result wait_socket(sn_socket *socket, int writing)
{
    if (!socket || socket->handle == INVALID_SOCKET) { return SN_NOT_CONNECTED; }
    const ULONGLONG now = GetTickCount64();
    if (now >= socket->deadline) { sn_abort(socket); return SN_TIMEOUT; }
    const long remaining = (long)(socket->deadline - now);
    struct timeval timeout = { remaining / 1000, (remaining % 1000) * 1000 };
    fd_set ready, errors;
    FD_ZERO(&ready); FD_ZERO(&errors);
    FD_SET(socket->handle, &ready); FD_SET(socket->handle, &errors);
    const int count = select(0, writing ? NULL : &ready, writing ? &ready : NULL, &errors, &timeout);
    if (count == 0) { sn_abort(socket); return SN_TIMEOUT; }
    if (count == SOCKET_ERROR || FD_ISSET(socket->handle, &errors)) {
        sn_abort(socket); return SN_CONNECTION;
    }
    return SN_OK;
}

sn_result sn_open(const char *port, sn_socket **result)
{
    if (!result) { return SN_INVALID; }
    *result = NULL;
    unsigned number = 0;
    if (!port || !*port) { return SN_INVALID; }
    for (const char *p = port; *p; ++p) {
        if (*p < '0' || *p > '9' || number > 6553) { return SN_INVALID; }
        number = number * 10 + (unsigned)(*p - '0');
        if (number > 65535) { return SN_INVALID; }
    }
    if (number == 0) { return SN_INVALID; }
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) { return SN_CONNECTION; }
    sn_socket *connection = calloc(1, sizeof(*connection));
    if (!connection) { WSACleanup(); return SN_CONNECTION; }
    connection->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection->handle == INVALID_SOCKET) { sn_close(connection); return SN_CONNECTION; }
    u_long nonblocking = 1;
    const BOOL no_delay = TRUE;
    if (ioctlsocket(connection->handle, FIONBIO, &nonblocking) == SOCKET_ERROR
        || setsockopt(connection->handle, IPPROTO_TCP, TCP_NODELAY,
                      (const char *)&no_delay, sizeof(no_delay)) == SOCKET_ERROR) {
        sn_close(connection); return SN_CONNECTION;
    }
    sn_begin(connection);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((u_short)number);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sn_result status = SN_OK;
    if (connect(connection->handle, (const struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) { status = SN_CONNECTION; }
        else { status = wait_socket(connection, 1); }
    }
    if (status == SN_OK) {
        int error = 0, size = sizeof(error);
        if (getsockopt(connection->handle, SOL_SOCKET, SO_ERROR, (char *)&error, &size) == SOCKET_ERROR || error) {
            status = SN_CONNECTION;
        }
    }
    if (status != SN_OK) { sn_close(connection); return status; }
    *result = connection;
    return SN_OK;
}

static sn_result transfer(sn_socket *socket, uint8_t *buffer, size_t count, int writing)
{
    if (!socket || socket->handle == INVALID_SOCKET) { return SN_NOT_CONNECTED; }
    while (count) {
        sn_result status = wait_socket(socket, writing);
        if (status != SN_OK) { return status; }
        const int requested = (int)(count > SN_TRANSFER_LIMIT ? SN_TRANSFER_LIMIT : count);
        const int transferred = writing
            ? send(socket->handle, (const char *)buffer, requested, 0)
            : recv(socket->handle, (char *)buffer, requested, 0);
        if (transferred == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) { continue; }
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
