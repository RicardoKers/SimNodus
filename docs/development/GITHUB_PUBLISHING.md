# GitHub setup and publication record

The owner authorized the first public source publication at `RicardoKers/SimNodus`, with Ricardo Kerschbaumer as author. The first commit, `b3163a1`, was published on 2026-08-31. No raw private conversations or binary simulator release are included. See [current state](../planning/CURRENT.md) for ongoing work.

## Source publication gate

- [x] English README, architecture, roadmap, backlog, and contribution guidance.
- [x] MIT license for original material; third-party review policy.
- [x] Git ignore/attributes/editor conventions and issue/PR templates.
- [x] Structural CI workflow with read-only permissions and pinned checkout action.
- [x] Local validation results recorded in the quality document.
- [x] GitHub account/organization and repository name confirmed.
- [x] Copyright attribution updated to Ricardo Kerschbaumer.
- [x] Public metadata and files reviewed for personal paths, credentials, and student information.
- [x] Preliminary GitHub name search completed; no matching repository found before publication.
- [x] Commit identity confirmed against the authenticated account and existing Git configuration.
- [x] Initial commit reviewed and recorded.
- [x] Remote created as public and initial push authorized/performed.
- [x] Hosted CI passed on Windows and Ubuntu; private vulnerability reporting configured.
- [x] Nine initial issues and four milestones created; `main` protection enabled.

Do not publish raw chat transcripts. The research synthesis is sufficient and does not expose private conversation identifiers.

## Remote setup

Repository: [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Description: “Open-source mixed-signal and embedded systems simulator, starting with STM32.”

Suggested topics: `simulation`, `electronics`, `stm32`, `ngspice`, `renode`, `education`, `cpp`, `qt`. Default branch: `main`.

Use a reviewed initial commit and an empty remote, then add the remote and push explicitly. Do not generate an unrelated remote README/license that creates an avoidable history conflict. The owner has authorized these steps. Formal trademark/domain clearance remains open; the preliminary public-name check is not legal clearance.

## Collaboration setup

Initial issues cover SN-010 through SN-018. See the [backlog mapping](../planning/BACKLOG.md) and [GitHub issues](https://github.com/RicardoKers/SimNodus/issues). Labels identify work type, priority, platform, and ready/waiting status. No contributors were invited and no separate project board was created.

Milestones: [M1](https://github.com/RicardoKers/SimNodus/milestone/1) (September 30), [M2](https://github.com/RicardoKers/SimNodus/milestone/2) (October 31), [M3](https://github.com/RicardoKers/SimNodus/milestone/3) (December 15), and [classroom readiness](https://github.com/RicardoKers/SimNodus/milestone/4) (January 31, 2027). These are planning targets, not delivery guarantees.

`main` requires pull requests, an up-to-date branch, resolved conversations, and successful `Foundation (windows-latest)` and `Foundation (ubuntu-latest)` checks from GitHub Actions. Separate reviewer approval count is zero for the single-maintainer phase. Rules include administrators; force pushes and branch deletion are disabled. Squash merging is the enabled merge method and merged source branches are automatically deleted.

The wiki is disabled so documentation stays versioned with code. Private reports use the [security advisory form](https://github.com/RicardoKers/SimNodus/security/advisories/new). Dependency-update PRs should be reviewed, not automatically merged.

## Authorized source checkpoint: 2026-09-13

The owner authorized a local commit and GitHub push of accumulated source and
versionable evidence on `codex/checkpoint-2026-09-13`. This is a recovery point,
not a main-branch merge, issue synchronization or release. Verify the remote tip
matches the local commit before considering the transfer complete. Ignored
`build/` engines, binaries and raw runtime logs are not included in this source
checkpoint; preserve them separately for a complete local-data backup.

Historical evidence JSON is stored without Git line-ending conversion so recorded
hashes retain their original byte meaning. C#, Java, XML, manifest, patch and
PowerShell sources explicitly use LF on checkout. This prevents new checkout
normalization from invalidating fixture/source hashes; it does not certify a
clean-machine engine setup. Existing evidence contents are not rewritten.
Git whitespace checks recognize CRLF in these historical JSON records; preserved
unified-diff patch artifacts retain their required blank context lines.
The already measured `tests/experiments/adc/stm32f103_adc.cs` also retains its
original CRLF bytes explicitly, because real-engine evidence hashes that source.
This exception preserves the input rather than rewriting historical hashes.

The checkpoint review found no common credential-token/private-key patterns in
the versionable files and no binaries selected by the ignore rules. This is a
bounded source-publication check, not a security audit or binary licensing gate.

## Ongoing validated-source policy: 2026-09-13

The owner subsequently authorized coherent validated commits, integration into
protected `main`, and push so GitHub represents the official development state.
This supersedes the checkpoint-only limit. Preserve unrelated valid local work;
never reset, clean, force-push or rewrite published history for housekeeping.
Use the existing pull-request/squash flow with required checks. Do not merge
unresolved conflicts, failing checks or unfinished experiments. Acceptance of a
schema specification does not claim its runtime application is implemented.

The SN-020 integration unit includes the accepted bounded prior headless/baseline
evidence, completed declaration specification/reference tests, and the preserved
SN-044 documentation. No new engine profile, issue synchronization or binary
release is included. Older experimental reports stay labelled and preserved.

Source commit `1500898a7ad08b37d7e6fba15c53183fdb584875` was published on
`codex/sn-020-schema`. [PR #19](https://github.com/RicardoKers/SimNodus/pull/19)
is the integration and hosted-check record. Follow that record for the resulting
squash commit and final status; source-branch publication alone does not update main.

## SN-021 source integration: 2026-09-13

The first bounded local-resource reference slice is published on
`codex/sn-021-local-resources`, starting with source commit
`cb6fc8d977b725d2d2c60d7aa59c2fb8efa23d2c`. Both Foundation jobs passed in
[run 34785300902](https://github.com/RicardoKers/SimNodus/actions/runs/34785300902).
[PR #20](https://github.com/RicardoKers/SimNodus/pull/20) is the final hosted-check
and squash integration record. Confirm its final state and main identity before
claiming integration. Foundation exposes verbose CTest summaries to distinguish
filesystem passes from OS/privilege skips. This slice does not close SN-021 or
publish a simulator, binary, new supported engine profile or release.

## Binary release gate

Record backend/runtime revisions and licenses, supported Windows versions, installer provenance, checksums, and tested lesson projects. Add required notices and source/relinking materials for dependencies as applicable. Do not redistribute CubeIDE, vendor firmware, model packs, or documentation by assumption.

Use `0.x` prereleases until compatibility expectations and project-format migrations are established. Publish limitations beside the download. Keep January's classroom candidate fixed except for reviewed fixes.
