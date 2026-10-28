# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# Example 47: an ESP32-C3 reads an MCP3208 and programs a W25Q32 on SPI2 (ESP-IDF v5.5.2).
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32c3_spi)
