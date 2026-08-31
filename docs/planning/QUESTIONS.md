# Owner decisions and open questions

Updated: 2026-08-31. Unanswered questions are recorded, not repeated as permission requests for already authorized local work.

## Confirmed

| Topic | Decision |
|---|---|
| Name | SimNodus |
| GitHub destination (Q-01 resolved) | Public repository `RicardoKers/SimNodus`; publication authorized |
| Author attribution (Q-02 resolved) | Ricardo Kerschbaumer |
| Repository language | English throughout, including code comments |
| First platform | Windows; Linux afterward |
| Openness | Public and open source from the first publication |
| License authority | Owner authorized selection for broad use/copy/modification; MIT selected |
| Classroom date | February 2027 |
| Technical direction | C++ with Qt, ngspice + Renode, Blue Pill first |
| Product priorities | Reusable components/subcircuits, teaching diagnostics, CubeIDE debugging |

## Remaining questions

| ID | Question | Needed by | Working assumption |
|---|---|---|---|
| Q-03 | Which Windows versions, PC specifications, installation privileges, and network restrictions apply in the lab? | September setup review | Local desktop use; no cloud requirement |
| Q-04 | Which first lesson matters most: GPIO/LED, button/EXTI, ADC, or PWM/RC? | M2 scope review | GPIO/RC first, then digital input and ADC |
| Q-05 | How much development/review time is available, and will anyone else contribute? | September planning review | No capacity-based delivery guarantee |
| Q-06 | Which STM32CubeIDE/toolchain version will students use? | E-05 and classroom packaging | Select and record a tested version |
| Q-07 | Should the application UI later support Portuguese as well as English? | M3 UI design | English source/UI baseline; leave room for translation |

None of Q-03 through Q-07 prevents publication or the first standalone backend experiments. The requirement that repository files be English does not preclude a future localization feature.
