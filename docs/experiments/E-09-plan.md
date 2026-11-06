# E-09: Wi-Fi for the ESP32-C3 — plan

Date: 2026-11-05. Task: SN-250 in the [backlog](../planning/BACKLOG.md), split
into SN-264..SN-269 below. Status: **in progress**. Decisions DE9a-DE9c, the prerequisites (SN-264),
the PHY start-up (SN-265) and the MAC with the air (SN-266) were done on
2026-11-05: a scan finds SimNodus's access point. SN-267 was done on
2026-11-06: stations join open and WPA2-PSK networks and get an address by
DHCP. The gateway (SN-268) is next. Results:
[E-09-results.md](E-09-results.md). As for [E-07](E-07-results.md),
[E-08](E-08-plan.md) and [E-10](E-10-plan.md), the criteria below are recorded
before any code and are not relaxed after a run: a failed criterion leads to
a recorded decision.

## Question

Can SimNodus's own ESP32-C3 emulator run the **unmodified** Wi-Fi libraries
inside ESP-IDF, Arduino-ESP32 and MicroPython images, so that:

- a board scans and joins a virtual access point that SimNodus provides (open
  first, then WPA2-PSK);
- the board gets an address by DHCP and resolves names;
- with the owner's permission per project, it reaches the computer's network
  through a gateway that SimNodus runs in user space;
- a web server on the board is reachable from the computer's browser;

and all this fast enough for lessons, without changing anything for boards
that do not use Wi-Fi?

## Why it is hard

- **The radio hardware is undocumented.** The Technical Reference Manual has
  no chapter for the Wi-Fi MAC, the baseband (BB, NRX) or the RF front end
  (FE, FE2). ESP-IDF's headers only give their base addresses
  (`DR_REG_FE2_BASE` 0x60005000, `DR_REG_FE_BASE` 0x60006000,
  `DR_REG_NRX_BASE` 0x6001CC00, `DR_REG_BB_BASE` 0x6001D000) and the interrupt
  sources (`ETS_WIFI_MAC_INTR_SOURCE`, `_NMI_`, `_PWR_`, `_BB_`).
- **The libraries are closed.** `libnet80211.a`, `libpp.a`, `libcore.a` and
  `libphy.a` are binary blobs. They are Apache-2.0, so images that contain
  them may be run and used as test material. Their register sequences change
  between ESP-IDF versions.
- **Intercepting functions is ruled out** (E-08 plan). Release images have no
  symbols, and the libraries' internals change between versions. So the model
  works at the register and DMA-descriptor level, like the rest of the chip.

## Probe (2026-11-05, before this plan)

The official MicroPython v1.29.0 image (ESP-IDF v5.5.2) was run in
`simnodus-emu --esp32c3` with `--trace-io`, typing `import network`,
`w = network.WLAN(network.STA_IF)` and `w.active(True)` on USB Serial/JTAG:

- The REPL accepts the first two lines.
- `w.active(True)` never returns (9 s of virtual time were observed).
- The PHY start-up writes the analog I2C master (`RTC_I2C`, 0x6000E000), FE
  and APB_SARADC (power detector). It then waits forever in the ROM's
  `rom_i2cmst_reg_init`, which reads 0x6000E050 about 60 million times.
- The MAC registers are never reached.

The first obstacle is therefore the PHY and RF calibration path, before any
MAC work. Speed for comparison: the same run, idle at the REPL, reaches
42.7 MIPS (about 0.27x real time at 160 MHz).

## Oracles and sources

- **Espressif's QEMU** (`esp_develop_9.2.2_20250817`, pinned in E-08) has no
  Wi-Fi model for `esp32c3`: its only NIC is `open_eth`. It is no oracle here.
- **lcgamboa's QEMU fork** (GPL) emulates an open access point for the ESP32
  and, since 2023, the ESP32-C3. If it builds, it may serve as a test-only
  oracle under the clean-room rule of E-08: a separate process, behaviour
  observed through its traces, source never read while the matching model is
  written, never linked or copied.
