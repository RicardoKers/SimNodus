# Bounded passive SPICE source inspection

SN-021 adds an inert adapter-level reader of caller-supplied bytes. It owns the
complete accepted source and records entrypoint names, ordered formal terminals,
one parameter/default literal, R/C primitive kind and source spans. No filesystem,
engine, downloader, renderer or project-opening path is invoked.

Accepted grammar consists of full-line ASCII `*` comments, blanks and three-line
subcircuits: `.subckt name first second parameter=literal`, exactly one R or C
primitive using those terminals in order and `{parameter}`, and `.ends name`.
Comments/blanks may intervene. Identifiers have 1–64 ASCII letters, digits or
underscores, beginning with a letter/underscore. Names compare case-insensitively;
entrypoints must be unique and terminals distinct. No positional inference occurs.
Reserved parameter names `m`, `temp`, `temper`, `time`, `hertz`, `pi`, `e` are rejected.

Positive literals contain 1–16 mantissa digits, an optional decimal point, optional
signed exponent of magnitude at most 18 (one/two digits), and optional SPICE suffix
`t`, `g`, `meg`, `k`, `m`, `u`, `n`, `p`, `f`. All-zero mantissas are rejected.
The literal is retained, not evaluated or checked against descriptor units/ranges.
Limits: 65536 bytes, 1024 lines, 1024 bytes per line before removing trailing CR,
32 subcircuits. LF and CRLF are accepted. Every unsupported byte/construct rejects
the entire operation. Spans include the header through the matching ends line,
excluding its LF (including its CR when present).

This is a deliberately narrow source grammar, not a general SPICE parser. It
does not certify arbitrary accepted literal semantics, complete interface
compatibility, trusted origin, redistribution permission or execution authority.
`resource_interfaces_verified` remains false. Physical containment and hashes are
separate gates; consumption must use the captured owned bytes, never reopen a
verified pathname. Retained-handle acquisition remains unchanged.

## Predeclared integration acceptance

The explicit test runner may use only the existing owned `passive.cir` fixture,
captured through the native physical verifier against the existing lock. The
reader must accept it before materialization. Run the existing E-01 host with
the pinned ngspice 47 runtime, no downloads. Explicit harness authority supplies
ground, a 3.3 V source, 1 kohm/1 uF values, zero initial output and the existing
5 ms transient/1 us step/options. Acceptance reuses E-01 analytical comparison:
finite monotonic samples, over 100 samples, endpoint within 1 ps, maximum voltage
error at most 0.0165 V, successful process and idle shutdown. Preserve all attempts.
This proves only these owned R/C definitions in the existing ideal RC profile;
it does not compile a project or authorize arbitrary model execution.

Next: explicitly bind recognized models to descriptors, ordered maps, units/ranges
and effective parameters, then define reference/stimulus/analysis authority and
backend lowering. Full SN-021 and safe overwrite remain pending.

References: [resource links](RESOURCE_LINKS_DRAFT.md),
[ngspice documentation](https://ngspice.sourceforge.io/docs.html),
[ADR 0068](../decisions/0068-passive-source-inspection.md).
