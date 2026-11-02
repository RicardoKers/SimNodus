// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Example 49: ESP-IDF's continuous ADC driver samples GPIO0 (ADC1 channel 0,
// a 50 Hz sine) and GPIO1 (channel 1, a potentiometer) at 20 kHz with DMA,
// 10 kHz each; once a second it prints how many samples each channel got and
// their minimum, maximum and mean in mV, and the chip's temperature from its
// temperature sensor. Output on UART0.
#include <stdio.h>
#include <string.h>

#include "driver/temperature_sensor.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FRAME_BYTES 256 // 64 conversions of 4 bytes

struct stats {
    int count;
    int min_mv;
    int max_mv;
    long long sum_mv;
};

static void reset(struct stats* s)
{
    s->count = 0;
    s->min_mv = 1 << 30;
    s->max_mv = -1;
    s->sum_mv = 0;
}

void app_main(void)
{
    adc_continuous_handle_t adc = NULL;
    const adc_continuous_handle_cfg_t handle_config = {
        .max_store_buf_size = 4 * FRAME_BYTES,
        .conv_frame_size = FRAME_BYTES,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_config, &adc));
    adc_digi_pattern_config_t pattern[2];
    for (int i = 0; i < 2; ++i) {
        pattern[i] = (adc_digi_pattern_config_t){
            .atten = ADC_ATTEN_DB_12,
            .channel = i, // GPIO0, GPIO1
            .unit = ADC_UNIT_1,
            .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
        };
    }
    const adc_continuous_config_t config = {
        .pattern_num = 2,
        .adc_pattern = pattern,
        .sample_freq_hz = 20000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_ERROR_CHECK(adc_continuous_config(adc, &config));
    adc_cali_handle_t cali = NULL;
    const adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &cali));

    temperature_sensor_handle_t sensor = NULL;
    const temperature_sensor_config_t sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ESP_ERROR_CHECK(temperature_sensor_install(&sensor_config, &sensor));
    ESP_ERROR_CHECK(temperature_sensor_enable(sensor));

    printf("Sampling GPIO0 and GPIO1 at 20 kHz with DMA\n");
    ESP_ERROR_CHECK(adc_continuous_start(adc));
    struct stats channels[2];
    reset(&channels[0]);
    reset(&channels[1]);
    static uint8_t frame[FRAME_BYTES];
    int64_t next = esp_timer_get_time() + 1000000;
    for (;;) {
        uint32_t length = 0;
        if (adc_continuous_read(adc, frame, sizeof(frame), &length, 100) == ESP_OK) {
            for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= length; i += SOC_ADC_DIGI_RESULT_BYTES) {
                const adc_digi_output_data_t* d = (const adc_digi_output_data_t*)&frame[i];
                const int channel = d->type2.channel;
                if (d->type2.unit != 0 || channel > 1) {
                    continue;
                }
                int mv = 0;
                adc_cali_raw_to_voltage(cali, d->type2.data, &mv);
                struct stats* s = &channels[channel];
                s->count++;
                s->sum_mv += mv;
                s->min_mv = mv < s->min_mv ? mv : s->min_mv;
                s->max_mv = mv > s->max_mv ? mv : s->max_mv;
            }
        }
        if (esp_timer_get_time() >= next) {
            next += 1000000;
            for (int ch = 0; ch < 2; ++ch) {
                const struct stats* s = &channels[ch];
                printf("GPIO%d: %d samples, min %d mV, max %d mV, mean %d mV\n", ch, s->count,
                    s->count ? s->min_mv : 0, s->count ? s->max_mv : 0,
                    s->count ? (int)(s->sum_mv / s->count) : 0);
            }
            float celsius = 0;
            ESP_ERROR_CHECK(temperature_sensor_get_celsius(sensor, &celsius));
            printf("Chip temperature: %.1f C\n", celsius);
            reset(&channels[0]);
            reset(&channels[1]);
        }
    }
}
