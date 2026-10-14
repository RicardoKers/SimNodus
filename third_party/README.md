# Third-party material

| Folder | Content | Version | License |
|---|---|---|---|
| `nlohmann/` | `json.hpp`, single-header JSON library, unmodified | 3.11.3 (release asset, SHA-256 `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`) | MIT, [LICENSE.MIT](nlohmann/LICENSE.MIT) |
| `renode_external_control/` | Renode external control C client, adapted (native loopback transport, per-connection callbacks); [provenance](renode_external_control/README.md) | Renode 1.16.1, commit `d66b0c2a` | MIT (Antmicro), [LICENSE.txt](renode_external_control/LICENSE.txt) |

No simulation engine binaries, vendor models, firmware packages, or visual
assets are vendored here. The only upstream source is the Renode client above;
its matching server lives in `resources/renode/`.

Before adding anything, record origin, immutable revision/hash, license,
copyright notices, modifications, and redistribution obligations in the
[dependency inventory](../docs/research/DEPENDENCIES.md). Preserve upstream
license files. The root MIT license applies to original SimNodus material, not
automatically to external assets.
