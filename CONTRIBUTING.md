# Contributing to SimNodus

SimNodus is at the architecture/scaffolding stage. Start with [current work](docs/planning/CURRENT.md), [architecture](docs/architecture/README.md), and the [backlog](docs/planning/BACKLOG.md). There is no simulation application to build yet.

## Contribution workflow

1. Identify an existing `SN-xxx` task or describe the problem in an issue.
2. Keep changes focused. Discuss backend, time, format, or public-API changes through an ADR.
3. Write all code, comments, documentation, templates, and committed project text in English.
4. Run `python tools/check_repository.py` and relevant implementation checks.
5. Explain the problem, change, evidence, and limitations in the pull request.
6. Update affected requirements, reports, task status, and current-state notes.

Use descriptive branch names such as `docs/project-format` or `experiment/renode-gpio`. Commit messages should be concise English statements; optional prefixes include `docs:`, `build:`, `feat:`, `fix:`, and `test:`. Never commit credentials, personal paths, private student material, or generated firmware/build output.

## Evidence standards

Do not describe mocked results as integration tests. Backend work must report exact versions, reproducible inputs, units, and tolerances. Follow the [quality policy](docs/development/QUALITY.md). A documentation-only change does not need simulator tests.

## Rights and conduct

Submit only material you have the right to contribute. Original contributions are offered under the project's MIT license unless clearly identified third-party material has been reviewed with its original terms. This is not a copyright assignment.

Be respectful, keep discussion focused on the work, and do not publish personal information. Security-sensitive reports follow [SECURITY](SECURITY.md). Maintainer contact and repository-specific links will be finalized at publication.
