# SN-021: physical resource-root binding for managed documents

Date: 2026-09-28. Status: **bounded local physical prerequisite passed;
managed lifecycle composition pending**.

This is the smallest prerequisite for composing explicit managed import and
fixed E-01 replay. The import must capture the selected document and its root's
NTFS volume/file identity from the same retained root handle. A managed context
may persist that identity and the original external locator, but neither field
grants filesystem authority by itself. A later explicit compile must compare the
persisted expected identity with `FileIdInfo` from the same retained root handle
used to open and verify each declared resource. It must reject a replaced root
even when the replacement contains byte-identical resources. It must not reopen
the root solely to compare identities or use a textual path prefix as proof.

The predeclared local acceptance is: unchanged ordinary acquisition and resource
verification; same-handle observed identity for a valid imported document;
bound compilation of the accepted fixed replay when the root is unchanged;
identity refusal before child-resource reads when the root is replaced; refusal
of aliases, reparse points, missing files, size/hash mismatches and existing
budgets under the established physical policy. Existing source mapping, temporal
policy, 10 microvolt and 1 ps bounds, and readiness flags must remain unchanged.
An unbound standalone compiler remains a separate explicit operation; managed
consumption must select the bound variant.

No import, Save, Open, export or compilation automatically starts an engine,
loads firmware or native models, downloads dependencies, or proves trusted origin,
interface compatibility or redistribution rights. The later composed lifecycle
needs actual authenticated request/store evidence and real ngspice consumption
of its resulting fixed E-01 netlist.

## Measured local result

The additive APIs return `PhysicalRootIdentity` with the acquired graph and
require an expected identity for bound resource verification and fixed replay.
The existing unbound APIs remain explicit. On this Windows/NTFS host, 18 native
acquisition regressions ran (16 passed, two platform/privilege skips), 34 native
resource regressions ran (31 passed, two symlink privilege skips and one 8.3
alias availability skip), and all 20 lifecycle regressions passed. The adversarial
cases moved the selected root after acquisition and installed byte-identical
resources at the same locator. The captured identity remained the old root's;
bound verification and fixed replay returned `root_identity` before child
resource access. Wrong expected identity also won over a missing resource.

One intermediate direct resource-test run failed because `GetLongPathNameW`
returned access denied for an ancestor of the system TEMP directory under the
restricted process. The alias fixture was moved under the owned repository test
directory. Its volume did not generate an 8.3 alias, so that optional case now
reports a skip; earlier physical alias evidence is preserved. This setup issue
did not affect the new root-replacement cases.

The explicit real [fixed replay harness](../../tests/experiments/ngspice/fixed_replay_acceptance.py)
passed using the updated bound lifecycle probe. The generated original and
revised documents differed only at the name token; policy, source mapping and
pre/post-save netlist matched. Ngspice consumed the post-reopen netlist, emitted
5,012 samples, reached 0.004999999999999989 s, and had a maximum analytical
error of 9.889724283951296e-08 V under the unchanged 10 microvolt and 1 ps
project limits. The raw run is retained under
`build/sn021-root-binding-engine-01`; no dependency was downloaded.

The full MSVC Debug build and all 55 CTests passed. The repository checker
passed for 812 text files, and `git diff --check` passed. The committed
[bounded evidence summary](evidence/SN-021-root-binding-summary.json) records
the raw engine-result hash, exact limits and test counts. All 185 previously
recorded historical evidence hashes and ten untouched SN-045 overlay files
still match their prepared baseline; the two edited planning files retain
their separate SN-045 sections.

This does not exercise a managed store, authenticated Save, import/export, or
an engine consuming a *managed* revision. SN-021 remains in_progress.
