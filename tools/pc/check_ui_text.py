#!/usr/bin/env python3
"""Check the port's own words' translations (languages/ui/*.json,
src/pc/platform/ui_text.h).

Each catalog maps an English string of the port to its translation. The
English is the key UI() looks up, so a key the C source (src/pc) no longer
has is a translation nobody sees: an error. A translation must keep its
English's printf conversions (%d, %s, %.48s...) in the same order, or the
format would read its arguments wrongly: an error too.

--missing lists the strings passed to UI() that a catalog has no
translation for (strings in tables, translated where they are drawn, are not
seen by that list).
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CATALOGS = ROOT / "languages/ui"
SOURCES = ROOT / "src/pc"

LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
CHARACTER = re.compile(r"'(?:[^'\\\n]|\\.)+'")
CONVERSION = re.compile(r"%[-+ #0']*(?:\*|\d+)?(?:\.(?:\*|\d+))?(?:hh|h|ll|l|z|j|t|L)?[diouxXeEfgGcspn%]")
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\", "'": "'", "?": "?", "0": "\0"}


def unescape(text):
    """A C literal's bytes, as UTF-8 text."""
    out, i = bytearray(), 0
    while i < len(text):
        if text[i] == "\\" and i + 1 < len(text):
            nxt = text[i + 1]
            if nxt == "x":
                digits = re.match(r"[0-9a-fA-F]+", text[i + 2:]).group(0)
                out.append(int(digits, 16) & 0xFF)
                i += 2 + len(digits)
                continue
            if nxt in "01234567" and re.match(r"[0-7]{1,3}", text[i + 1:]).group(0) != "0":
                digits = re.match(r"[0-7]{1,3}", text[i + 1:]).group(0)
                out.append(int(digits, 8) & 0xFF)
                i += 1 + len(digits)
                continue
            out.extend(ESCAPES.get(nxt, nxt).encode())
            i += 2
            continue
        out.extend(text[i].encode())
        i += 1
    return out.decode("utf-8", "replace")


def strip(code):
    """The code without its comments or character constants, keeping the
    string literals (a comment's quote is no literal)."""
    pieces, at = [], 0
    for match in re.finditer(r'"(?:[^"\\\n]|\\.)*"|/\*.*?\*/|//[^\n]*|' + CHARACTER.pattern, code, re.S):
        pieces.append(code[at:match.start()])
        token = match.group(0)
        pieces.append(token if token.startswith('"') else " ")
        at = match.end()
    pieces.append(code[at:])
    return "".join(pieces)


def literals(code):
    """Every string a run of adjacent literals makes, with where each run
    starts."""
    runs, at = [], 0
    for match in re.finditer(r'(?:"(?:[^"\\\n]|\\.)*"\s*)+', code):
        parts = LITERAL.findall(match.group(0))
        runs.append((match.start(), unescape("".join(parts))))
    return runs


def source_strings():
    found, translated = set(), {}
    for path in sorted(SOURCES.rglob("*.c")) + sorted(SOURCES.rglob("*.h")):
        code = strip(path.read_text(encoding="utf-8", errors="replace"))
        for start, text in literals(code):
            found.add(text)
            if re.search(r"\bUI\(\s*$", code[max(0, start - 40):start]):
                translated.setdefault(text, f"{path.relative_to(ROOT)}:{code.count(chr(10), 0, start) + 1}")
    return found, translated


def conversions(text):
    return [c for c in CONVERSION.findall(text) if c != "%%"]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--missing", action="store_true", help="list UI() strings a catalog does not translate")
    options = parser.parse_args()
    found, translated = source_strings()
    errors = 0
    catalogs = sorted(CATALOGS.glob("*.json"))
    if not catalogs:
        print(f"check_ui_text: no catalog in {CATALOGS.relative_to(ROOT)}")
        return 1
    for catalog in catalogs:
        name = catalog.relative_to(ROOT)
        try:
            entries = json.loads(catalog.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            print(f"{name}: {error}")
            errors += 1
            continue
        if not isinstance(entries, dict):
            print(f"{name}: not an object of English strings")
            errors += 1
            continue
        for english, words in entries.items():
            if not isinstance(words, str) or not words:
                print(f"{name}: {english!r}: not a translation (a non-empty string)")
                errors += 1
            elif english not in found:
                print(f"{name}: {english!r}: no longer in the source (src/pc)")
                errors += 1
            elif conversions(english) != conversions(words):
                print(f"{name}: {english!r}: {conversions(words)} where the English has {conversions(english)}")
                errors += 1
        if options.missing:
            absent = sorted((where, english) for english, where in translated.items()
                            if english not in entries and re.search(r"[A-Za-z]{2}", english))
            for where, english in absent:
                print(f"{name}: no translation: {english!r} ({where})")
        print(f"check_ui_text: {name}: {len(entries)} strings")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
