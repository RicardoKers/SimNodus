# SN-021: oplock replacement boundary

Date: 2026-09-22. Status: **three physical observations; overwrite still unsupported**.
The owner selected continued research without TxF. Keep ADR 0064 and create-only
persistence in force. This is a manual experiment, not a production save protocol.

## Question and acceptance of this investigation

Can an ordinary user-mode oplock retain exclusion while a separate file object
publishes replacement bytes? An observation must record a granted request,
break event/flags, whether replacement finished before releasing the watched
handle, completion after release and final bytes. A successful probe is not a
successful overwrite API: publication still needs expected identity/version,
continuous exclusion, atomic visibility and ordinary-failure preservation.

The [probe](../../tests/resources/windows_oplock_boundary.py) creates three fresh
owned fixture directories, pins every ancestor with the existing Windows/NTFS
handle-relative verifier, and obtains a volume-GUID spelling from that retained
parent. The GUID path is a name, not a textual containment proof. All mutations
use fixed fixture leaf names; the replacement is handle-relative to the retained
parent. The source file is opened before requesting the oplock. A same-process
worker requests POSIX replacement through a different file object with its normal
oplock key. File handles permit delete sharing, so this tests oplock behavior
rather than repeating the known no-delete-share failure.

The captured old identity is checked for file cases. No claim of capturing a
protected expected version after the oplock grant is made. The experiment issues
no competing read/open while the oplock is retained. It waits up to two seconds
for notification, samples completion for 250 ms, closes the watched handle and
joins the worker for up to five seconds. Request buffers survive asynchronous
completion. No application, backend or project-selected resource is executed.

## Results and limits

| Requested mode | Before watched-handle release | After release |
|---|---|---|
| Directory Read-Handle (3) | Break signaled, ACK_REQUIRED set, replacement already completed | Complete new bytes |
| File Read-Write (5) | No break event; replacement already completed | Complete new bytes |
| File Read-Write-Handle (7) | Break to Read-Write, ACK_REQUIRED set, replacement still pending | Replacement completed; complete new bytes |

All requests returned ERROR_IO_PENDING (997), indicating a granted oplock. All
replacements ultimately returned success. The directory observation is especially
important: an ACK_REQUIRED flag alone did not imply the child-entry replacement
was blocked. No ordering of two queued writers or continuous reader history was
measured. A finite pending observation does not prove indefinite blocking.

The first attempt inverted HANDLE and WRITE bits and requested an invalid mode
on a directory. It failed before replacement. Its raw report and source hash are
preserved; it omitted the numeric error and partial case, which are not invented
later. The corrected attempt used installed SDK constants (READ=1, HANDLE=2,
WRITE=4) and completed all three cases. The [audit](evidence/SN-021-oplock-boundary-summary.json)
retains both attempts, current source/dependency hashes, 107 historical evidence
hashes and twelve preserved local files. No historical result was rewritten.

## Interpretation and next gate

Microsoft describes directory-content oplock notifications as advisory in
[FSCTL_REQUEST_OPLOCK](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_request_oplock).
Its [rename behavior](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-set-information2)
distinguishes modes that allow rename without a break from handle-caching modes
that require acknowledgment. These observations do not establish an exception
that lets our replacement retain protection while excluding another writer.
Closing or acknowledging and then rechecking is not an accepted repair.

Do not extrapolate this to a proof against all oplock protocols. Shared-key/kernel
extensions, cross-process adversaries, mappings, failure recovery and host variants
were not tested. No driver, privileged helper, ACL mutation or new project format
is selected. The next useful step is a concrete ownership/concurrency proposal
with an independently enforceable publication authority: evaluate whether that
requires a host-owned store or broker, and state how it differs from the current
arbitrary selected-directory contract before implementation. A cooperating-writer
lock or rename primitive alone is insufficient. Do not add a broker or weaken
SN-021 acceptance implicitly; TxF remains excluded by the owner's decision.

No engines changed, so no real-engine rerun is claimed or required by this slice.
Preserve SN-017 Python/GDB/fixture ownership, SN-044, shared instrumentation,
MCU/toolchain independence, numerical profiles/tolerances and PDF/PID fixes.
SN-021 remains in progress; the next-cycle prompt awaits final acceptance.

```sh
python tests/resources/windows_oplock_boundary.py --output build/sn021-oplock-boundary-new-run
python tools/check_repository.py
git diff --check
```

Run manually on local Windows/NTFS with a fresh output path. No supported-save
fallback is inferred from unsupported, failed or inconclusive probe results.
