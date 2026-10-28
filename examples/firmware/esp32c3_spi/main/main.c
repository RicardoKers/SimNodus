// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Example 47: ESP-IDF's SPI master driver on SPI2 (SCLK GPIO6, MOSI GPIO7,
// MISO GPIO2, with DMA). An MCP3208 A/D converter on CS GPIO10 (1 MHz) reads
// a potentiometer on CH0; a W25Q32 flash on CS GPIO3 (4 MHz) gives its
// JEDEC ID and keeps a text written to it. Output on UART0.
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static spi_device_handle_t add_device(int cs, int hz)
{
    const spi_device_interface_config_t config = {
        .clock_speed_hz = hz,
        .mode = 0,
        .spics_io_num = cs,
        .queue_size = 1,
    };
    spi_device_handle_t device = NULL;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &config, &device));
    return device;
}

// One full-duplex transfer of `length` bytes.
static void transfer(spi_device_handle_t device, const uint8_t* tx, uint8_t* rx, size_t length)
{
    spi_transaction_t t = {
        .length = 8 * length,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    ESP_ERROR_CHECK(spi_device_transmit(device, &t));
}

static void flash_wait(spi_device_handle_t flash)
{
    uint8_t tx[2] = {0x05, 0x00}; // read status register 1
    uint8_t rx[2] = {0};
    do {
        transfer(flash, tx, rx, 2);
    } while (rx[1] & 0x01); // BUSY
}

void app_main(void)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = GPIO_NUM_6,
        .mosi_io_num = GPIO_NUM_7,
        .miso_io_num = GPIO_NUM_2,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_handle_t adc = add_device(GPIO_NUM_10, 1000000);
    spi_device_handle_t flash = add_device(GPIO_NUM_3, 4000000);

    uint8_t id_tx[4] = {0x9F, 0, 0, 0};
    uint8_t id_rx[4] = {0};
    transfer(flash, id_tx, id_rx, sizeof id_tx);
    printf("Flash JEDEC ID: %02X %02X %02X\n", id_rx[1], id_rx[2], id_rx[3]);

    // Erase the 4 KiB sector at 0, program "SimNodus" at 0x000100, read it back.
    uint8_t enable[1] = {0x06};
    uint8_t rx[16] = {0};
    transfer(flash, enable, rx, 1);
    uint8_t erase[4] = {0x20, 0x00, 0x00, 0x00};
    transfer(flash, erase, rx, sizeof erase);
    flash_wait(flash);
    transfer(flash, enable, rx, 1);
    uint8_t program[12] = {0x02, 0x00, 0x01, 0x00, 'S', 'i', 'm', 'N', 'o', 'd', 'u', 's'};
    transfer(flash, program, rx, sizeof program);
    flash_wait(flash);
    uint8_t read[12] = {0x03, 0x00, 0x01, 0x00};
    transfer(flash, read, rx, sizeof read);
    printf("Flash: %.8s\n", (const char*)&rx[4]);

    for (;;) {
        // Start bit, single-ended, channel 0, then 12 bits of result.
        const uint8_t tx[3] = {0x06, 0x00, 0x00};
        uint8_t result[3] = {0};
        transfer(adc, tx, result, sizeof tx);
        const int code = ((result[1] & 0x0F) << 8) | result[2];
        printf("CH0: %d = %d mV\n", code, code * 3300 / 4096);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
