// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Arduino-ESP32 Wi-Fi test sketch for SimNodus (E-09, gate G9d): joins the
// access point "SimNodus" (WPA2-PSK) and serves "/" (a greeting and the LED's
// state) and "/led?on=0|1" (GPIO8) on port 80.
#include <WebServer.h>
#include <WiFi.h>

static const char* ssid = "SimNodus";
static const char* password = "simnodus-wifi";
static const int led = 8;

WebServer server(80);

static void page() {
  server.send(200, "text/plain", String("Hello from SimNodus, LED ") + (digitalRead(led) ? "on" : "off") + "\n");
}

void setup() {
  Serial.begin(115200);
  pinMode(led, OUTPUT);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }
  Serial.print("web server: http://");
  Serial.println(WiFi.localIP());
  server.on("/", page);
  server.on("/led", [] {
    digitalWrite(led, server.arg("on") == "1");
    page();
  });
  server.begin();
}

void loop() {
  server.handleClient();
  delay(2);
}
