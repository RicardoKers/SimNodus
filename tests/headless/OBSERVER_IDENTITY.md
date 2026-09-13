# SN-018 corrected observer campaign

Declared after nine identity tests passed, before engine execution on 2026-09-12.
The [previous contract](OBSERVER_CONTROL.md) remains historical. Keep its exact
engine profile, flags, inputs, tolerances and U,S / S,U / U,S order: six batches,
36 fresh sessions, 18 per condition, no warm-up, outlier removal or retries.

Run `python tests/headless/measure_identity.py --output build/sn018/identity-01`.
The original sampler and unsuccessful campaign are preserved, with all previous
raw-file hashes checked before/after. New script, sampler, tests and this contract
are frozen in the manifest before execution. Versions remain those of the
unchanged baseline inputs; no build, download or capability change occurs.

Only S uses `ProcessTreeSampler`. Both retain 100 ms poll sleeps. The corrected
sampler retains process handles until batch end and records creation FILETIME,
parent PID/creation identity, snapshot timestamp, query errors and rejections.
Accept a newly seen child only if its creation is strictly after its verified
parent's creation, no later than snapshot start and no later than the parent's
exit when known. Query memory through the same retained handle. Reject ambiguous
equal timestamps and processes that vanish or cannot be opened. Follow only
verified parents; names are descriptive, never an ownership allowlist. Close all
handles at the end of each batch before proceeding to the next condition.

This is conservative sampled attribution, not a security boundary or complete
process census. Missed short-lived parents/children, access failures and equal
timestamps can undercount. Retaining handles adds observer resources; measure
that corrected observer's cost rather than claiming equivalence to the old one.
Creation timestamps are OS wall timestamps, not simulation time. Clock anomalies
may cause conservative rejection. Raw parent/creation records support review.

Acceptance: the original 10 microvolt/1 ps/2 s functional limits, nine fixed
checkpoints and final virtual 4027000 ns/ADC 3541 remain. Audit every memory row
for a path of recorded parent identities back to the root; no child can predate
its parent. Any attribution inconsistency invalidates the control and must be
retained. Record query/rejection counts and sampling gaps; nonzero conservative
rejections imply incomplete memory coverage, not a fabricated zero.

Report all three paired S-minus-U wall differences and percentages, paired
median pause differences, condition min/median/max/sample deviation, memory
sampled peaks and sampler wall durations. U memory stays unmeasured. Mixed signs
or variability leave causal slowdown unresolved; no p99/equivalence/host-independent
performance claim. Keep PDF suppression, PID retry and ADRs 0014/0027/0028/0043.
This campaign does not certify autonomous C++ orchestration or close SN-018's
remaining coverage review.
