// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t ticks;
    uint32_t gpio_changes;
    uint32_t adc_reads;
    uint32_t adc_result;
    uint32_t step_count;
    uint32_t fault;
    uint32_t reserved;
} sn_e05_mailbox;

#define SN_E05_MAILBOX_ADDRESS 0x20000000u
#define SN_E05_BOOT_MAGIC 0x534e3035u
