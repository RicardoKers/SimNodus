# Local build storage and safe cache cleanup

The Windows experiments give every Renode process a fresh `TEMP`/`TMP`
directory under its result directory. The pinned executable extracts its .NET
runtime into `runtime-*/.net/Renode`. Each copy is approximately 265 MiB and
can be recreated from the retained executable. Repeated process groups can
therefore consume many GiB even when their logs and results are small.

## Cleanup procedure

Stop all experiment runners, Renode, GDB, and STM32CubeIDE first. From the
repository root in a PowerShell environment where local scripts are permitted:

```powershell
# Preview only; no caches are removed.
./tools/clean_experiment_cache.ps1

# Remove only eligible extracted Renode runtimes.
./tools/clean_experiment_cache.ps1 -Apply
```

The [cleanup tool](../../tools/clean_experiment_cache.ps1) is restricted to
`build/sn012`, `build/sn014`, `build/sn015`, `build/sn016`, and `build/sn019`.
Within those directories it removes only `.net/Renode` below `runtime-*`
directories containing the expected extracted-runtime markers. It checks
absolute path containment and refuses linked/reparse paths and active named
backend/debugger processes. Do not start another experiment during cleanup;
the process check is not a session lock.

Dependencies and downloaded packages in `build/deps`, generated firmware,
native builds, source audits, JSON evidence, logs, IDE workspaces, and unfinished
prototype sources remain in place. The next Renode execution re-extracts the
runtime; this may add startup work but requires no new download.

Every invocation writes a local JSON inventory/removal report under
`build/maintenance`. Review skipped paths: Windows may restrict runtime
directories to the account/context that created them. Use the original allowed
context when appropriate. The script does not change execution policies, take
ownership, alter ACLs, or suppress permission failures.

## Do not delete all of build during unfinished experiments

`build/` is ignored by Git, but that is not proof that every file is disposable.
The unfinished E-05 CubeIDE executor currently includes hand-written Java and
Python sources under `build/sn016`, and its latest plain-GDB evidence is in
`build/sn016/plain-debug7`. Both are needed to resume SN-016. Promote the useful
prototype source and finalized evidence before considering broader cleanup.

Run the targeted cleanup after completed experiment batches as needed. It is
explicit maintenance; experiment runners do not yet remove these caches
automatically. Do not change simulation timing, failure evidence, or process
isolation merely to reduce storage.

## SN-042 local cleanup evidence (2026-09-03)

- Removed 243 extracted runtime caches: 287,712 files,
  67,616,433,747 logical bytes (62.973 GiB).
- Ran two disjoint cleanup passes in the contexts that could already access
  their respective runtime directories: 43 caches and 200 caches. No ownership
  or permission changes were required. Both subsequent previews found zero
  eligible caches in their accessible scopes.
- Storage remaining is approximately 1 GB, mostly pinned dependencies and
  retained experimental work. Individual filesystem scans have partial access
  to the other context's temporary directories; they are not independent totals
  to add together.
- Immediately after deletion, SHA-256 verification found all 10,690 protected
  files outside the process-temporary directories byte-identical. This covered
  unfinished CubeIDE sources, raw E-05 results, other experiment evidence,
  native builds, firmware, and dependencies. Planning documentation was then
  deliberately updated with this result.
- All 14 pinned dependency/archive checks passed. A disposable fixture verified
  preview behavior, bounded removal, preservation of logs/sources/dependencies,
  rejection of unrecognized cache contents, and rejection of a junction inside
  an otherwise eligible cache. No new simulation run was required for cleanup.

The detailed inventories and preservation hashes remain local under
`build/maintenance`. This maintenance does not complete the E-05 CubeIDE gate.
