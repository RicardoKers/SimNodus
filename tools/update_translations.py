"""Refreshes the translation sources in resources/translations (UI-16).

1. lupdate collects the texts of the application's code (tr(), translate()).
2. The texts of the component library (names, categories, descriptions,
   parameter labels and descriptions, choice words) are added under the
   context "ComponentLibrary": they are data, so lupdate cannot see them.

Existing translations are kept; texts that no longer exist are dropped; new
ones are marked unfinished for Qt Linguist. The build turns the .ts files
into .qm files when Qt's LinguistTools are installed.

Usage: python tools/update_translations.py [--lupdate PATH]
"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDER = os.path.join(ROOT, "resources", "translations")
LANGUAGES = ["pt_BR"]
CONTEXT = "ComponentLibrary"


def library_texts():
    texts = []
    seen = set()

    def add(text):
        if text and text not in seen:
            seen.add(text)
            texts.append(text)

    for path in sorted(glob.glob(os.path.join(ROOT, "resources", "library", "**", "*.json"), recursive=True)):
        with open(path, encoding="utf-8") as fh:
            d = json.load(fh)
        for key in ("name", "category", "description"):
            add(d.get(key))
        for p in d.get("parameters", []):
            add(p.get("label"))
            add(p.get("description"))
            for choice in p.get("choices", []):
                add(choice)
    # Definitions made in code: vendor subcircuit parts (src/library/blocks.cpp, SN-214).
    for text in ("Subcircuits", "Subcircuit", "Vendor subcircuit from the model folders",
                 "The vendor subcircuit this part runs; Pins... says which port is which pin"):
        add(text)
    return texts


def find_lupdate(given):
    if given:
        return given
    found = shutil.which("lupdate")
    if found:
        return found
    for candidate in glob.glob("C:/Qt/*/msvc*/bin/lupdate.exe"):
        return candidate
    sys.exit("lupdate not found; pass --lupdate")


def kept_library_translations(ts_path):
    """Translations of the library context before lupdate drops it."""
    kept = {}
    if not os.path.exists(ts_path):
        return kept
    for ctx in ET.parse(ts_path).getroot().findall("context"):
        if ctx.findtext("name") != CONTEXT:
            continue
        for message in ctx.findall("message"):
            translation = message.find("translation")
            if translation is not None and translation.text and translation.get("type") != "unfinished":
                kept[message.findtext("source")] = translation.text
    return kept


def add_library_context(ts_path, kept):
    tree = ET.parse(ts_path)
    root = tree.getroot()
    for ctx in root.findall("context"):
        if ctx.findtext("name") == CONTEXT:
            root.remove(ctx)
    ctx = ET.SubElement(root, "context")
    ET.SubElement(ctx, "name").text = CONTEXT
    for text in library_texts():
        message = ET.SubElement(ctx, "message")
        ET.SubElement(message, "source").text = text
        translation = ET.SubElement(message, "translation")
        if text in kept:
            translation.text = kept[text]
        else:
            translation.set("type", "unfinished")
    ET.indent(tree, space="    ")
    with open(ts_path, "wb") as fh:
        fh.write(b'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n')
        tree.write(fh, encoding="utf-8", xml_declaration=False)
        fh.write(b"\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--lupdate")
    args = parser.parse_args()
    lupdate = find_lupdate(args.lupdate)
    os.makedirs(FOLDER, exist_ok=True)
    for language in LANGUAGES:
        ts_path = os.path.join(FOLDER, f"simnodus_{language}.ts")
        kept = kept_library_translations(ts_path)
        subprocess.run([lupdate, os.path.join(ROOT, "apps", "simnodus"), "-ts", ts_path, "-no-obsolete",
                        "-locations", "none", "-target-language", language], check=True)
        add_library_context(ts_path, kept)
        print(f"{ts_path}: {len(kept)} library translations kept")


if __name__ == "__main__":
    main()
