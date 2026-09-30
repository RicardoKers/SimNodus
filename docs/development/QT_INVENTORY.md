# Qt module and licensing inventory

Recorded: 2026-09-30. Status: SN-022 local development inventory; binary
redistribution remains pending. Original SimNodus code remains MIT. This review
uses the existing [licensing policy](LICENSING.md) and does not approve shipping
the installed Qt directory.

## Selected experimental boundary

The bounded experiment selects Qt 6 Widgets with **Core, Gui and Widgets** in
presentation only. Core supplies the asynchronous `QProcess` presentation bridge;
the worker, domain, instrumentation and backend adapters require no Qt. The
Release experiment uses the Windows platform plugin and explicitly selects the
built-in Fusion style. No optional style, plotting or QML module is selected.

| Item | Role | Inspected license options |
|---|---|---|
| Qt Core | Event loop, process notifications, presentation data conversion | Commercial, LGPL-3.0-only, GPL-2.0-only or GPL-3.0-only |
| Qt Gui | Window system and drawing used by Widgets | Same options |
| Qt Widgets | Independent windows, docks, splitters and preview shell | Same options |
| QWindowsIntegrationPlugin | Native Windows platform integration | Same options; separate Wintab attribution also applies |
| EntryPointPrivate | Windows entrypoint helper when linked by the build | Commercial or BSD-3-Clause |

These are the expressions in the installed 6.11.1 SBOM. Official
[Core](https://doc.qt.io/qt-6/qtcore-index.html#licenses-and-attributions),
[Gui](https://doc.qt.io/qt-6/qtgui-index.html#licenses-and-attributions) and
[Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html#licenses) documentation
confirms the LGPLv3 option. Dynamic linking under that option is the proposed
distribution route, subject to the release review below.

Widgets directly supplies [dock layout and state restoration](https://doc.qt.io/qt-6/qmainwindow.html)
and [collapsible splitters](https://doc.qt.io/qt-6/qsplitter.html). Qt Quick remains
a possible future presentation choice; this experiment does not establish a
performance comparison or a general preference for all UI workloads.
[Qt Charts](https://doc.qt.io/qt-6/qtcharts-index.html#licenses) and
[Qt Graphs](https://doc.qt.io/qt-6/licensing.html) offer GPLv3/commercial licensing
rather than the selected LGPL route and are excluded. No plotting library,
WebEngine, Quick/QML, Network, Concurrent or third-party docking library is added.

## Exact installed evidence

The [machine-readable inventory](../experiments/evidence/SN-022-qt-inventory.json)
records SHA-256 hashes of the configuration, both qtbase SBOMs, license files,
selected Release DLLs, platform plugin and entrypoint library. It records direct
PE imports and the selected packages' declared SPDX dependency closure.

The existing kit identifies itself as `6.11.1/msvc2022_64`, x86_64, shared,
Debug/Release, built with MSVC `19.39.33520`. Its qtbase SBOM references commit
`59c81a3c2247b821b9b84b4eb8d939b77e07e276`. The binary SBOM hash is
`c8ceace4efa14d933d09d8a58a279992be257d1c8e889152ae972346f07be5e3`.
These identify inspected local bytes and package declarations; the original
download and trusted origin were not independently established here. No upstream
source, DLL, plugin, license text or complete SBOM is copied into the repository.

The official pages inspected now describe Qt 6.11.2. Exact dependency versions
below come from the installed 6.11.1 SBOM, not those newer pages. Qt documents
its [SBOM format and scope](https://doc.qt.io/qt-6/sbom.html).

## Third-party notices and runtime limits

| Declared dependency/attribution | Installed version or category | Recorded terms |
|---|---|---|
| PCRE2 and its JIT | 10.47 | BSD-3-Clause with PCRE2 binary-like package exception; JIT BSD-2-Clause |
| zlib / libpng | 1.3.2 / 1.6.58 | Zlib / Libpng AND libpng-2.0 |
| FreeType | 2.14.3, plus rasterizer and format support | FTL or GPL-2.0-only; format support also has MIT, MIT-open-group and Zlib notices |
| HarfBuzz / MD4C | 14.2.0 / 0.5.2 | MIT |
| Unicode tables / CLDR | 36 / v48.1 | Unicode-3.0 |
| Hashing, number conversion, CBOR and easing | Exact entries in JSON | CC0, BSD, MIT and public-domain license references, varying by component |
| MIME definitions and emoji segmentation | Exact entries in JSON | Apache-2.0 |
| Graphics headers, allocation, scaling and color data | Exact entries in JSON | MIT, Apache-2.0, BSD, Imlib2, X11, HPND and ICC license reference, varying by component |
| Windows tablet interface | Wintab, version unknown | LCS/Telegraphics license reference |

The JSON preserves the exact SPDX names, versions and concluded expressions;
the table groups them only for readability. These declarations are an inventory,
not a completed review of every notice or a trace of all loaded code. For example,
the platform-plugin SBOM declares OpenGL, while the inspected `qwindows.dll`
direct import table contains only Core and Gui among Qt DLLs.

The inspected Core DLL also imports `icuuc.dll`, absent from this kit's `bin`
directory. Actual Windows ICU resolution, Microsoft C++/UCRT dependencies,
system libraries, optional/delayed loads and any packaged runtime remain separate
release checks. Direct PE imports do not establish the complete runtime closure.
No engine/runtime or numerical acceptance claim follows from this inventory.

## Pending release obligations

Before a binary release, establish the exact acquired package/source provenance,
complete the runtime and plugin inventory, and retain all applicable copyright
and license notices. Verify the selected LGPLv3 distribution mechanism, matching
corresponding source availability, replacement/relinking and installation
requirements, and permission for debugging modifications. Review each embedded
third-party term and Microsoft runtime distribution separately. Qt's
[licensing overview](https://doc.qt.io/qt-6/licensing.html) explicitly distinguishes
module terms, third-party code, tools, documentation and examples.

Build tools and entrypoint helpers also need their applicable notices; a local
development installation is not a redistribution decision. Packaging and clean
offline installation remain SN-027 gates. Dynamic linking alone does not close
those gates, and no redistribution permission is asserted by SN-022.
