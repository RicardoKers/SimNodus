# ADR 0004: License, language, and platform

Date: 2026-08-31. Status: accepted under explicit owner authorization.

## Context

The owner requires English project files, Windows first and Linux later, and a public project that anyone can use, copy, and modify. The owner authorized selecting the license.

## Decision

License original SimNodus code and associated documentation under MIT. Preserve copyright/license notices and document third-party terms independently. Use the collective attribution `SimNodus contributors` until the owner supplies a preferred attribution.

All committed project text, documentation, comments, and templates are English. The conversation with the owner can remain Portuguese. Develop the Windows application first while keeping domain/core portable; Linux application packaging comes later.

## Consequences

MIT permits broad reuse, including proprietary derivatives, without requiring changes to be shared. It contains warranty/liability disclaimers but cannot guarantee absence of legal problems. Qt modules, firmware, vendor models, and native dependencies need individual review; MIT does not replace their licenses.

## Alternatives considered

Apache-2.0 provides explicit patent-related provisions but is longer and adds compliance detail. GPL-family licensing enforces sharing in relevant distributions, which the owner did not request. MIT is the selected simple permissive baseline, not a legal assurance.

## Revisit when

The owner changes distribution goals or a required dependency imposes incompatible conditions. Do not casually relicense existing contributions.

## Publication follow-up — 2026-08-31

The owner subsequently confirmed Ricardo Kerschbaumer as the author and authorized publication at `RicardoKers/SimNodus`. The initial MIT copyright line now uses that name instead of the provisional collective attribution. The license choice is unchanged; future contributors retain their own rights.
