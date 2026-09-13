# Persistent analog pause candidate

Before execution, require foreground `stop when time >= T` combined with
`ngSpice_SetBkpt(T)` to return within 1 ps of each requested boundary:
2010875, 2011375, 4010750, and 4027000 ns, followed by a 5 ms endpoint.
Use one loaded RC circuit and `resume`, with strictly increasing copied samples.
No callback invokes engine commands. Keep all observed boundaries, including
failed candidates. This tests known future boundaries, not asynchronous halt or
joint CPU/analog coordination. An exact-boundary failure blocks that claim.

For the repeated confirmation, retain the 1 ps boundary tolerance. Require
100 ms of wall-clock pause with unchanged copied sample count, time, and voltage.
For the source held at 3.3 V from time zero, require every boundary and the
endpoint to agree with `3.3 * (1 - exp(-t / 0.001))` within 10 microvolts,
and the endpoint to agree with fresh uninterrupted replay within 10 microvolts.
Require three identical result sequences. These tolerances are fixed before
running the confirmation. Callback stability observes emitted data; it does not
inspect all internal solver memory.

## Result (2026-09-08)

Three repetitions passed, with identical boundary/sample/voltage sequences and
stable paused observations. All four intermediate times and the 5 ms endpoint
met the original 1 ps tolerance. Final output was 3.2777647750411987 V; the
uninterrupted reference was 3.277764775048202 V (difference about -7 pV).
Every observed boundary met the analytical 10 microvolt tolerance.
The same DLL and loaded circuit performed four pauses and resumes; no replay
was used within that sequence. The separate reference process is comparison only.

Run after building `e05_persistent_circuit` and `e05_circuit`:

```powershell
python tests/experiments/debugging/persistent_analog.py --native build/sn016/persistent-native/Debug --output build/sn016/persistent-analog-next
```

The output directory must be new. See
[hashes and observations](../../../docs/experiments/evidence/E-05-persistent-analog-summary.json).
This establishes a bounded analog building block. Integrating a CPU stop with
persistent analog advancement, exchanged GPIO/ADC state, and fault/lifecycle
handling remains necessary before claiming joint pause or completing SN-016.
