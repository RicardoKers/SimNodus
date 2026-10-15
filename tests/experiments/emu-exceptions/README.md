# Exception-model scenarios (E-07)

`exceptions.s` runs one of ten scenarios selected by the word at 0x200000F4
and logs handler entries and exits (and some register values) to RAM. The
expected logs, worked out from the ARMv7-M manual (DDI 0403E, B1.5), are in
`tests/unit/emu_tests.cpp` ("exception model scenarios..."), where the
emulator runs every scenario. `python tests/experiments/emu-exceptions/build.py`
rebuilds the committed `exceptions.elf`.

| # | Scenario | Expected log (= emulator) | Renode 1.16.1 |
|---|---|---|---|
| 0 | Nothing | EE | same |
| 1 | IRQ1 (0x40) preempts IRQ0 (0x80) | A0 A1 B1 B0 EE | same |
| 2 | PRIGROUP 5: same group, no preemption, tail-chain | A0 B0 A1 B1 EE | same |
| 3 | BASEPRI 0x80 masks priority 0x80 | C0 A0 B0 C1 EE | same |
| 4 | Pending order: priority, then number | A3 B3 A2 B2 A4 B4 A5 B5 EE | same |
| 6 | Thread on PSP | A6 FFFFFFFD B6 20003000 EE | same |
| 7 | Invalid EXC_RETURN 0xFFFFFFF5 | 5C FF CFSR=00040000 (INVPC) HFSR=40000000 (FORCED) | **5C only**: no fault (B1.5.8 requires a UsageFault, INVPC) |
| 8 | STKALIGN with SP = 4 mod 8 | A7 20003FD8 200 B7 20003FFC EE | same |
| 9 | FAULTMASK blocks IRQ0, NMI still taken | 4E C8 A0 B0 C9 EE | **4E C8 C9 EE**: IRQ0, pended while FAULTMASK was set, is never taken (B1.5.4: it stays pending) |
| 10 | WFI wakes on a masked pending IRQ; SVC under PRIMASK | D0 A0 B0 D1 FF 0 40000000 | **D0 A0 B0 D1 EE**: the SVC is neither taken nor escalated (B1.5.4: escalation to HardFault) |

Renode was run on the bare Cortex-M3 of `../emu-differential/diff.repl`
(2026-10-15). The three differences are Renode deviations according to the
manual; running the same scenarios on the owner's Blue Pill would confirm them
on silicon (optional, plan N5).