- **esp32-open-mac** (the original ESP32, Xtensa) documents reverse-engineered
  MAC registers, TX slots and RX DMA. Its QEMU fork brings up a station
  against an emulated AP. The arXiv paper 2501.17684 ports that work to the
  ESP32-C3 Wi-Fi driver. Both may be read as documentation. Their code is used
  only if its licence is permissive, with notice (checked in SN-264).
- **The owner's ESP32-C3-DevKitM-1 and a real access point** (a router or a
  phone hotspot) are the final reference: console output, the time to
  connect, reason codes, DHCP. A capture of the air (Wireshark on an adapter
  in monitor mode) is optional.
- **Our own traces.** `--trace-io` names every access to an unmodelled
  register, which drives each step as in E-08.

## Scope

**In:**

- The PHY and RF calibration path, modelled until the libraries accept it.
- The Wi-Fi MAC: TX slots with DMA descriptors, RX DMA, interrupts, the TSF
  timer, ACKs and retries, and the key slots for CCMP.
- A virtual radio medium in the session: frames between nodes on a channel,
  in virtual time, with an approximate airtime.
- A virtual access point in SimNodus's own code: beacons, probe responses,
  authentication and association; open, then WPA2-PSK with the 4-way
  handshake; a DHCP server and a DNS forwarder.
- A user-space NAT gateway to the computer's network, off by default and
  enabled per project.
- Port forwarding from the computer to a board.
- Several boards on one access point.
- Station mode in ESP-IDF, Arduino `WiFi` and MicroPython `network.WLAN`.

**Out (later, after DE9):**

- The board as an access point (SoftAP).
- ESP-NOW and WPA3-SAE.
- Real radio conditions: RSSI is a property, and there is no propagation
  model.
- Bluetooth LE (not planned).
- A bridge to the computer's real Wi-Fi adapter.

## Prerequisites (SN-264, before emulator code)

1. **Pinned firmware.** The Wi-Fi libraries of ESP-IDF v5.5.2: the
   `esp32-wifi-lib` commit recorded from `components/esp_wifi/lib`, as used by
   MicroPython v1.29.0 and Arduino-ESP32 3.3.12. Owned test images, each with
   its SHA-256 recorded:
   - ESP-IDF `wifi/getting_started/station` and `scan`;
   - `protocols/http_server/simple`;
   - `protocols/sntp`;
   - a UDP echo;
   - an Arduino `WiFiClient` sketch and a `WebServer` sketch;
   - MicroPython `network` and `socket` scripts.

   Images from ESP-IDF v6.x follow only after v5.5.2 passes.
2. **Licences.** Check the licences of esp32-open-mac, of the arXiv paper's
   code and of lcgamboa's fork; record what may be read, what may be reused
   and what may only be run.
3. **Crypto (decision DE9b).** WPA2 needs SHA-1, HMAC-SHA1, PBKDF2, the
   802.11 PRF, AES key wrap and AES-CCMP, each checked against the
   IEEE 802.11 and NIST test vectors. Option 1 is our own code (`sha.cpp`
   exists already). Option 2 is Mbed TLS (Apache-2.0).
4. **TCP/IP for the gateway (decision DE9b).** The gateway ends the board's
   TCP and UDP and opens operating-system sockets. Option 1 is libslirp
   (BSD-3/MIT, used by QEMU; every file's licence to be checked, and whether
   it builds with MSVC). Option 2 is lwIP (BSD-3). Option 3 is our own minimal TCP.
   This is recorded in an ADR.
5. **Network policy (decision DE9c).** Opening a project never opens the
   network:
   - The gateway is off by default.
   - The owner of the computer turns it on per project, with a warning that
     the firmware gets the computer's network.
   - Forwarded ports listen on 127.0.0.1 only, unless the user chooses
     otherwise.
   - Nothing is downloaded, and nothing listens while the simulation is
     stopped.

## Decisions (2026-11-05)

The owner accepted the recommended defaults on 2026-11-05 and asked to start
SN-264. DE9b's two choices were worked out in SN-264, after that answer; they
may still change before the package that needs them (crypto in SN-267, TCP/IP
in SN-268).

