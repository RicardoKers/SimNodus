// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Fault test for SimNodus's ESP32-C3 machine (experiment E-08, gate G8b.6):
// a character on the console picks the fault: 'i' an illegal instruction,
// 'l' a load from an address with nothing mapped, 'a' abort().
#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    printf("fault test: send i, l or a\n");
    for (;;) {
        const int c = getchar();
        if (c == 'i') {
            printf("illegal instruction\n");
            __asm__ volatile(".word 0x00000000"); // the all-zero halfword is illegal
        } else if (c == 'l') {
            printf("load fault\n");
            volatile int* nothing = (volatile int*)0x10;
            printf("%d\n", *nothing);
        } else if (c == 'a') {
            printf("abort\n");
            abort();
        }
        vTaskDelay(1);
    }
}
