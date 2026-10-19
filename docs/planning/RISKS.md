# Risks

Revision 2 — 2026-10-14. Previous register: [archive](../archive/2026-10-legacy/RISKS.md)
(its backend risks R-01..R-16 remain valid evidence).

| ID | Risk | Impact | Mitigation |
|---|---|---|---|
| R-101 | ngspice memory growth in unbounded interactive runs | Interactive mode unusable after minutes | Spike S1 (SN-103): segmented runs with initial conditions; alternatives in ADR 0080 |
| R-102 | Real-time pacing not reached for stiff circuits or small MCU quanta | Slow-motion simulation | Show achieved speed; adaptive output step; tune quantum Q; limit component set |
| R-103 | MCU↔circuit feedback is approximate (E-03) | Timing artifacts visible to students | Label approximate mode; choose Q from measurements; document in guide |
| R-104 | Renode STM32F1 peripheral gaps (timers/PWM, clocks) | Lessons 3+ blocked | Closed 2026-10-18: Renode removed (ADR 0094); timers, PWM, clocks and ADC are modeled in the own emulator |
| R-105 | Windows packaging (Qt runtime, ngspice DLL) | Classroom install friction | CI-built packages from release 0.1; clean-PC test in phase 4 |
| R-106 | Two-week phases too tight | Slips | Reduce phase scope, keep release dates; debugging is first to postpone |
| R-107 | Development environment cannot run Windows | Windows-only defects found late | Windows CI on every change; owner tests each Friday build |
| R-108 | Engine crash inside the app process (phases 1–3) | Loss of unsaved work | Closed 2026-10-19: the engine runs in its own process (SN-151, ADR 0101); autosave stays |
| R-110 | Own emulator: subtle instruction or exception bugs | Wrong firmware behavior that students blame on their code | Instruction tests from the ARMv7-M pseudocode, differential tests against Renode in E-07 (removed with Renode, ADR 0094), firmware corpus, real board (plan N0–N5) |
| R-111 | Interpreter too slow on lab PCs | Runs below real time in class | Gate G0a.3 asks 2x real time at 72 MHz on the owner's laptop; WFI fast path; measured speed shown |
| R-112 | Peripheral scope grows beyond the calendar | Releases slip | Tiers in the design, gates per milestone, roadmap scope protection |
| R-113 | Edge breakpoints make the analog engine slow | Serial/PWM circuits below real time | Measure in N2 (EQ-2); limit breakpoints to watched or connected pins |
| R-114 | Classroom release depends on a young engine | Failures in class | Decision D3 at the freeze; no fallback engine since D4 (ADR 0094): MCU regressions are fixed in the emulator |
| R-115 | Emulator code written and reviewed only by agents | Shared blind spots | Independent references: ARMv7-M pseudocode vectors, Renode (E-07, until ADR 0094) and QEMU as test oracles, the owner's real board for disputed cases |
| R-116 | Tests on two engines double CI time (ctest already about 4 min) | Slower feedback | Closed 2026-10-18: one engine since ADR 0094; the Renode suites were removed |
| R-117 | After D2 the Renode fallback lacks phase 3/4 features | A fallback at the freeze ships without ADC/PWM/debugging | Closed 2026-10-18: no Renode fallback since ADR 0094 |
| R-118 | ngspice keeps memory at every interactive segment restart | Slow growth in long runs (about 8 MB per hour with a Blue Pill at 1 s segments) | Measured 2026-10-16 (G1 soak); SN-188 revisits the segment length; destroy all + remcirc already run after each segment |
| R-109 | Lab constraints (no admin rights, old PCs) unknown | Install/perf problems | Portable zip without installer as fallback; ask lab specs (Q-103) |
