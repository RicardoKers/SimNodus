# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# Example 49: an ESP32-C3 samples two inputs with its ADC and DMA and reads its temperature sensor (ESP-IDF v5.5.2).
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32c3_adc_dma)
