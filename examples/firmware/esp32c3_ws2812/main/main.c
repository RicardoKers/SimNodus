// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Example 48: ESP-IDF's RMT driver sends a strip of 8 WS2812B LEDs on GPIO5.
// A bytes encoder at 10 MHz turns each bit into 0.4/0.8 us (0) or
// 0.8/0.4 us (1) pulses, GRB order, most significant bit first. One LED
// runs along the strip every 200 ms, changing colour each lap.
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LEDS 8

void app_main(void)
{
    const rmt_tx_channel_config_t channel_config = {
        .gpio_num = GPIO_NUM_5,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, // 0.1 us per tick
        .mem_block_symbols = 48,
        .trans_queue_depth = 4,
    };
    rmt_channel_handle_t channel = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_config, &channel));
    const rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = {.level0 = 1, .duration0 = 4, .level1 = 0, .duration1 = 8},
        .bit1 = {.level0 = 1, .duration0 = 8, .level1 = 0, .duration1 = 4},
        .flags.msb_first = 1,
    };
    rmt_encoder_handle_t encoder = NULL;
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&encoder_config, &encoder));
    ESP_ERROR_CHECK(rmt_enable(channel));

    // Colours as red, green, blue; the strip takes green, red, blue.
    static const uint8_t colours[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    const rmt_transmit_config_t transmit = {.loop_count = 0};
    uint8_t grb[LEDS * 3];
    for (int step = 0;; ++step) {
        const int lit = step % LEDS;
        const uint8_t* rgb = colours[(step / LEDS) % 3];
        memset(grb, 0, sizeof grb);
        grb[3 * lit + 0] = rgb[1];
        grb[3 * lit + 1] = rgb[0];
        grb[3 * lit + 2] = rgb[2];
        ESP_ERROR_CHECK(rmt_transmit(channel, encoder, grb, sizeof grb, &transmit));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(channel, -1));
        printf("LED %d %s\n", lit + 1, rgb[0] ? "red" : rgb[1] ? "green" : "blue");
        vTaskDelay(pdMS_TO_TICKS(200)); // also the strip's reset time (> 50 us low)
    }
}
