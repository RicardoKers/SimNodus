# SN-021: observed TxF overwrite boundary

Date: 2026-09-22. Status: **six fixture observations passed; not product acceptance**.
No production save API changes. ADR 0064 and create-only persistence remain in force.

## Question and protocol under test

The earlier [overwrite counterexamples](SN-021-overwrite-boundary.md) lose exclusion
when the destination handle closes. Probe whether transaction-level isolation can
span that gap, while checking expected identity and bytes before changing content.
This evaluates a different mechanism; it does not repeat check-then-rename.

The manually invoked [probe](../../tests/resources/windows_txf_boundary.py) creates
six fresh owned fixtures. It pins every directory ancestor through the existing
Windows/NTFS handle-relative checks, obtains a volume-GUID spelling from the retained
parent and captures expected identity/bytes through the existing verifier. After
releasing that read handle, it opens a transacted writer, verifies identity and
bounded bytes under that writer, writes/truncates only its private transaction,
closes the handle and commits or rolls back. Fixed simple leaf names and no-reparse/
regular-file/single-link checks are retained. No textual-prefix or resolve-then-open
containment proof is used in this probe. Expected state comes from the host fixture,
never a project manifest. This is test orchestration, not a reusable save capability.

## Observations

| Case | Measured result |
|---|---|
| Commit | Outside reads retain old bytes during writes and after writer-handle close; final bytes become the complete new fixture after commit |
| Rollback | Outside reads retain old bytes; rollback restores the original bytes |
| Same bytes, different file identity | Rejected before writing; replacement preserved |
| Same identity, changed bytes | Rejected before writing; changed version preserved |
| Existing nontransactional writer | Transacted writer open rejected with error 6800; original preserved |
| Existing writable mapping after its file handle closes | Transacted writer open rejected with error 6800; original preserved |

Commit and rollback cases each launch separate Python competitors for write and
rename, both before and after closing the transaction's file handle. All eight
competitor requests reject. Rename reports Windows error 32; Python's buffered
writes expose no Win32 code (null), which is not relabelled as a measured code.
Transaction isolation persists beyond the individual writer handle in this host.
This observes sampled before/after states, not an exhaustive proof of atomicity
against every reader, crash, file mapping, adversarial timing or filesystem variant.

The first two preliminary probes also observed rollback and commit isolation.
They used ordinary fixture paths and explicitly did not claim containment; retain
them as preliminary results, separate from the final ancestor-pinned probe.
The commit probe retains the historical key `outside_before_rollback`, whose value
is the outside observation before finalization; its actual final action was commit.
Do not rewrite that raw report. No failed probe result occurred in this slice.

The [audit](evidence/SN-021-txf-boundary-summary.json) records host, Python, source
and raw-result hashes, both preliminary reports, 106 unchanged historical evidence
files and twelve preserved local files. Raw fixtures remain under ignored build
paths. No engines or dependencies are downloaded or selected by project contents.

```sh
python tests/resources/windows_txf_boundary.py --output build/sn021-txf-boundary-new-run
python tools/check_repository.py
git diff --check
```

Use a fresh output directory on a supported local Windows/NTFS host. The probe is
manual and is not added as a required cross-platform CTest. Unsupported TxF or a
failed observation is a failed/inconclusive probe, not accepted product fallback.

## Selection remains open

Microsoft documents [transactional writer isolation](https://learn.microsoft.com/en-us/windows/win32/fileio/txf-basic-concepts)
and [transacted file behavior](https://learn.microsoft.com/en-us/windows/win32/fileio/programming-considerations-for-transacted-fileio-).
It also [recommends alternatives](https://learn.microsoft.com/en-us/windows/win32/fileio/deprecation-of-txf)
to taking a new TxF dependency because future availability is uncertain. These local
observations do not override that product/platform consideration. The owner explicitly
selected continued research without TxF and retained overwrite in the SN-021
acceptance scope. Keep create-only support while investigating a maintained
alternative. These probes are retained evidence, not approval to adopt TxF.

Before any product claim, a selected protocol still needs bounded validated-project
inputs, trusted expected-version authority, physical conflict/failure coverage,
unsupported-host behavior, structured uncertain-commit handling and a declared
concurrency/durability contract. Ordinary rollback here is not power-loss recovery.
No replacement of the current API, directory ACL policy, privileged helper or new
storage format was introduced. Preserve SN-017, SN-044, instrumentation, MCU/toolchain
independence, engine profiles/tolerances and PDF/PID. SN-021 remains in progress;
its final audit and next-cycle prompt await a validated overwrite protocol.
