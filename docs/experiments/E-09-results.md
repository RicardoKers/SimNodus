# E-09: Wi-Fi for the ESP32-C3 — results

Plan and predeclared gates: [E-09-plan.md](E-09-plan.md). This report grows
with each work package; the gates are reviewed at the end (SN-269, decision
DE9).

## SN-265: PHY and RF start-up (2026-11-05)

**Result.** The station starts on all three frameworks:

- the unmodified ESP-IDF v5.5.2 images;
- the Arduino-ESP32 3.3.12 sketches;
- the official MicroPython v1.29.0 image.

This covers `esp_wifi_init`, the PHY's full calibration, `esp_wifi_start`,
`WIFI_EVENT_STA_START` and `wifi:mode : sta (34:85:18:00:c3:01)`. With no
access point modelled yet, they behave as a board in a room with no networks:

- the ESP-IDF scan reports `Total APs scanned = 0`;
- the station example retries and gives up (`connect to the AP fail`);
- Arduino's `WiFi.begin` reports reason 201, `NO_AP_FOUND`;
- MicroPython's `WLAN.active(True)` returns, and `scan()` gives `[]`.

On the next boot, the calibration saved in the `phy_init` NVS data is loaded
and the full calibration is skipped, as on the board.

**What was modelled.** Every wait was found with `simnodus-emu --trace-io`
and the image's ELF, which keeps libphy's and libpp's symbols, then read in
the disassembly. Each wait reads a status bit, and the model reports it as
finished at once. Only the bits that the libraries wait for are set; nothing
claims a measured value:

| Register | Bits | Waited for by | Meaning (from the code that waits) |
|---|---|---|---|
| `0x6000E050` (analog I2C master) | 26:24 = 7 | `ram_pkdet_vol_start` | peak detector state machine done |
| `0x6000E04C` | 24 | `txdc_cal_v70` | TX DC offset comparator sample ready |
| REGI2C block `0x62`, register 7 | 1 | `wait_rfpll_cal_end` (through `g_phyFuns`) | RF PLL calibration ended; without it, `phy: error: pll_cal exceeds 2ms!!!` is printed |
| FE `0x60006174` | 16 | `ram_iq_est_enable` | IQ estimate ready |
| MAC `0x60033D14` | 0, after the driver sets bit 1 | libpp `hal_init` | MAC ready after its reset |

The radio's blocks are named in the trace (FE, FE2, NRX, BB, WIFI_MAC):
`RadioBlock` in `src/emu/esp32c3.cpp` keeps what is written and adds these
bits.

**Calibration results.** The comparator and estimator outputs read back what
was written, or 0. The calibrations therefore end with values that only fit
an ideal radio. That is accepted for a simulation with no real air, and is
labelled approximate (G9e.3).

**What the start touches** (wifi_scan, from 300 ms to 3 s; distinct
register-and-direction pairs):

| Block | Count |
|---|---|
| WIFI_MAC | 126 |
| FE | 95 |
| NRX | 75 |
| RTC_I2C | 73 |
| BB | 16 |

The MAC also uses `0x60034000` and `0x60035000`. Eight register groups
0x4C apart start at `0x60034100`; these are probably the TX slots, which
SN-266 checks. One page at `0x60011000` (+0x04C, +0x848) still has no name.
These read back what was written; SN-266 models the MAC on them.

**Tests.** ctest `emu-esp32c3` has a new case covering the full calibration,
the station start, the empty scan, the second boot with stored calibration,
the station's failure and MicroPython's `active(True)` and `scan()`.

**Unchanged.**

- Every ctest suite passes.
- `tools/examples_pass.py` passes; examples 45-49 run at 1.45-2.15x real
  time, as before.

**Gate G9a status.**

