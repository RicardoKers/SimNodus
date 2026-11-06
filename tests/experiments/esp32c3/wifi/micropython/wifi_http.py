# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
# E-09, gates G9b and G9c: join "SimNodus" (WPA2-PSK), print the address,
# then fetch http://host.simnodus/ through the gateway.
import network
import socket
import time

w = network.WLAN(network.STA_IF)
w.active(True)
w.connect("SimNodus", "simnodus-wifi")
while not w.isconnected():
    time.sleep_ms(100)
print("address:", w.ifconfig()[0])
address = socket.getaddrinfo("host.simnodus", 80)[0][-1]
s = socket.socket()
s.connect(address)
s.send(b"GET / HTTP/1.0\r\nHost: host.simnodus\r\n\r\n")
print("reply:", s.recv(200).split(b"\r\n")[0].decode())
s.close()
