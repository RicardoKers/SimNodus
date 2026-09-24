# SN-021 saving workflow decision

Date: 2026-09-24. Status: **owner accepted managed-document saving**.

The owner selected Save updating a copy managed by SimNodus. Explicit import leaves
the external original unchanged; export remains create-only. This selects the
workflow and revises SN-021's saving destination criterion, not its identity/version,
atomicity or preservation guarantees. External overwrite remains unsupported.

[ADR 0076](../decisions/0076-managed-document-saving.md) records the decision and
the [managed saving contract](../architecture/MANAGED_DOCUMENT_SAVE.md) defines
acceptance. Installation, concrete commit layout and physical save/recovery evidence
remain pending. No service installation is authorized. SN-021 remains in_progress.

The prior alternatives were keeping arbitrary-directory overwrite or deferring the
choice while retaining Save Copy. They were not selected. The bounded configured
RC lifecycle already has evidence; further work must not invent new runtime scope.

Next complete only the relevant isolation prerequisites, then specify the commit
algorithm before implementing it. Preserve all earlier failed/inconclusive evidence,
local SN-045 work and domain/runtime boundaries. Prepare the next-cycle prompt only
after actual SN-021 acceptance.