- **DE9a, scope and order.** Station mode first. SoftAP and ESP-NOW come
  after DE9. E-09 starts now; the real-chip comparisons of the AVR and
  ESP32-C3 tracks are done when the chips and boards are at hand.
- **DE9b, libraries.**
  - **Crypto:** our own code (SHA-1, HMAC-SHA1, PBKDF2, the 802.11 PRF, AES,
    AES key wrap, CCMP), checked against the IEEE 802.11 and NIST vectors.
    It is small, and `src/emu/sha.cpp` exists already. Mbed TLS (Apache-2.0
    option of its dual licence) is the fallback.
  - **TCP/IP:** **lwIP** (BSD-3-Clause, no dependencies, builds with MSVC and
    GCC). The tun2socks gateways end a device's TCP the same way. libslirp is
    rejected because it needs GLib (LGPL-2.1, built with Meson). Our own TCP
    is rejected as too risky. The lwIP release is pinned and vendored in
    `third_party/lwip` with SN-268.
- **DE9c, network policy.**
  - The gateway is off by default. Turning it on is a project property, with
    a warning that the firmware gets the computer's network.
  - The first run of such a project on a computer asks for confirmation. The
    answer is kept in the local settings, never in the project file, so a
    project received from someone else does not get the network just by being
    opened and run.
  - When on, the gateway reaches what the computer reaches: the internet, the
    LAN, and the computer itself under the name `host.simnodus`.
  - Forwarded ports listen on 127.0.0.1 only, unless the user picks another
    address. Nothing listens while the simulation is stopped, and nothing is
    downloaded.

## Prerequisite results (SN-264, 2026-11-05)

**Pinned libraries.** ESP-IDF v5.5.2 with `components/esp_wifi/lib`
(esp32-wifi-lib) at commit `01d52d9e69032c486015dc28b08c3bf6aaf348a9`
(2025-12-24). PHY library `phy_version 1232,d493f299,Aug 25 2025`, as the
images print. Arduino-ESP32 3.3.12 and MicroPython v1.29.0 are on ESP-IDF
v5.5.x.

**Test images.** These are in `tests/experiments/esp32c3/firmware`, with
their SHA-256 and build settings in the
[test material README](../../tests/experiments/esp32c3/README.md):

- six ESP-IDF v5.5.2 examples: `wifi/scan`, `getting_started/station` (open
  and WPA2-PSK), `http_server/simple`, `sntp` and `sockets/udp_server`;
- two Arduino sketches of ours: a `WiFiClient` and a `WebServer`;
- three MicroPython scripts of ours, for the MicroPython image already there.

The test access point is `SimNodus` with password `simnodus-wifi` (WPA2-PSK),
and `SimNodus-open` (open).

**Baseline.** Every ESP-IDF image boots, prints `wifi_init` and
`phy_init: phy_version 1232`, then
`failed to load RF calibration data (0x1102), falling back to full calibration`,
and stays in the full calibration; 3 s of virtual time were observed. Both
Arduino sketches stop after `WiFi.begin` in the same place. The first
obstacle is the PHY calibration for all three frameworks, as the MicroPython
probe showed.

**Licences.**

| Source | Licence | Use in E-09 |
|---|---|---|
| esp32-wifi-lib blobs inside the images | Apache-2.0 (Espressif) | Run as test material |
| esp32-open-mac (`esp32-open-mac/esp32-open-mac`) | MIT; its licence README states that the code is a clean rewrite and reverse engineering of the Apache-2.0 blobs | May be read and reused, with its notice |
| esp32-open-mac's QEMU fork | GPL-2.0 (QEMU) | Read its documentation and observe its behaviour; never read its model's source while writing ours; never copy it |
| lcgamboa's QEMU fork | GPL-2.0 (QEMU) | Test-only oracle under the same rule; built only if a step needs it |
| arXiv 2501.17684 | Paper | Read as documentation |
| lwIP | BSD-3-Clause | Vendored for the gateway (SN-268) |
| libslirp | BSD-3-Clause, but needs GLib (LGPL-2.1) | Not used |
| Mbed TLS | Apache-2.0 OR GPL-2.0-or-later | Fallback for crypto, under Apache-2.0 |

