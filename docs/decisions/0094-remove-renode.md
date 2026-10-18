# ADR 0094: Remove Renode; the own emulator is the only microcontroller engine

Date: 2026-10-18. Status: accepted (decision D4 of the
[emulator plan](../planning/MCU_EMULATOR_PLAN.md), owner). Supersedes
[ADR 0082](0082-renode-engine-integration.md) and the legacy-engine part of
[ADR 0090](0090-own-emulator-default.md).

## Context

Since [ADR 0090](0090-own-emulator-default.md) (decisions D1 and D2,
2026-10-16) Blue Pill boards run on SimNodus's own STM32F103 emulator by
default, and every new MCU feature is built only there: USART frames on the
pins and RX from the pin, exact edges, timers and PWM, ADC with circuit
voltages, DMA, and the GDB server for debugging. Renode 1.16.1 remained as a
frozen legacy engine (**Simulate > Use Renode for Microcontrollers (Legacy)**)
without those features. Release 0.2.0 (2026-10-18) shipped with the emulator as
the default.

Keeping Renode costs:

- **Package size:** the Windows zip grew to about 137 MB when the Renode
  portable bundle with its .NET runtime was added; the bundle accounts for
  about 100 MB of it ([MCU_EMULATOR.md](../architecture/MCU_EMULATOR.md),
  ADR 0090). Its bundled LGPL/MPL dependencies need their own notices and
  review ([ADR 0082](0082-renode-engine-integration.md)).
- **Start time:** a board on Renode starts after 7–9 s; on the emulator at once
  ([E-07](../experiments/E-07-results.md)).
- **Double maintenance:** a child process per run with platform-specific
  lifetime handling, a vendored external-control client, a loopback server and
  C# models compiled by Renode at run time, a second runner behind
  `McuBackend`, and engine-specific deviations in the user documentation.
- **Slow tests:** the Renode suites need a Renode download on every
  development machine and CI job, and ran several times longer than the same
  cases on the emulator (ADR 0090: 53 s against 10 s for the session cases,
  41 s against 7 s for the GUI cases).

The plan set D4 "after two native releases"; the owner took it on 2026-10-18,
after the first, because the legacy option no longer offered anything a
student needs and its costs remained.

## Decision

- Renode is removed from SimNodus: the engine (`RenodeEngine`,
  `RenodeMcuRunner`, the child-process code and the Renode engine check), the vendored
  client `third_party/renode_external_control/`, `resources/renode/` (platform
  description and the C# models), the Renode bundle and its notices in the
  Windows package, `tools/fetch_backends.py --renode`, the CMake options
  `SIMNODUS_RENODE` and `SIMNODUS_REQUIRE_RENODE`, the menu option, and the
  experiment workflows that drove Renode.
- The command-line option `--check-mcu-engine` stays and now checks the
  emulator: it runs the blink example's firmware (`blink.elf` next to the
  application) for 1.1 s of virtual time and checks that PC13 toggles; the
  package script still runs it.
- The own emulator (`src/emu`, `NativeMcuRunner`) is the only microcontroller
  engine. The old engine setting is ignored when read.
- Board profiles lose the Renode-only fields `mcu.engine`, `mcu.platform`,
  `mcu.extensions`, `mcu.before_firmware` and `mcu.after_firmware`; they are no
  longer part of the format.
- Tests that compared the emulator with Renode (`emu-differential`,
  `emu-firmware`) and the Renode suites (`engine-renode`,
  `engine-renode-session`, `gui-editor-renode`) are removed. Their results stay
  recorded in [E-07](../experiments/E-07-results.md) and the ADRs.

## Alternatives

- **Keep Renode as an optional download, outside the package.** Keeps the
  double maintenance and the slow test path for an engine that lacks the
  features of phases 3 and 4.
- **Wait for a second release on the emulator, as planned.** No MCU regression
  on the emulator was reported; waiting would keep the package size and the
  maintenance for another release without a user of the legacy option.

## Consequences

- One microcontroller engine: user documentation no longer lists Renode
  differences, and the session has one runner behind `McuBackend`.
- The Windows package no longer contains Renode or .NET, nor their license
  texts. Measured on 2026-10-18 with the same build tool: the 0.2.0 zip is
  145 MB, a package of this change 27 MB.
- Development needs no Renode download; the test suite has fewer and faster
  suites (on the owner's laptop, `ctest -j 8` took 165 s with the Renode suites
  and 18 s without them).
- The rollback D1b of the plan (back to Renode after an unfixed regression)
  no longer exists; an MCU regression is fixed in the emulator.
- Fidelity is no longer checked against Renode as an oracle. It rests on the
  instruction tests from the ARMv7-M pseudocode, the exception-model
  scenarios, the emulator's unit and firmware tests, and the owner's Blue Pill
  for disputed cases (plan milestone N5).
- Renode deviations recorded in ADRs 0083, 0085 and 0088 no longer apply to
  the product. Those ADRs, ADR 0082, the experiments SN-019, E-02..E-05 and
  E-07, and their folders stay as history.

## Revisit criteria

A chip or peripheral that the emulator cannot model in reasonable time and
that a course needs, where an external engine would be cheaper than writing
the model; such a case gets a new ADR rather than restoring this one.
