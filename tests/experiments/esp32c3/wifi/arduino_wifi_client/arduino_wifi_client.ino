// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Arduino-ESP32 Wi-Fi test sketch for SimNodus (E-09, gates G9b and G9c):
// joins the access point "SimNodus" (WPA2-PSK), prints its address, then
// every 5 s fetches http://host.simnodus/ through the gateway and prints the
// status line and the first line of the body.
#include <WiFi.h>

static const char* ssid = "SimNodus";
static const char* password = "simnodus-wifi";
static const char* host = "host.simnodus"; // the computer, as SimNodus's DNS names it

void setup() {
  Serial.begin(115200);
  Serial.println("wifi client: connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }
  Serial.print("wifi client: address ");
  Serial.println(WiFi.localIP());
}

void loop() {
  WiFiClient client;
  if (!client.connect(host, 80)) {
    Serial.println("wifi client: no connection");
  } else {
    client.printf("GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host);
    const String status = client.readStringUntil('\n');
    Serial.print("wifi client: ");
    Serial.println(status);
    while (client.connected() && client.readStringUntil('\n') != "\r") {
    }
    Serial.print("wifi client: body ");
    Serial.println(client.readStringUntil('\n'));
    client.stop();
  }
  delay(5000);
}
