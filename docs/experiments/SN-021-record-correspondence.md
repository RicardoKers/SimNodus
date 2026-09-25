# SN-021: selected physical record correspondence

Date: 2026-09-25. Status: **observed for five selected records only**.

The existing successful [managed-store batch](SN-021-managed-store-candidate.md)
recorded 64 revisions but did not retain full record bytes for independent comparison
with the input project and resource-context encoding. A read-only VIX collection from
the same unreverted disposable VM copied the generation manifest, the original
project fixture and committed revisions 1, 2, 3, 63 and 64. It created no accounts,
executed no guest program and changed no snapshot. The collection result is
[`observed-selected-record-correspondence-only`](evidence/SN-021-managed-record-correspondence-result.json).

The [byte archive](evidence/SN-021-managed-record-correspondence-bytes.json) stores
the seven copied inert files as base64 with their SHA-256 values and the pinned hash
of the prior physical report. This publishes evidence bytes, not an executable.
The [independent reader](../../tests/resources/managed_record_correspondence.py)
uses Python `struct` and `hashlib`, with no project execution. The
[replay helper](../../tests/resources/replay_managed_record_correspondence.py)
decodes the archive to a temporary directory and reproduces the checks:

```text
python tests/resources/replay_managed_record_correspondence.py
```

Local replay passed. An inert adversarial self-check rejected changed project
content and altered record/request bytes, including recomputed record hashes.
The repository checker passed on 802 workspace text files; hosted checks still
gate integration.

For each selected record, its full-file SHA-256 matches the earlier independent
filesystem snapshot. The reader checks exact lengths and scope against the copied
manifest; binary client SID, operation ID and resource-context bytes; the exact
original project padded to 71,680 bytes, with the single additional space for
even revisions; and independently recomputed request and record SHA-256 digests.
It verifies the predecessor links 1→2→3 and 63→64. Revisions 1 and 3 have equal
project bytes but different complete records and revisions. The runner source hash
matches the source retained in the original attempt.

This is a selected-file comparison after the prior run. It does not re-inspect all
64 records, authenticate a Save request, recapture the physical resource-root
identity, establish live reader visibility, prove power-loss durability, or grant
execution, redistribution or simulation readiness. Five matching records and a
manifest do not establish trusted origin. SN-021 remains in progress; next close
the remaining physical matrix and authenticated managed Save boundary before
composition with the accepted real RC lifecycle. The external original remains
outside managed Save, and export stays create-only.
