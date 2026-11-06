# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# E-09, gate G9d: join "SimNodus" (WPA2-PSK) and answer HTTP on port 80.
import network
import socket
import time

w = network.WLAN(network.STA_IF)
w.active(True)
w.connect("SimNodus", "simnodus-wifi")
while not w.isconnected():
    time.sleep_ms(100)
print("server: http://%s/" % w.ifconfig()[0])
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 80))
s.listen(1)
while True:
    c, peer = s.accept()
    c.recv(512)
    c.send(b"HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nHello from MicroPython\n")
    c.close()
