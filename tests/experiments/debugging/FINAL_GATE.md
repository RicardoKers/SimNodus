# Final bounded E-05 gate checks

Predeclared 2026-09-10. Preserve the original acceptance criteria and all failed
unpaced candidates. Before deciding the final capability profile, complete the
direct-GDB command-path check: add `--guarded` to the existing extended persistent
pause/lifecycle runner. Route all GDB packets through CooperativeGuard. Issue
actual `-exec-interrupt` after observed progress during the paced grant. Require
exactly one retained interrupt, never forwarded raw, CPU cancellation accounting,
persistent analog agreement before verified SIGINT, stable inspection and ADC
3541 after fresh grants. Run three lifecycle repetitions (six sequences),
including same-GDB recreated detach/reattach and complete listener cleanup.
Keep the two-second request-to-stop limit and 1 ps/10 microvolt tolerances.

```powershell
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --steps --lifecycle --pause --guarded --output build/sn016/<fresh-directory>
```

Also verify the disposable IDE startup fix: preseed the documentation plugin's
configuration-scope major.minor.micro preference to true, using the active
bundle version from bundles.info. Inspection of the installed ReleaseNotesOpener
shows it opens DM00603738.pdf only when that preference is false. The owned
startup plugin must confirm the configured value. Do not modify the installed
IDE, remove PDFs, change browser settings or close existing user documents.
Run extended IDE lifecycle and default lifecycle controls, checking the latter
against the exact existing stop/circuit reference. Record this as startup-policy
evidence; no mouse-driven PDF viewer inspection is implied.

## Measured result

Three guarded direct-GDB lifecycle repetitions passed in
`build/sn016/final-guarded-gdb-01`: six sequences, six retained MI interruptions,
54 fixed checkpoints, stable joint SIGINT inspection and ADC 3541. Request-to-stop
times ranged from 41.5932 to 91.5682 ms. CPU, analog, GDB and relay cleanup passed.
The shared guard/bridge/engine hashes match the final 35-session fault matrix;
the direct runner adds relay routing without changing its Analog class.

Three IDE startup controls passed in `no-release-notes-joint-01`,
`no-release-notes-joint-02` and `no-release-notes-default-01` under `build/sn016`.
The active documentation bundle is 2.3.500; its preseeded preference is confirmed
at startup. Extended lifecycle passed twice, and default lifecycle retained its
exact reference records. Three original four-stop GDB regressions in
`build/sn016/final-plain-control-01` also matched `plain-joint-pause-control-01`
exactly. All 14 relay/accounting tests passed.

[Final evidence](../../../docs/experiments/evidence/E-05-final-gate-summary.json)
and [ADR 0014](../../../docs/decisions/0014-bounded-cooperative-debugging.md) close
SN-016 for the bounded profile. Earlier raw partial flags and failed candidates
are preserved. SN-017 is ready; no production extraction occurred in this cycle.
