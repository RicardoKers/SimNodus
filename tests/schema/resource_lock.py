"""Experimental locked inventory validation; no resource access or acquisition."""
import argparse
import hashlib
import json
from pathlib import Path
import re

import topology as t

MAX_FILES = 256
MAX_FILE_BYTES = 16 * 1024 * 1024
MAX_TOTAL_BYTES = 64 * 1024 * 1024
SEGMENT = re.compile(r"[A-Za-z0-9_-][A-Za-z0-9_.-]{0,79}\Z")
HASH = re.compile(r"[0-9a-f]{64}\Z")
VERSION = re.compile(r"[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}\Z")
RESERVED = {"con", "prn", "aux", "nul", "conin$", "conout$"} | {
    f"{prefix}{number}" for prefix in ("com", "lpt") for number in range(1, 10)
}


def portable_path(value, loc):
    t.require(type(value) is str and 0 < len(value) <= 240, "path", loc, "Invalid relative resource path")
    parts = value.split("/")
    t.require(len(parts) <= 16 and all(
        SEGMENT.fullmatch(part) and not part.endswith(".")
        and part.split(".")[0].lower() not in RESERVED for part in parts
    ), "path", loc, "Path must use unambiguous portable segments")
    return value.lower()


def content_hash(files):
    """Digest declared file inventory, not the bytes of files on disk."""
    rows = [{key: file[key] for key in ("path", "bytes", "sha256")}
            for file in sorted(files, key=lambda item: item["path"])]
    raw = json.dumps(rows, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")
    return hashlib.sha256(raw).hexdigest()


def validate(document):
    t.nesting(document)
    t.fields(document, "format version dependencies", "$")
    t.require(document["format"] == "simnodus-resource-lock" and document["version"] == "0.1",
              "version", "$", "Expected experimental resource lock 0.1")
    dependencies = t.array(document["dependencies"], "$/dependencies", 1)
    t.require(len(dependencies) <= 32, "budget", "$", "Dependency count exceeded")
    ids, paths = set(), set()
    count, total = 0, 0

    def text(value, loc):
        t.require(type(value) is str and 0 < len(value) <= 512 and value == value.strip()
                  and all(32 <= ord(c) <= 126 for c in value), "value", loc, "Expected bounded printable metadata")

    def digest(value, loc):
        t.require(type(value) is str and HASH.fullmatch(value) is not None,
                  "hash", loc, "Expected lowercase SHA-256")

    for index, dependency in enumerate(dependencies):
        loc = f"$/dependencies/{index}"
        t.fields(dependency, "id version origin license files content_sha256", loc)
        t.identifier(dependency["id"], loc)
        t.require(dependency["id"] not in ids, "id", loc, "Duplicate dependency ID")
        ids.add(dependency["id"])
        version = dependency["version"]
        t.require(type(version) is str and VERSION.fullmatch(version) is not None,
                  "version", loc, "Expected exact three-part numeric version")
        origin = dependency["origin"]
        t.fields(origin, "kind author reference revision", loc + "/origin")
        t.require(origin["kind"] in ("owned", "external"), "value", loc, "Unknown origin kind")
        for key in ("author", "reference", "revision"):
            text(origin[key], loc + "/origin/" + key)
        license_record = dependency["license"]
        t.fields(license_record, "identifier notice", loc + "/license")
        text(license_record["identifier"], loc + "/license/identifier")
        t.identifier(license_record["notice"], loc + "/license/notice")
        files = t.array(dependency["files"], loc + "/files", 1)
        count += len(files)
        t.require(count <= MAX_FILES, "budget", loc, "Resource count exceeded")
        file_ids = set()
        for number, file in enumerate(files):
            where = f"{loc}/files/{number}"
            t.fields(file, "id path bytes sha256", where)
            t.identifier(file["id"], where)
            t.require(file["id"] not in file_ids, "id", where, "Duplicate local resource ID")
            file_ids.add(file["id"])
            path = portable_path(file["path"], where)
            t.require(path not in paths, "path", where, "Case-insensitive resource path collision")
            paths.add(path)
            size = file["bytes"]
            t.require(type(size) is int and 0 <= size <= MAX_FILE_BYTES,
                      "budget", where, "Invalid declared resource size")
            total += size
            t.require(total <= MAX_TOTAL_BYTES, "budget", where, "Total declared resource size exceeded")
            digest(file["sha256"], where)
        t.require(license_record["notice"] in file_ids, "reference", loc, "License notice must reference a local resource")
        digest(dependency["content_sha256"], loc)
        t.require(dependency["content_sha256"] == content_hash(files), "hash", loc, "Declared inventory digest mismatch")
    for path in paths:
        parts = path.split("/")
        t.require(all("/".join(parts[:n]) not in paths for n in range(1, len(parts))),
                  "path", "$", "A resource cannot also be a parent directory")
    return dict(dependencies=len(ids), resources=count, declared_bytes=total,
                resources_verified=False, containment_verified=False,
                redistribution_verified=False, simulation_ready=False)


def parse(raw):
    t.require(len(raw) <= t.MAX_BYTES, "input", "$", "Input byte budget exceeded")

    def pairs(items):
        result = {}
        for key, value in items:
            t.require(key not in result, "input", "$", "Duplicate JSON key")
            result[key] = value
        return result

    def constant(_):
        raise t.Invalid("input", "$", "Non-finite JSON literal")

    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
        validate(document)
        return document
    except (UnicodeError, ValueError, RecursionError) as error:
        if isinstance(error, t.Invalid):
            raise
        raise t.Invalid("input", "$", "Invalid encoding, JSON or nesting") from error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("document", type=Path)
    args = parser.parse_args()
    try:
        with args.document.open("rb") as source:
            document = parse(source.read(t.MAX_BYTES + 1))
        print(json.dumps(dict(status="valid-lock-metadata-only", **validate(document))))
        return 0
    except (t.Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, t.Invalid) else dict(code="input", location="$", message="Cannot read input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
