# Security policy

## Project status

There are no released or supported simulator versions yet. This repository currently contains design documents, scaffolding, and development checks. No untrusted-code sandbox is implemented.

## Reporting a vulnerability

Do not put exploit details, credentials, private firmware, or personal information in a public issue. Once the repository is published, use GitHub's private vulnerability reporting if it is enabled. The maintainer must enable that channel or publish a private contact before accepting external security reports.

If no private channel is available, request a contact through a public issue **without sensitive details**. No response-time guarantee is currently offered.

## Intended trust boundaries

Treat circuit projects, component packages, ELF files, SPICE commands/includes, symbols, archives, native code models, WASM, and HDL build inputs as untrusted. Validate paths, sizes, recursion, resource consumption, and execution permissions.

Opening a project must not implicitly download resources or execute host programs. Native dependencies can affect the host; process isolation alone is not a sandbox. Bind development/debug servers to loopback and verify actual behavior.

## Dependency and release policy

Pin and review backend revisions. Track licenses and origin. Do not redistribute proprietary firmware or vendor models without permission. Public CI must not execute untrusted pull-request content with privileged tokens or access to private environments.

Before binary releases, add malicious-input, timeout, crash-recovery, and packaging reviews as described in [QUALITY](docs/development/QUALITY.md). The simulator is an educational tool, not a safety-certification system.
