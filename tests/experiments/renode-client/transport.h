// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct sn_socket sn_socket;
typedef enum { SN_OK, SN_CONNECTION, SN_TIMEOUT, SN_NOT_CONNECTED, SN_INVALID } sn_result;

sn_result sn_open(const char *port, sn_socket **result);
sn_result sn_begin(sn_socket *socket);
sn_result sn_send_all(sn_socket *socket, const uint8_t *data, size_t count);
sn_result sn_receive_all(sn_socket *socket, uint8_t *data, size_t count);
void sn_abort(sn_socket *socket);
void sn_close(sn_socket *socket);

// Experiment controls, shared by both builds; not a production configuration API.
#define SN_TIMEOUT_MS 1000
#ifndef SN_TRANSFER_LIMIT
#define SN_TRANSFER_LIMIT 2147483647
#endif
