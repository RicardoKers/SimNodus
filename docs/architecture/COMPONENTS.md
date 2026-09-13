# Components and subcircuits

## Component model

SN-020 now has an [experimental topology 0.1 draft](TOPOLOGY_DRAFT.md) with a
reference validator and hierarchical fixtures, followed by a separate
[parameter/override 0.2 draft](PARAMETERS_DRAFT.md) and
[symbol/model interface 0.3 draft](BINDINGS_DRAFT.md). Full packages, graphic/model
resources and implementations below remain design requirements. Valid interface
declarations do not establish simulation support.

A package describes identity, version, author/origin, license, category, unit-bearing parameters, stable pins, symbol, and behavior models. Visual representations can change without changing electrical connectivity. Package pin number, signal name, and alternate function are separate data.

Proposed layout, not a stable schema:

```text
component-package/
  component.json
  symbols/default.svg
  models/...
  README.md
  LICENSE
  examples/...
```

Bundled packages must have verified origin and license terms. Supported model categories are intended to include SPICE/XSPICE, MCU/Renode, trusted built-in behavior, and later WASM/HDL. The manifest declares required capabilities and pin mappings. Missing models cause explicit errors, not silent replacement with an ideal component.

Vendor SPICE files may require dialect adaptation and may restrict redistribution. A file extension proves neither compatibility nor permission.

## Subcircuit workflow

1. Define a circuit and external ports with stable IDs, names, domains, and descriptive directions.
2. Save the definition in the project's local library.
3. Refresh the component list and insert the subcircuit.
4. Insert another instance with independent state and parameter overrides.
5. Export/copy its package to a user library for reuse elsewhere.

Discovery reads manifests without executing models. Proposed lookup precedence: explicitly locked project dependency, local package, user library, bundled library. Report ID/version/hash conflicts instead of silently changing a model.

## Hierarchy and identity

Preserve hierarchy in the project representation. Compilation may flatten a circuit for ngspice, retaining unique instance names and provenance for diagnostics/instruments. Two instances never share state just because they use the same definition.

Validate missing ports/nets/dependencies, direct or indirect recursion, and excessive depth. Do not connect every pin named `GND` implicitly: global references, if supported, must be explicit. Removing ports in a definition requires migration or an error.

Saving a definition must not change other projects. Detect dependency changes by version/content, and require an explicit update action.

## Future extensions

WASM receives only granted capabilities, bounded memory/execution, virtual time, and controlled randomness. Disable filesystem/network access by default. Compiled HDL and native code models need isolation and trust policy; an adapter is not a sandbox.

There is no public plugin ABI yet. Avoid exposing C++ ABI. Start with a simple component and a reusable RC subcircuit instantiated twice.
