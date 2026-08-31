"""Check local documentation links, JSON syntax, and repository text hygiene.

This standard-library tool does not test simulator behavior, external URLs,
Markdown anchors, YAML semantics, English grammar, or security properties.
"""

from __future__ import annotations

import json
import os
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = {
    ".git", ".codex", ".agents", ".private", ".vs", ".vscode",
    "build", "out", "dist", ".venv", "venv", "__pycache__",
    ".cache", ".pytest_cache", "node_modules",
}
TEXT_SUFFIXES = {".md", ".json", ".yml", ".yaml", ".py", ".cmake", ".txt", ".cpp", ".h"}
TEXT_NAMES = {"LICENSE", ".gitignore", ".gitattributes", ".editorconfig", "spinit"}
REQUIRED = {
    "README.md", "AGENTS.md", "LICENSE", "CONTRIBUTING.md", "SECURITY.md",
    "CHANGELOG.md", "CMakeLists.txt", "CMakePresets.json", "docs/README.md",
    "docs/planning/CURRENT.md", "docs/planning/BACKLOG.md",
    "docs/planning/ROADMAP.md", "docs/architecture/README.md",
    "docs/decisions/README.md", ".github/workflows/ci.yml",
}
LINK = re.compile(r"!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\)")
FENCE = re.compile(r"^\s*(`{3,}|~{3,})")


def source_files() -> list[Path]:
    files = []
    for directory, children, names in os.walk(ROOT, followlinks=False):
        children[:] = sorted(
            name for name in children
            if name not in EXCLUDED and not (Path(directory) / name).is_symlink()
        )
        for name in sorted(names):
            path = Path(directory) / name
            if path.is_symlink():
                continue
            if path.suffix in TEXT_SUFFIXES or name in TEXT_NAMES:
                files.append(path)
    return files


def check_markdown(path: Path, content: str, errors: list[str]) -> None:
    relative = path.relative_to(ROOT).as_posix()
    fence = None
    for number, line in enumerate(content.splitlines(), 1):
        marker = FENCE.match(line)
        if marker:
            token = marker.group(1)
            if fence is None:
                fence = token
            elif token[0] == fence[0] and len(token) >= len(fence):
                fence = None
            continue
        if fence is not None:
            continue
        # Inline code may document syntax that is not an actual Markdown link.
        visible = re.sub(r"`+[^`]*`+", "", line)
        for match in LINK.finditer(visible):
            target = match.group(1).strip("<>")
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            local_path = unquote(parsed.path)
            if local_path.startswith("/"):
                errors.append(f"{relative}:{number}: use a relative repository link")
                continue
            destination = (path.parent / local_path).resolve()
            if not destination.is_relative_to(ROOT):
                errors.append(f"{relative}:{number}: link escapes repository: {target}")
            elif not destination.exists():
                errors.append(f"{relative}:{number}: missing link target: {target}")
    if fence is not None:
        errors.append(f"{relative}: unclosed Markdown fence")


def main() -> int:
    errors: list[str] = []
    for required in sorted(REQUIRED):
        if not (ROOT / required).is_file():
            errors.append(f"Missing required file: {required}")

    files = source_files()
    for path in files:
        relative = path.relative_to(ROOT).as_posix()
        try:
            raw = path.read_bytes()
            content = raw.decode("utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"{relative}: cannot read UTF-8 text: {error}")
            continue
        if raw.startswith(b"\xef\xbb\xbf"):
            errors.append(f"{relative}: remove UTF-8 BOM")
        if "\x00" in content:
            errors.append(f"{relative}: unexpected NUL byte")
        if content and not content.endswith("\n"):
            errors.append(f"{relative}: missing final newline")
        for number, line in enumerate(content.splitlines(), 1):
            if line != line.rstrip() and not (
                path.suffix == ".md" and line.endswith("  ") and not line.endswith("   ")
            ):
                errors.append(f"{relative}:{number}: trailing whitespace")
        if path.suffix == ".md":
            check_markdown(path, content, errors)
        if path.suffix == ".json":
            try:
                json.loads(content)
            except (ValueError, RecursionError) as error:
                errors.append(f"{relative}: invalid JSON: {error}")

    if errors:
        print("Repository check failed:")
        for error in errors:
            print(f"- {error}")
        return 1
    print(f"Repository check passed ({len(files)} text files). No simulator tests were run.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