## Work packages

| # | Package | Done when |
|---|---|---|
| 1 | SN-264 prerequisites: pinned images, licences, DE9b and DE9c taken | Recorded in this plan (done 2026-11-05) |
| 2 | SN-265 PHY and RF start-up: the analog I2C master's status bits, FE/FE2, BB/NRX, power detector, calibration read-backs, PHY data in NVS; diagnostics for every new unmodelled access | `esp_wifi_init` and `esp_wifi_start` return `ESP_OK` → part of **G9a** (done 2026-11-05) |
| 3 | SN-266 MAC core: TX slots and DMA descriptors, RX DMA ring, interrupts (MAC, PWR, BB), TSF, ACK and retry, the radio medium with channels; a frame trace exportable as pcap for Wireshark | A scan lists the virtual AP → **G9a** (done 2026-11-05 for ESP-IDF and MicroPython) |
| 4 | SN-267 virtual access point: open, then WPA2-PSK (hardware CCMP key slots in the MAC model); DHCP, DNS; reason codes; the AP's settings in the project | Association and DHCP → **G9b** (done 2026-11-06 for open and WPA2-PSK; the board comparisons wait for the owner) |
| 5 | SN-268 gateway and port forwarding: NAT through the library of DE9b; policy of DE9c; several boards | **G9c**, **G9d** |
| 6 | SN-269 integration and report: the AP and gateway in the GUI and `simnodus-cli`/MCP (`wifi.*` commands), Wi-Fi examples, user guide (English and pt-BR), `E-09-results.md` | **G9e**, decision **DE9** |

## Gate G9a — radio up

| ID | Criterion |
|---|---|
| G9a.1 | Unmodified images (ESP-IDF `scan`, Arduino `WiFi.scanNetworks()`, MicroPython `WLAN.active(True)` then `scan()`) start the Wi-Fi without errors on the console, and the scan lists the virtual AP with its SSID, channel, authentication mode and the RSSI set in its properties |
| G9a.2 | Every register access on this path is modelled or listed in the report with its reason; the run shows no unmodelled-access diagnostic that is not listed |
| G9a.3 | Start-up of the Wi-Fi (`esp_wifi_start` to `WIFI_EVENT_STA_START`) under 2 s of wall-clock time on the owner's laptop |

## Gate G9b — association and address

