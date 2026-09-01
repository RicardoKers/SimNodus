// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// The host owns command, channel, and sample_setting. Firmware owns all results.
typedef struct {
    uint32_t magic;
    uint32_t command;
    uint32_t acknowledged;
    uint32_t channel;
    uint32_t sample_setting;
    uint32_t started;
    uint32_t completed;
    uint32_t result;
    uint32_t status_before_read;
    uint32_t fault;
    uint32_t reserved0;
    uint32_t reserved1;
} sn_e04_mailbox;

#define SN_E04_MAILBOX_ADDRESS 0x20000000u
#define SN_E04_BOOT_MAGIC 0x534e3034u