- G9a.1 is open: the scan works but lists no AP until SN-266 and SN-267.
- G9a.2 waits for the MAC model.
- G9a.3: the first boot, calibration included, reaches `wifi:mode : sta` at
  417 ms of virtual time. The whole 6 s run of `wifi_scan` takes 1.1 s of
  wall-clock time on the owner's laptop, so the start is well under the 2 s
  limit. It is measured exactly with the gate.

**Next.** SN-266: the MAC's TX slots and RX DMA, its interrupts and the TSF,
so that the probe requests of the scan reach a radio medium.

## SN-266: the MAC and the air (2026-11-05)

**Result.** The unmodified Wi-Fi libraries send and receive through the MAC
model. The ESP-IDF `scan` example finds SimNodus's access point on its own:
`Total APs scanned = 1`, SSID `SimNodus`, RSSI -40, channel 6, open. A WPA2
network is reported as `WIFI_AUTH_WPA2_PSK` with CCMP. MicroPython's
`WLAN.scan()` gives `[(b'SimNodus', b'\x02SN\x00\x00\x01', 11, -40, 0, False)]`.

During an active scan the station sends one probe request on each of
channels 1-11, about 120 ms apart, from its address `34:85:18:00:c3:01`. The
access point answers on its channel. The station example then finds the
network and sends a deauthentication and an open-system authentication
(`wifi:state: init -> auth`). No answer comes yet, so it times out after 1 s
and retries, as against a silent AP. Answering is SN-267.

**How the MAC was found.** Sources:

- arXiv 2501.17684 gives the C3's MAC range and roles: RX enable 0x084,
  RX base 0x088, TX 0xD08, interrupt status 0xC3C and clear 0xC40,
  power 0xCA0.
- The disassembly of libpp and the ROM gives the rest, with the function
  names: `hal_mac_txq_enable`, `hal_mac_get_txq_state`,
  `lmacProcessTxComplete`, `hal_mac_rx_*`, `wdevProcessRxSucDataAll`,
  `wDev_ProcessRxSucData`, `wDev_ProcessFiq`.
- Traces and GDB sessions on the unmodified images confirmed each step.

The register map is in the comment of `Esp32C3::WifiMac`
(`src/emu/esp32c3.cpp`).

| Item | Finding |
|---|---|
| TX queue q | control word at `0x60033D08 - 8q`; bits 31:30 start it; bits 19:0 address the first lldesc, with the upper bits from `0xC64` (0x3FC00000) |
| TX completion | bit q of `0xCB0`, cleared through `0xCAC`; the result in bits 15:12 of `0x60034320 - 0x4C*q` (0 success, 1 RTS error, 2 CTS timeout, 4 TX error, 5 ACK timeout) |
| RX | `0x084` bit 31 enables it, bit 0 asks for a reload; `0x088` base, `0x08C` next, `0x090` last filled; lldesc words with EOF (bit 30) and owner (bit 31) |
| RX buffer | the 48-byte `wifi_pkt_rx_ctrl_t` header, then the frame with its FCS; header word 0 bit 28 marks interface 0 (the station), bit 29 interface 1; `rx_state` (byte 47) 0 |
| Interrupts | `0xC3C` = events masked by `0xC34`; bit 24 RX, bit 7 TX done, bit 19 TX timeout, bit 21 RX event; source 0 |
| Channel | the low byte of the analog register `0x6000E0C4` is twice the frequency above 2400 MHz (`rom_write_chan_freq`) |
| Addresses | own address `0x040/0x044`, BSSID `0x000/0x004`, with masks and enable bits at `0x060/0x064` and `0x020/0x024` |

**The air.** `WifiAir` (`src/emu/wifi_air.*`) connects boards and an access
point:

- The access point sends beacons every 102.4 ms with SSID, rates, DS, TIM,
  ERP, extended rates, and RSN for WPA2.
- It answers probe requests for any SSID or its own.
- It acknowledges frames addressed to its BSSID.

A frame sent to one station that nobody acknowledges ends with status 5 (ACK
timeout), so the driver's retries apply. Each frame's airtime is timed at
1 Mbit/s with the long preamble. This is approximate: rates, collisions and
CCA are not modelled.

