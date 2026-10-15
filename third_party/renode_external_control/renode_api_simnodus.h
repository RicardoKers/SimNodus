// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SimNodus additions to the Renode external control client.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "renode_api.h"

// Deadline in milliseconds for each later operation on this connection
// (default 1000, 0 restores it). A timeout retires the connection.
void renode_set_operation_timeout(renode_t *renode, uint32_t milliseconds);

// UART of the machine by peripheral name ("usart1"), through the SimNodus UART
// command of the loopback server. Bytes the peripheral transmits arrive as
// events during renode_run_for, with Renode's virtual time.
typedef struct renode_uart renode_uart_t;
#pragma pack(push, 1)
typedef struct {
    uint64_t timestamp_us;
    uint8_t byte;
} renode_uart_event_data_t;
#pragma pack(pop)
renode_error_t *renode_get_uart(renode_machine_t *machine, const char *name, renode_uart_t **uart);
// Feeds bytes to the peripheral's receiver (at most 65536).
renode_error_t *renode_uart_write(renode_uart_t *uart, const uint8_t *data, uint32_t count);
renode_error_t *renode_register_uart_callback(renode_uart_t *uart, void *user_data,
    void (*callback)(void *, renode_uart_event_data_t *));

#ifdef __cplusplus
}
#endif
