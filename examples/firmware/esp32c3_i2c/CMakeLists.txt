# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# Example 46: an ESP32-C3 reads an LM75 and writes a 24C02 on I2C0 (ESP-IDF v5.5.2).
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32c3_i2c)
