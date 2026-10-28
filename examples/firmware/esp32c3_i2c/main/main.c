// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Example 46: ESP-IDF's I2C master driver on I2C0 (SCL GPIO5, SDA GPIO4,
// 100 kHz). The program looks for an LM75 temperature sensor (0x48) and a
// 24C02 EEPROM (0x50), writes "SimNodus" to the EEPROM and reads it back,
// then prints the temperature every second on UART0.
#include <stdio.h>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_dev_handle_t add_device(i2c_master_bus_handle_t bus, uint16_t address)
{
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t device = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &config, &device));
    return device;
}

void app_main(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_4,
        .scl_io_num = GPIO_NUM_5,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));

    printf("LM75 at 0x48: %s\n", i2c_master_probe(bus, 0x48, 100) == ESP_OK ? "found" : "no answer");
    printf("24C02 at 0x50: %s\n", i2c_master_probe(bus, 0x50, 100) == ESP_OK ? "found" : "no answer");
    i2c_master_dev_handle_t sensor = add_device(bus, 0x48);
    i2c_master_dev_handle_t eeprom = add_device(bus, 0x50);

    // One 8-byte page at address 0, then the 5 ms write cycle.
    const uint8_t page[9] = {0x00, 'S', 'i', 'm', 'N', 'o', 'd', 'u', 's'};
    if (i2c_master_transmit(eeprom, page, sizeof page, 100) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        const uint8_t address = 0x00;
        char text[9] = {0};
        if (i2c_master_transmit_receive(eeprom, &address, 1, (uint8_t*)text, 8, 100) == ESP_OK) {
            printf("EEPROM: %s\n", text);
        }
    }

    for (;;) {
        const uint8_t temperature_register = 0x00;
        uint8_t data[2] = {0};
        if (i2c_master_transmit_receive(sensor, &temperature_register, 1, data, 2, 100) == ESP_OK) {
            // 9 bits in 0.5 C steps, left aligned.
            const int tenths = ((int16_t)((data[0] << 8) | data[1]) >> 7) * 5;
            printf("Temperature: %s%d.%d C\n", tenths < 0 ? "-" : "", (tenths < 0 ? -tenths : tenths) / 10,
                (tenths < 0 ? -tenths : tenths) % 10);
        } else {
            printf("Temperature: no answer\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
