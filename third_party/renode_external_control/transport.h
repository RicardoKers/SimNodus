// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// IPv4 loopback transport for the Renode external control client. Every
// client operation arms one deadline (sn_begin) shared by all of its bytes;
// a timeout or socket error retires the stream (later calls report
// SN_NOT_CONNECTED). Windows (Winsock) and POSIX.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sn_socket sn_socket;
typedef enum { SN_OK, SN_CONNECTION, SN_TIMEOUT, SN_NOT_CONNECTED, SN_INVALID } sn_result;

enum { SN_DEFAULT_TIMEOUT_MS = 1000 };

// Connects to 127.0.0.1:port (decimal text) within SN_DEFAULT_TIMEOUT_MS.
sn_result sn_open(const char *port, sn_socket **result);
// Deadline for each later operation; 0 restores the default.
void sn_set_timeout(sn_socket *socket, uint32_t milliseconds);
sn_result sn_begin(sn_socket *socket);
sn_result sn_send_all(sn_socket *socket, const uint8_t *data, size_t count);
sn_result sn_receive_all(sn_socket *socket, uint8_t *data, size_t count);
void sn_abort(sn_socket *socket);
void sn_close(sn_socket *socket);

// A loopback port that was free a moment ago (0 on failure). Another process
// may take it before Renode binds it; the caller detects that at connect.
uint16_t sn_free_loopback_port(void);

#ifdef __cplusplus
}
#endif
