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