**Tools.**

- `simnodus-emu --esp32c3 image.bin --wifi-ap SSID[@channel][,wpa2]` puts
  the access point on the air.
- `--wifi-log` prints every frame.
- `--wifi-pcap file.pcap` writes them for Wireshark (link type 105).

**Tests.** `emu-esp32c3` has two new cases:

- The ESP-IDF scan against an open AP and a WPA2 AP: the probe requests on
  channels 1-11, the probe response, the beacons, and two runs that give the
  same frames at the same times.
- MicroPython's scan, the station's first authentication, and the pcap
  format.

**Gate G9a status.**

- G9a.1 is met for ESP-IDF and MicroPython. The Arduino sketches connect
  instead of scanning, and they reach the same authentication step.
- G9a.2 is partial. These still read back what was written:
  - the power interrupt block (`0x60035118`/`11C`) and the TSF registers
    (`0x60035000`), which SN-267 needs for beacon timing and power save;
  - the page at `0x60011000`.
- G9a.3: the start was measured with SN-265.

**Not yet.**

- Authentication, association, the WPA2 handshake and DHCP (SN-267).
- The gateway (SN-268).
- The air in the application: today only `simnodus-emu` and the tests
  attach one.
- Several boards on one air need a shared clock between their machines.

## SN-267, first part: an open network with DHCP (2026-11-06)

**Result.** Against an open access point, the unmodified ESP-IDF station
example joins and gets an address:

- `init -> auth -> assoc -> run`, then
  `connected with SimNodus-open, aid = 1, channel 6, BW20`;
- `sta ip: 192.168.4.2, mask: 255.255.255.0, gw: 192.168.4.1`.

MicroPython's `WLAN.connect('SimNodus-open')` gives `isconnected()` True and
`ifconfig()` `('192.168.4.2', '255.255.255.0', '192.168.4.1', '192.168.4.1')`.

lwIP's duplicate-address probes (ARP) take about 1 s of virtual time before
the address is announced, as on a board. The station then starts power save
(`pm start, type: 1`) and sends null-data frames with the PM bit; the frames
still reach it, because the MAC model receives while the station "sleeps".

**The access point.** `WifiAir` now:

- answers open-system authentication and (re)association (AID from 1);
- forgets a station on deauthentication or disassociation;
- passes unprotected data frames from associated stations (LLC/SNAP, QoS
  data included) to its network, and sends the answers as data frames from
  the distribution system;
- spaces its own frames so that they never overlap.

`WifiNetwork` (`src/emu/wifi_network.*`) is the network behind it:

- 192.168.4.1/24;
- a DHCP server for 192.168.4.2-20: OFFER, ACK, NAK for another address,
  RELEASE;
- ARP and ping for the gateway;
- DNS answered with "no such name" until the gateway of SN-268 resolves
  names.

**Tests.** `emu-esp32c3` has two new cases:

- the ESP-IDF station and MicroPython joining the open network, with the
  lease checked;
- ping, DNS and the DHCP messages, tested directly on the network.

**Gate G9b status.**

- G9b.1 is met for the open network on ESP-IDF and MicroPython. The Arduino
  sketches use the WPA2 network.
- WPA2 comes next: the 4-way handshake, the MAC's CCMP key slots and the
  wrong-password reason code (G9b.2, G9b.4).

## SN-267, second part: WPA2-PSK (2026-11-06)

**Result.** Against a WPA2-PSK access point (`SimNodus` / `simnodus-wifi`),
all three frameworks join and get an address:

- The unmodified ESP-IDF station example prints
  `connected with SimNodus, aid = 1, channel 6, BW20` and
  `security: WPA2-PSK, phy: bg, rssi: -40`, then
  `sta ip: 192.168.4.2` and `connected to ap SSID:SimNodus password:simnodus-wifi`.
