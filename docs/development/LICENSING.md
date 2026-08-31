# Licensing and redistribution

## Original SimNodus material

The owner authorized a permissive license for broad use, copying, and modification. Original project code and associated documentation use [MIT](../../LICENSE). The owner confirmed **Ricardo Kerschbaumer** for initial copyright and author attribution. Future contributors retain rights in their contributions; this is not a copyright assignment.

MIT allows commercial use and modification, subject to retaining its notices, and does not require derivatives to publish their source. See the [OSI license text](https://opensource.org/license/mit) and [GitHub license summary](https://choosealicense.com/licenses/mit/). This fits the requested broad reuse; it does not guarantee freedom from legal disputes.

The license contains warranty/liability disclaimers. It does not establish trademark rights, certify safety, or replace rights/terms attached to third-party content. This policy is project guidance, not legal advice.

## Dependency policy

| Material | Policy |
|---|---|
| Renode | Review the selected release and bundled libraries; its top-level MIT terms do not cover every dependency |
| ngspice/XSPICE | Review the exact build's notices and exceptions, not just the general BSD description |
| Qt 6 | Inventory each module and runtime; prefer a reviewed LGPL-compatible dynamic-linking distribution for the MIT application |
| Firmware/toolchains | Distribute owned source/build recipes; verify startup, HAL/CMSIS, toolchain and example terms separately |
| Vendor SPICE models | Include only with verified redistribution rights and attribution |
| WASM/HDL libraries | Review runtime/compiler/model terms when actually selected |
| Symbols/fonts/images | Prefer original or clearly licensed assets; record provenance |

Qt offers different licensing options and some modules are GPL-only under its open-source offering. Do not select plotting or other modules merely because they are part of Qt. [Official Qt licensing](https://doc.qt.io/qt-6/licensing.html). Dynamic linking alone is not a complete compliance checklist.

## Before adding an external asset

Record name, exact revision/hash, source URL, author/copyright notice, license, modifications, and whether it will be linked, executed separately, or redistributed. Preserve required notices. If terms are unclear, leave the asset out and document an installation/reference procedure instead.

No engine binaries, vendor models, firmware packages, fonts, or copied upstream source are bundled in the initial repository. The checkout action is referenced by commit for hosted CI, not vendored.

SN-019 subsequently adds a source-transformation recipe and the [upstream Renode MIT terms](../../tests/experiments/renode-client/UPSTREAM-LICENSE.txt). Its [manifest](../../tests/experiments/renode-client/upstream.json) records exact official sources and hashes. Generated files retain Antmicro and Realtime Embedded notices and remain in ignored build output. This limited source adaptation does not approve shipping the complete Renode runtime or its other dependencies.

E-02's firmware, linker script, platform subset, and host probe are original MIT project material. Matching Renode model sources are fetched only for read-only audit and remain ignored with upstream notices. The Arm/CubeIDE compiler is a development tool; the freestanding ELF links no compiler runtime or C library. Preserve toolchain package licenses and do not redistribute vendor tools or manuals without a separate review.

## Publication and maintenance

Review attribution and private-data removal before source publication. Review the full packaged dependency set before binary releases. Keep legal names/contact details owner-confirmed; do not invent them. Revisit this policy when packaging or dependencies change.