| ID | Criterion |
|---|---|
| G9b.1 | The ESP-IDF `station` example joins an open AP and a WPA2-PSK AP, and gets an address from SimNodus's DHCP (`IP_EVENT_STA_GOT_IP`); so do the Arduino sketch and MicroPython (`isconnected()`, `ifconfig()`) |
| G9b.2 | A wrong password fails with the reason code the board gives against a real AP (recorded with the owner's board); an AP turned off and on again during the run is rejoined by the ESP-IDF example's retry logic |
| G9b.3 | The ESP-IDF station example's console output equals the board's against a real AP with the same SSID and security, except lines with times, MAC addresses, RSSI or addresses (listed in the report) |
| G9b.4 | WPA2 crypto passes the IEEE 802.11 test vectors (PBKDF2 PSK, PRF, CCMP) in ctest |

## Gate G9c — the computer's network

| ID | Criterion |
|---|---|
| G9c.1 | With the gateway on, an HTTP GET from the board reaches a test server that ctest starts on 127.0.0.1, and the reply reaches the board's console; DNS resolves the test server's name; a UDP exchange (SNTP against a local test server) works |
| G9c.2 | With the gateway off (the default), the same firmware gets no route and no DNS answer, and SimNodus opens no socket (checked in the test) |
| G9c.3 | By hand with the owner: an HTTP request to a public site and an MQTT client against a broker on the computer |
| G9c.4 | A simulation slower than real time does not break TCP: the board's own TCP timers run on virtual time, and the gateway keeps the host side open (measured at 0.25x real time) |

## Gate G9d — servers and several boards

| ID | Criterion |
|---|---|
| G9d.1 | ESP-IDF `http_server/simple`, an Arduino `WebServer` sketch and a MicroPython socket server answer `curl http://127.0.0.1:<forwarded port>/` and a browser on the computer |
| G9d.2 | Two boards on one AP exchange UDP datagrams and open a TCP connection to each other |

## Gate G9e — fidelity and no regression

| ID | Criterion |
|---|---|
| G9e.1 | With the gateway off, two runs of the same images with the same inputs give identical traces (hash), frames included; with the gateway on, determinism is not claimed, and the user guide says so |
| G9e.2 | A connected MicroPython board idle at the REPL keeps at least 80 % of the MIPS of the same board without Wi-Fi |
| G9e.3 | Timing labels: airtime, rates and RSSI are labelled approximate in the part and in the guide |
| G9e.4 | Nothing changes for other boards: every ctest suite and `tools/examples_pass.py` pass unchanged; `emu-benchmark` within 3 %; a project without Wi-Fi starts no radio medium, AP or gateway |

## Outcomes (decision DE9)

- **G9a-G9d pass:** an ADR for Wi-Fi on the own emulator. The Wi-Fi ships
  marked experimental until G9b.3 holds against the owner's board.
- **G9a fails twice** (the PHY or MAC path cannot be found from traces and
  the public reverse engineering): the experiment stops with its measured
  reasons. The owner then chooses between waiting, a different chip, or a
  documented high-level emulation, which this plan rejects today.
- **Only G9e.2 or G9a.3 fail (speed):** one more week for performance, then
  a review.

## Decisions for the owner before code (taken 2026-11-05, see above)

- **DE9a: scope and order.** Station first; SoftAP and ESP-NOW after DE9.
  Is this the right priority against the real-chip comparisons of the AVR and
  ESP32-C3 tracks?
- **DE9b: libraries.** Our own crypto or Mbed TLS; libslirp, lwIP or our own
  TCP for the gateway.
- **DE9c: network policy.** As in prerequisite 5. Also: may the gateway reach
  the LAN as well as the internet? Is a confirmation needed the first time
  per project?

## Risks

1. **Undocumented hardware** (the largest risk). The libraries may take
   paths that depend on PHY read-backs, which have no public description.
   Mitigation: traces, the public reverse engineering, the board as the final
   check, and the stop rule of DE9.
2. **Timing inside the MAC.** ACKs within SIFS, beacon and TSF
   synchronization and power save (DTIM, TIM) must be consistent in virtual
   time. The libraries may check them.
3. **Speed.** MicroPython already runs at about 0.27x real time while idle,
   and the Wi-Fi tasks and the TSF interrupts add load. The board's TCP timers
   are safe on virtual time; remote servers may time out idle connections.
4. **Security.** A gateway gives firmware network access. This is handled by
   DE9c's defaults; forwarded ports are local only.
5. **Maintenance.** New ESP-IDF versions may change the register sequences.
   Each supported version is pinned and tested.
6. **Licences** of the reverse-engineering sources and of the TCP/IP library
   (SN-264).

## References

- ESP32-C3 Technical Reference Manual; ESP-IDF v5.5.2 sources (Apache-2.0):
  `components/esp_wifi`, `components/esp_phy`,
  `components/soc/esp32c3/register/soc/reg_base.h`,
  `components/soc/esp32c3/include/soc/regi2c_defs.h`.
- IEEE 802.11-2020 (frames, 4-way handshake, CCMP and its test vectors).
- ESP32 open MAC: https://esp32-open-mac.be/,
  https://github.com/esp32-open-mac/qemu
- *Reverse Engineering the ESP32-C3 Wi-Fi Drivers for Static Worst-Case
  Analysis of Intermittently-Powered Systems*, arXiv 2501.17684.
- lcgamboa's QEMU fork (ESP32 and ESP32-C3 Wi-Fi AP emulation; GPL,
  test-only): https://github.com/lcgamboa/qemu
- libslirp: https://gitlab.freedesktop.org/slirp/libslirp; lwIP:
  https://savannah.nongnu.org/projects/lwip/
