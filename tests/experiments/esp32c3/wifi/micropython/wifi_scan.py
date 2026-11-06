# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# E-09, gate G9a: start the station interface and list the access points.
import network

w = network.WLAN(network.STA_IF)
w.active(True)
for ssid, bssid, channel, rssi, security, hidden in w.scan():
    print("scan:", ssid.decode(), channel, rssi, security)
