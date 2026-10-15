// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// FreeRTOS configuration of the freertos_tasks example: STM32F103 at its reset
// clock (HSI 8 MHz), 1 kHz tick, 4 priority bits. Every task switch writes the
// new task's tag to PB0-PB2, so the switches show up in the Logic tab.
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCPU_CLOCK_HZ 8000000u
#define configTICK_RATE_HZ 1000u
#define configMAX_PRIORITIES 5
#define configMINIMAL_STACK_SIZE 128
#define configTOTAL_HEAP_SIZE (8 * 1024)
#define configMAX_TASK_NAME_LEN 12
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configQUEUE_REGISTRY_SIZE 0
#define configUSE_TIMERS 0
#define configUSE_CO_ROUTINES 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configUSE_APPLICATION_TASK_TAG 1

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelayUntil 1

// Cortex-M3 interrupt priorities (4 bits implemented on the STM32F103).
#define configPRIO_BITS 4
#define configKERNEL_INTERRUPT_PRIORITY (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))

#define configASSERT(x)                                                                                     \
    if ((x) == 0) {                                                                                         \
        __asm volatile("cpsid i");                                                                          \
        for (;;) {                                                                                          \
        }                                                                                                   \
    }

// The port's handlers under the names in the vector table.
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

// Task switch trace: the new task's tag (1..3, idle 0) on PB0-PB2.
#define traceTASK_SWITCHED_IN()                                                                             \
    (*(volatile uint32_t*)0x40010C10u = (0x7u << 16) | ((uint32_t)(uintptr_t)pxCurrentTCB->pxTaskTag & 0x7u))

#endif
