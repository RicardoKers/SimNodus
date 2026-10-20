# ADR 0102: Digital devices run in step with the firmware (hybrid coupling)

Date: 2026-10-19. Status: accepted (owner's choice of 2026-10-19; tasks
SN-203 to SN-206).

## Context

After 0.5 the owner asked for SPI and I2C with parts to talk to: 74HC595,
MAX7219 with 7-segment digits and 8x8 matrices, 24C02, LM75, PCF8574 and an
HD44780 LCD. Board and circuit are coupled in windows of 100 µs (ADR 0085,
ADR 0091): circuit changes reach the firmware up to 0.1 ms late. That delay
does not matter for what the board only writes (74HC595, MAX7219, a parallel
LCD), but every I2C byte ends with an acknowledge the master reads, and EEPROM,
sensor and SPI reads return data within the same bus cycle. Through the
circuit, reads would only work below a few kHz, under the slowest SPI clock
the STM32 can make (PCLK / 256). The owner first chose "pin level through the
circuit", then the hybrid coupling once this was explained.

## Decision

- **Device logic runs in the board's runner**, in the emulator thread
  (`src/devices`, Qt-free). Library parts with a `device` section
  (`kind`, `supply_pin`, `ground_pin`, `output_resistance`) name their model.
- **Digital nets are resolved at once.** The session finds, for each board,
  the devices whose signal pins share a net with a GPIO pin, directly or
  through another device of that board (`build_mcu_requests`). Nets on ground
  or on a supply are constants: they tie inputs and never join devices. The
  runner's `DeviceBus` resolves a digital net whenever a board pin (emulator
  pin hook) or a device output changes: a driven low wins (with a message if
  something drives high at the same time), then a driven high, then the board
  pin's internal pull, then a resistor to a supply or to ground found in the
  circuit, else the net keeps its level. The board's inputs and the devices
  see the result at the same instant, so devices answer within the bus cycle.
- **The circuit sees the same edges.** Device output pins are watched pins of
  the board's runner ("U2.Q0"): they drive the circuit through B sources like
  board pins (`{ext:<pin>_g}`, `{ext:<pin>_i}`), at the device's supply and
  output resistance, with exact edges when enabled, and have logic lanes. A
  missing supply, ground or pull-up is visible: the part does nothing, or the
  bus stays where the pull leaves it.
- **Board pins on digital nets take their input from the bus**, not from the
  circuit, so the circuit's late view cannot overwrite them.

## Alternatives

- **Pin level through the circuit:** realistic, but I2C and every read fail
  at normal clocks (above); rejected by the owner after the explanation.
- **Transaction level** (bytes exchanged directly when pins share a net, like
  the board-to-board serial link): simplest and fastest, but no waveforms of
  the device's answers and no electrical mistakes to find; the hybrid keeps
  both.
- **Device models in SPICE/XSPICE:** state machines (EEPROM, LCD controller)
  would need compiled code models; out of proportion.

## Consequences

- A device works only together with a board; one on no board pin gets a
  message and does nothing. A device shared by two boards belongs to the first.
- Levels are digital: a 3.3 V board driving a 5 V chip is accepted even where a
  real chip's input threshold would not be met. Device inputs on nets without
  a board pin or device output read only constant ties (ground, a supply, open
  = low), not circuit signals such as a push button (planned with the PCF8574,
  SN-205).
- The protocol between editor and engine carries devices and nets (version 2).
- Multiplexed displays (MAX7219) will need glow averaged over a few
  milliseconds on the canvas (SN-204).
