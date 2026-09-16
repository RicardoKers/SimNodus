# Exact passive numerical binding

SN-021's `bind_passive_numeric` takes complete resource-links metadata, an exact
descriptor ID and stable caller-supplied source bytes. It reruns the selected
[interface correspondence](PASSIVE_INTERFACE.md), including whole-document
validation, bounded source recognition and exact size/hash matching. No caller-
constructed correspondence record can bypass these gates.

## Numerical and occurrence contract

Decode the selected R/C source default into decimal digits and a base-ten exponent.
SPICE suffixes shift that exponent: T +12, G +9, MEG +6, K +3, M -3, U -6,
N -9, P -12, F -15, case-insensitively. The existing grammar bounds all inputs.
Compare values by sign, decimal order and padded digits without binary floating-
point conversion. Return the default as a canonical fixed decimal string, removing
insignificant trailing fractional zeros. Preserve its original spelling in source.

The source default must be strictly positive and within the selected descriptor's
inclusive minimum/maximum, even if every occurrence overrides it or no reachable
occurrence uses the descriptor. Do not replace an invalid default silently.
This is a new operational gate, not a change to declarative validity.

For each reachable component occurrence explicitly bound to the selected descriptor,
compose the component parameter map with the source parameter correspondence.
Reuse the existing exact effective-value resolver, including containing-circuit
inheritance and unit normalization. Require the effective value to be positive
and within the descriptor range. Existing declaration validation already proves
component range containment and unit agreement. Preserve the effective value,
unit, origin and offset exactly; do not normalize away its meaningful provenance.
Compose pin maps into actual source-formal order and return logical pin IDs,
definition, full occurrence path and instance offset. Sort by full path. An unused
descriptor can produce an empty occurrence list after its default passes.

Output owns the complete correspondence and original inputs. Default errors use
the selected source subcircuit's begin offset; occurrence errors use the metadata
effective-value origin offset. Delegated interface errors retain their stage and
offset. No partial binding is returned after a failed gate.

## Acceptance and boundaries

Compare suffix/exponent conversions against Python Decimal with precision 100;
test exact inclusive boundaries and values immediately outside them, 32-digit
effective values, inherited and component defaults, zero/negative rejection,
reversed source maps, reordered metadata, unused descriptors and delegated errors.
Verify owned inputs, stable occurrence identities, source offsets and false flags.

This is mathematical binding only. It does not prove ngspice binary rounding,
numerical stability or fidelity for arbitrary accepted values. The existing
[real owned RC evidence](../experiments/SN-021-passive-source.md) remains the
accepted ideal RC profile. No netlist or engine call is added, and no electrical
profile/tolerance changes. Physical containment, all other assets/interfaces,
source trust, redistribution permission and explicit execution authority remain
separate gates. All declaration readiness flags remain false.

Next define explicit reference/stimulus/analysis authority and bounded backend
lowering using complete project validation, structural connectivity and verified
captured resources. Obtain real-engine evidence for that integration. Safe
overwrite remains pending ADR 0064; full SN-021 is not complete.

See [ADR 0070](../decisions/0070-passive-numeric-binding.md).
