# Third-party material

| Folder | Content | Version | License |
|---|---|---|---|
| `nlohmann/` | `json.hpp`, single-header JSON library, unmodified | 3.11.3 (release asset, SHA-256 `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`) | MIT, [LICENSE.MIT](nlohmann/LICENSE.MIT) |
| `esp-rom-elfs/` | `esp32c3_rev3_rom.elf`, Espressif's ESP32-C3 ROM ELF (chip revisions v0.3 and v0.4), unmodified; the emulator rebuilds the 384 KiB mask ROM from it (`src/emu/esp32c3_rom.hpp`) | espressif/esp-rom-elfs release `20260528` (commit `37b4a88f`), SHA-256 `19ac22e08707df926fb0cf4c54795d4067b983fae8635f396ded173a6d78fc3c` | Apache-2.0 including the compiled form, [LICENSE](esp-rom-elfs/LICENSE); Copyright (c) 2015-2022 Espressif Systems (Shanghai) Co. Ltd.; the ROM contains newlib (BSD), listed on ESP-IDF's copyright page |

No simulation engine binaries, vendor models, firmware packages, or visual
assets are vendored here, except the ESP32-C3 ROM ELF above (owner's decision
DE0 of 2026-10-28, experiment E-08). (The Renode client that lived here was removed with
Renode, ADR 0094.)

Before adding anything, record origin, immutable revision/hash, license,
copyright notices, modifications, and redistribution obligations in the
[dependency inventory](../docs/research/DEPENDENCIES.md). Preserve upstream
license files. The root MIT license applies to original SimNodus material, not
automatically to external assets.
