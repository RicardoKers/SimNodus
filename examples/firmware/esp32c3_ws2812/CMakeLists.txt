# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# Example 48: an ESP32-C3 drives a WS2812B strip with the RMT (ESP-IDF v5.5.2).
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32c3_ws2812)
