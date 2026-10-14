"""Build the portable Windows package (zip) from a configured build tree.

Steps: install the build into a staging folder, deploy the Qt runtime with
windeployqt, check that the bundled ngspice runtime is present, run the
packaged executable's --check-engine and --smoke-test (it opens windows
briefly), and write
dist/SimNodus-<version>-windows-x64.zip with a top-level SimNodus-<version>/
folder.

usage: package_windows.py --build-dir build/release [--config Release]
                          [--windeployqt PATH] [--output dist]
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "simnodus.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "platforms/qwindows.dll",
    "ngspice/ngspice.dll", "ngspice/sndfile.dll", "ngspice/samplerate.dll", "ngspice/libomp140.x86_64.dll",
    "ngspice/spinit", "ngspice/COPYING", "LICENSE", "README.txt", "THIRD-PARTY-NOTICES.txt",
    "licenses/LGPL-3.0-only.txt", "licenses/GPL-3.0-only.txt", "licenses/nlohmann-json-MIT.txt",
    "library/passive/resistor.json", "examples/01_LED_and_push_button.snx", "QUICK-START.md",
    "vcruntime140.dll", "msvcp140.dll",
]


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("+", " ".join(command))
    return subprocess.run(command, check=True, **kwargs)


def version(build_dir: Path) -> str:
    header = build_dir / "apps/simnodus/generated/simnodus/version.hpp"
    match = re.search(r'SIMNODUS_VERSION_FULL "([^"]+)"', header.read_text(encoding="utf-8"))
    if not match:
        raise SystemExit(f"Cannot read the version from {header}")
    return match.group(1)


def find_windeployqt(build_dir: Path, explicit: str | None) -> str:
    if explicit:
        return explicit
    cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^Qt6_DIR:PATH=(.+)$", cache, re.MULTILINE)
    if match:
        candidate = Path(match.group(1)).resolve().parents[2] / "bin" / "windeployqt.exe"
        if candidate.is_file():
            return str(candidate)
    found = shutil.which("windeployqt") or shutil.which("windeployqt6")
    if not found:
        raise SystemExit("windeployqt not found; pass --windeployqt")
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--windeployqt")
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--skip-checks", action="store_true", help="do not run the packaged executable")
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    full_version = version(build_dir)
    name = f"SimNodus-{full_version}"
    staging_root = build_dir / "package"
    staging = staging_root / name
    if staging_root.exists():
        shutil.rmtree(staging_root)

    run(["cmake", "--install", str(build_dir), "--config", args.config, "--prefix", str(staging)])
    run([find_windeployqt(build_dir, args.windeployqt), "--release", "--no-compiler-runtime",
         "--no-opengl-sw", "--no-system-d3d-compiler", "--no-quick-import", "--translations", "pt_BR",
         str(staging / "simnodus.exe")])
    missing = [item for item in REQUIRED if not (staging / item).exists()]
    if missing:
        raise SystemExit(f"Package is incomplete, missing: {', '.join(missing)}")

    if not args.skip_checks:
        # Run with a clean PATH so that only bundled DLLs can be found.
        env = {k: v for k, v in os.environ.items() if k.upper() not in {"PATH", "QT_PLUGIN_PATH"}}
        env["PATH"] = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
        exe = str(staging / "simnodus.exe")
        engine = run([exe, "--check-engine"], env=env, capture_output=True, text=True, timeout=120)
        print(engine.stdout.strip())
        with tempfile.TemporaryDirectory() as temp:
            # The package ships only the native platform plugin, so this opens real windows briefly.
            smoke = run([exe, "--smoke-test", "--settings", str(Path(temp) / "s.ini")],
                        env=env, capture_output=True, text=True, timeout=120)
            print(smoke.stdout.strip())

    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f"{name}-windows-x64.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in sorted(staging.rglob("*")):
            if path.is_file():
                output.write(path, path.relative_to(staging_root).as_posix())
    size = archive.stat().st_size / (1024 * 1024)
    print(f"Wrote {archive} ({size:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
