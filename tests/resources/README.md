# SN-021 local resource verification

The [native project acquisition tests](../../docs/experiments/SN-021-acquisition.md)
check bounded capture, physical path rejection, concurrent changes and graph/source
ownership. Declared resources remain unopened; the result grants no save authority.

The [overwrite counterexamples](../../docs/experiments/SN-021-overwrite-boundary.md)
add `windows-overwrite-boundary`: seven real NTFS cases rejecting unsafe candidate
replacement protocols. This is an acceptance guard, not a production overwrite API.

The [project creation tests](../../docs/experiments/SN-021-save-create.md) add
native save contracts and real Windows temporary/rename/collision/failure checks.
They regress the shared containment helpers without changing resource verification
policy. A killed writer's orphan temporary is explicitly recorded, not hidden.

The [native extraction](../../docs/experiments/SN-021-native-resources.md) adds
`native-resource-contracts` and Windows `native-resource-filesystem` CTest entries.
The [native regression driver](native_regression.py) compares actual snapshots
with the preserved reference and schedules real filesystem mutations through
an explicitly selected test-only probe. See the
[native contract](../../docs/architecture/NATIVE_RESOURCE_VERIFICATION.md).
The Python implementation below remains unchanged historical reference code.

The explicit [snapshot verifier](local_resources.py) consumes lock 0.1 metadata
and a caller-selected absolute root. It returns immutable, verified bytes and
never renders or executes them. The [contract](../../docs/architecture/LOCAL_RESOURCE_VERIFICATION.md)
defines the Windows/NTFS boundary; [results](../../docs/experiments/SN-021-local-resources.md)
record actual coverage and skips. This is a reference filesystem operation, not
a native project loader or an executable project profile.

```powershell
python -m unittest discover -s tests/resources -p 'test_*.py' -v
python tests/resources/local_resources.py tests/schema/fixtures/owned-resource-lock.json --root D:/03_Projects/01_Actives/SimNodus
```

Replace the root with the actual absolute checkout directory. The owned lock
verifies two inventoried resources; the project fixture's embedded lock verifies
three through the API. Do not reopen a snapshot's path for later consumption.
The CLI prints only a summary and discards its snapshots. Metadata validation
through the existing schema CLIs never invokes this operation automatically.

Tests use disposable owned files under `build/sn021-tests`, actual Windows
handles, links and junctions. Deterministic interposition schedules real attempted
filesystem mutations at open/read boundaries; it does not emulate the filesystem.
Symlink tests require Windows privileges; hosted Windows acceptance fails if
those privileges are unavailable. Short-name coverage reports a skip when the
volume does not generate 8.3 aliases. Linux exercises only input rejection and
unsupported-platform rejection; no Linux filesystem support is claimed.

CTest exposes `local-resource-snapshots` separately from the unchanged 94-case
`schema-declarations` suite. No engines, compilers, renderers, downloads or
project-selected native libraries are invoked by the verifier or its tests.
