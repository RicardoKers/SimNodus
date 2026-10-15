// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Three FreeRTOS tasks on the Blue Pill (8 MHz reset clock, no HAL):
//   blink_a (tag 1): PA0 toggles every 100 ms.
//   blink_b (tag 2): PA1 toggles every 150 ms and sends a counter to a queue.
//   printer (tag 3): waits on the queue and prints "count N" on USART1 (PA9).
// Every task switch writes the running task's tag to PB0-PB2 (0 = idle task),
// so the Logic tab shows the scheduler at work.
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOA_ODR REG(0x4001080Cu)
#define GPIOB_CRL REG(0x40010C00u)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)

static QueueHandle_t counts;

static void put(char c)
{
    while (!(USART1_SR & (1u << 7))) {
    }
    USART1_DR = (uint32_t)c;
}

static void print_count(uint32_t n)
{
    char digits[10];
    int k = 0;
    do {
        digits[k++] = (char)('0' + n % 10u);
        n /= 10u;
    } while (n != 0u);
    for (const char* s = "count "; *s; ++s) {
        put(*s);
    }
    while (k > 0) {
        put(digits[--k]);
    }
    put('\r');
    put('\n');
}

static void blink_a(void* unused)
{
    (void)unused;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(100));
        GPIOA_ODR ^= 1u << 0;
    }
}

static void blink_b(void* unused)
{
    (void)unused;
    TickType_t wake = xTaskGetTickCount();
    uint32_t count = 0;
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(150));
        GPIOA_ODR ^= 1u << 1;
        ++count;
        xQueueSend(counts, &count, 0);
    }
}

static void printer(void* unused)
{
    (void)unused;
    uint32_t count;
    for (;;) {
        if (xQueueReceive(counts, &count, portMAX_DELAY) == pdPASS) {
            print_count(count);
        }
    }
}

int main(void)
{
    RCC_APB2ENR |= (1u << 2) | (1u << 3) | (1u << 14); // GPIOA, GPIOB, USART1
    GPIOA_CRL = (GPIOA_CRL & ~0xFFu) | 0x22u;          // PA0, PA1: outputs, 2 MHz
    GPIOB_CRL = (GPIOB_CRL & ~0xFFFu) | 0x222u;        // PB0-PB2: task trace
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | 0xB0u;          // PA9: USART1 TX
    USART1_BRR = 69;                                   // 115200 baud at 8 MHz
    USART1_CR1 = (1u << 13) | (1u << 3);

    counts = xQueueCreate(4, sizeof(uint32_t));
    TaskHandle_t task;
    xTaskCreate(blink_a, "blink_a", configMINIMAL_STACK_SIZE, NULL, 2, &task);
    vTaskSetApplicationTaskTag(task, (TaskHookFunction_t)1);
    xTaskCreate(blink_b, "blink_b", configMINIMAL_STACK_SIZE, NULL, 2, &task);
    vTaskSetApplicationTaskTag(task, (TaskHookFunction_t)2);
    xTaskCreate(printer, "printer", configMINIMAL_STACK_SIZE, NULL, 1, &task);
    vTaskSetApplicationTaskTag(task, (TaskHookFunction_t)3);
    vTaskStartScheduler();
    for (;;) {
    }
}
