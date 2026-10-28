// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Arduino-ESP32 test sketch for SimNodus's ESP32-C3 machine (E-08, G8b.3):
// Serial on UART0, an output on GPIO8, the time from millis(), and an echo
// of the received characters in capitals.
void setup() {
  Serial.begin(115200);
  pinMode(8, OUTPUT);
  Serial.println("Arduino on ESP32-C3");
}

void loop() {
  static int n = 0;
  digitalWrite(8, n & 1);
  Serial.printf("tick %d at %lu ms\n", n++, millis());
  while (Serial.available()) {
    Serial.write(toupper(Serial.read()));
  }
  delay(250);
}
