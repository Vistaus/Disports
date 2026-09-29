#!/usr/bin/env python3
"""Generates data/emoji.json, the Unicode emoji list of the emoji picker.

Emoji are in Unicode's keyboard (CLDR) order, grouped into the picker's
categories. Skin tone variants are left out; the picker shows the default
yellow ones. Emoji newer than MAX_EMOJI_VERSION are left out too, because the
colour emoji font of Ubuntu Touch 24.04 (Noto 2.047) cannot draw them yet.

Usage: tools/generate-emoji-data.py [path/to/emoji-test.txt]
Without a path, the latest emoji-test.txt is downloaded from unicode.org.
"""
import json
import sys
from pathlib import Path
from urllib.request import urlopen

SOURCE_URL = "https://www.unicode.org/Public/emoji/latest/emoji-test.txt"
OUTPUT_PATH = Path(__file__).resolve().parents[1] / "data/emoji.json"

CATEGORIES = {
    "Smileys & Emotion": "faces",
    "People & Body": "people",
    "Animals & Nature": "nature",
    "Food & Drink": "food",
    "Travel & Places": "travel",
    "Activities": "activities",
    "Objects": "objects",
    "Symbols": "symbols",
    "Flags": "flags",
}
MAX_EMOJI_VERSION = 16.0
SKIN_TONES = {0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF}


def parse(text):
    version = ""
    category = "symbols"
    rows = []
    for line in text.splitlines():
        if line.startswith("# Version:"):
            version = line.split(":", 1)[1].strip()
        elif line.startswith("# group:"):
            category = CATEGORIES.get(line.split(":", 1)[1].strip(), "symbols")
        elif "; fully-qualified" in line:
            before_comment, comment = line.split("#", 1)
            codepoints = [int(cp, 16) for cp in before_comment.split(";", 1)[0].split()]
            if SKIN_TONES.intersection(codepoints):
                continue
            # Comment format: "<emoji> E<version> <CLDR short name>"
            parts = comment.strip().split(" ", 2)
            if len(parts) >= 2 and parts[1].startswith("E") and float(parts[1][1:]) > MAX_EMOJI_VERSION:
                continue
            label = parts[2].strip() if len(parts) >= 3 else comment.strip()
            rows.append(["".join(map(chr, codepoints)), category, label])
    return version, rows


def main():
    if len(sys.argv) > 1:
        text = Path(sys.argv[1]).read_text(encoding="utf-8")
    else:
        text = urlopen(SOURCE_URL, timeout=30).read().decode("utf-8")
    version, rows = parse(text)
    data = {"source": SOURCE_URL, "version": version, "emoji": rows}
    OUTPUT_PATH.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"Wrote {len(rows)} emoji from Unicode Emoji {version} to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
