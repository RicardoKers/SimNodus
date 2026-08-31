# GitHub publication checklist

The owner authorized the first public source publication at `RicardoKers/SimNodus`, with Ricardo Kerschbaumer as author. No raw private conversations or binary simulator release are included. See [current state](../planning/CURRENT.md) for progress.

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
- [ ] Initial commit reviewed and recorded.
- [ ] Remote created as public and initial push authorized/performed.
- [ ] Hosted CI observed; private vulnerability reporting/contact configured.

Do not publish raw chat transcripts. The research synthesis is sufficient and does not expose private conversation identifiers.

## Proposed remote setup

Repository: [RicardoKers/SimNodus](https://github.com/RicardoKers/SimNodus). Description: “Open-source mixed-signal and embedded systems simulator, starting with STM32.”

Suggested topics: `simulation`, `electronics`, `stm32`, `ngspice`, `renode`, `education`, `cpp`, `qt`. Default branch: `main`.

Use a reviewed initial commit and an empty remote, then add the remote and push explicitly. Do not generate an unrelated remote README/license that creates an avoidable history conflict. The owner has authorized these steps. Formal trademark/domain clearance remains open; the preliminary public-name check is not legal clearance.

## Collaboration setup

Create milestones from the roadmap and issues from ready backlog tasks, retaining `SN-xxx` IDs. Suggested labels: `type:bug`, `type:feature`, `type:experiment`, `type:docs`, `priority:P0`, `platform:windows`, `platform:linux`.

Protect `main` with the checks actually produced by CI after its first successful run. Do not invent CODEOWNERS usernames or require multiple maintainers in a one-person project. Dependency-update PRs should be reviewed, not automatically merged.

## Before a binary release

Record backend/runtime revisions and licenses, supported Windows versions, installer provenance, checksums, and tested lesson projects. Add required notices and source/relinking materials for dependencies as applicable. Do not redistribute CubeIDE, vendor firmware, model packs, or documentation by assumption.

Use `0.x` prereleases until compatibility expectations and project-format migrations are established. Publish limitations beside the download. Keep January's classroom candidate fixed except for reviewed fixes.
