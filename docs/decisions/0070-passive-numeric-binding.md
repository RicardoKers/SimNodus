# ADR 0070: Bind passive values exactly before numerical backend lowering

- Status: Accepted for the bounded SN-021 slice
- Date: 2026-09-15

## Context

Source-interface correspondence alone deliberately accepts defaults outside a
descriptor range. Metadata effective values already resolve exactly, and losing
their precision/provenance during compilation would weaken the existing contract.

## Decision

Add the pure [numerical binding contract](../architecture/PASSIVE_NUMERIC_BINDING.md).
Revalidate correspondence rather than accepting caller-constructed records. Convert
recognized SPICE defaults with exact decimal arithmetic and require positive,
in-range source defaults and reachable effective values. Bind explicit component
maps to actual source order and preserve occurrence paths and value provenance.
An unused or overridden source default is still checked; no silent fallback.

## Consequences

Declaration validity stays unchanged, including zero/negative metadata quantities.
Those values fail this positive R/C operation when effective. No binary floating-
point or solver accuracy claim follows from exact mathematical values. Engine
lowering and execution authority remain separate, as do physical containment and
safe overwrite. Existing engine profiles, SN-017 boundaries and UI stay unchanged.