- Arduino's `WiFi.begin("SimNodus", "simnodus-wifi")` prints
  `wifi client: address 192.168.4.2`.
- MicroPython's `WLAN.connect()` gives `isconnected()` True and the same
  address.

With a wrong passphrase, the station's message 2 fails the MIC check. The
access point sends message 1 again three times, 1 s apart, then
deauthenticates with reason 15, as hostapd does. The station reports
`wifi:state: run -> init (0xf00)` and the example retries. Whether a board
against a real access point prints the same is part of G9b.2, with the
owner's board.

**Our cryptography (DE9b).** `src/emu/wifi_crypto.*`:

- SHA-1, HMAC-SHA1, PBKDF2, the 802.11 PRF;
- AES-128/256, both ways;
- AES key wrap, CCM and CCMP.

They are checked against FIPS 180-4, RFC 2202, IEEE 802.11 annex J (PBKDF2),
FIPS 197 C.1/C.3, RFC 3394 and RFC 3610 packet vector 1. The PRF was checked
against Python's `hmac`, and CCMP by round trip and by the firmware itself.

**The access point.** After a WPA2 association, the 4-way handshake
(EAPOL-Key, RSN descriptor, HMAC-SHA1 MIC):

- Message 1 carries ANonce.
- Message 2 gives the PTK (KCK, KEK, TK) from PBKDF2(passphrase, SSID) and
  the nonces; its MIC is checked.
- Message 3 has the RSN element and the GTK KDE, wrapped with the KEK.
- Message 4 ends the handshake.

Then data frames are CCMP in both directions, with replay detection. The
nonces and the GTK are derived deterministically, so runs repeat.

**Three findings in the firmware's hardware path.**

1. **The FCS slot.** Every frame the driver queues has 4 more bytes at the
   end, the FCS slot, filled with `ef be ad de`. The MAC computes the FCS.
   The model now drops them; until then, the EAPOL MICs did not verify.
2. **The AES accelerator.** The supplicant unwraps message 3's key data with
   the AES accelerator in DMA mode (`aes_hal_wait_done` waits for STATE 2).
   `AesAccelerator` (`src/emu/esp32c3.cpp`; TRM chapter "AES Accelerator",
   ESP-IDF's `hwcrypto_reg.h`/`aes_ll.h`) works:
   - in typical mode;
   - in DMA mode through GDMA (peripheral 6), with ECB, CBC, OFB, CTR, CFB8
     and CFB128, AES-128 and AES-256, interrupt source 51.

   GCM gives a diagnostic.
3. **Hardware CCMP.** The driver installs the keys in the MAC
   (`hal_crypto_set_key_entry`):
   - Entry n is at `0x60034400 + 0x28*n`: the peer's address in words 0
     and 1, bit 30 of word 1 for a group key, the 16-byte key in words 2-5.
   - `0x60033814` is the mask of valid entries.
   - On this run the PTK went to entry 4 and the GTK to entry 0. The keys
     were matched against the ones recomputed from the captured handshake.

   The driver then queues protected frames with their CCMP header, the plain
   body and room for the MIC; the MAC encrypts them. Received protected
   frames are decrypted in place for the driver: header, CCMP header, plain
   body, MIC bytes zeroed.

**Tests.** `emu-esp32c3` has two new cases:

- the ESP-IDF WPA2 station: four EAPOL frames, data protected from the
  station with a non-plain body, DHCP;
- the wrong passphrase, Arduino, MicroPython, and the cryptographic vectors.

**Gate G9b status.**

- G9b.1 is met for open and WPA2 on ESP-IDF, Arduino and MicroPython.
- G9b.4 is met: the crypto vectors pass.
- G9b.2: the wrong-password reason is to be compared with the owner's board.
  The rejoin after the access point is turned off and on waits for the
  application's controls.
- G9b.3 (console against a real access point) waits for the owner's board.

The access point's settings in a project come with the integration
(SN-269).
