"""Download, verify and stage the pinned Windows backends for development.

Uses the archives pinned in tools/backend-baseline.json, checks every SHA-256
before extraction and stages the ngspice runtime in the layout the
application package uses:

  build/deps/ngspice-runtime/
    ngspice.dll, libomp140.x86_64.dll, sndfile.dll, samplerate.dll,
    spinit (owned, resources/ngspice/spinit), COPYING (ngspice license)

On Linux ngspice is not downloaded: install the distribution library
(Debian/Ubuntu: libngspice0-dev) instead.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "tools" / "backend-baseline.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, destination: Path, expected: str) -> None:
    if destination.is_file() and sha256(destination) == expected:
        print(f"verified {destination.name}")
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_suffix(destination.suffix + ".part")
    for attempt in range(1, 5):
        print(f"downloading {url} (attempt {attempt})")
        request = urllib.request.Request(url, headers={"User-Agent": "SimNodus-fetch-backends"})
        try:
            with urllib.request.urlopen(request, timeout=300) as response, partial.open("wb") as output:
                shutil.copyfileobj(response, output)
            break
        except OSError as error:
            # SourceForge mirrors occasionally throttle or drop a transfer.
            print(f"  failed: {error}")
            if attempt == 4:
                raise SystemExit(f"Cannot download {url}")
            time.sleep(5 * attempt)
    actual = sha256(partial)
    if actual != expected:
        partial.unlink()
        raise SystemExit(f"SHA-256 mismatch for {destination.name}: {actual}")
    partial.replace(destination)
    print(f"verified {destination.name}")


def extract_7z(archive: Path, target: Path) -> None:
    target.mkdir(parents=True, exist_ok=True)
    candidates = [shutil.which("7z"), r"C:\Program Files\7-Zip\7z.exe"]
    for tool in candidates:
        if tool and Path(tool).is_file():
            subprocess.run([tool, "x", str(archive), f"-o{target}", "-y"], check=True, stdout=subprocess.DEVNULL)
            return
    # Windows 10/11 ship libarchive's bsdtar, which reads 7z archives.
    bsdtar = Path(r"C:\Windows\System32\tar.exe")
    if bsdtar.is_file():
        subprocess.run([str(bsdtar), "-xf", str(archive), "-C", str(target)], check=True)
        return
    raise SystemExit("No 7z extractor found (install 7-Zip).")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--deps", type=Path, default=ROOT / "build" / "deps")
    args = parser.parse_args()
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    archives = {Path(a["path"]).name: a for a in baseline["archives"]}
    deps = args.deps.resolve()
    if sys.platform != "win32":
        print("Linux: install the distribution ngspice library (libngspice0-dev).")
        return 0
    wanted = ["ngspice-47_dll_64.7z", "ngspice-47_64.7z"]
    for name in wanted:
        record = archives[name]
        download(record["url"], deps / record["path"], record["sha256"])

    extract_7z(deps / "downloads/ngspice-47_dll_64.7z", deps / "ngspice")
    extract_7z(deps / "downloads/ngspice-47_64.7z", deps / "ngspice-console")
    files = {f["path"]: f["sha256"] for f in baseline["files"]}
    runtime = deps / "ngspice-runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    staged = {
        "ngspice/Spice64_dll/dll-vs/ngspice.dll": "ngspice.dll",
        "ngspice/Spice64_dll/dll-vs/libomp140.x86_64.dll": "libomp140.x86_64.dll",
        "ngspice-console/Spice64/bin/sndfile.dll": "sndfile.dll",
        "ngspice-console/Spice64/bin/samplerate.dll": "samplerate.dll",
    }
    for source, name in staged.items():
        path = deps / source
        if sha256(path) != files[source]:
            raise SystemExit(f"Extracted file does not match the baseline: {source}")
        shutil.copy2(path, runtime / name)
    shutil.copy2(deps / "ngspice-console/Spice64/docs/COPYING", runtime / "COPYING")
    shutil.copy2(ROOT / "resources/ngspice/spinit", runtime / "spinit")
    print(f"staged ngspice runtime in {runtime}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
