"""Cards in Portuguese (mods/cards-pt-br): writes the mod's text listing from its table.

mods/cards-pt-br/cards.json is the source. Each card has its Portuguese name,
Konami's official text when the card game has one ("official"), a text of our
own saying what the card does in this game ("text"), and which of the two the
game shows ("show"). A card whose effect the game comes to have as the card
game's switches "show" to "official"; nothing else changes. "strings" are the
other words of a card: its type, its guardian stars, and the card view's
GUARDIAN STAR headings.

usage: python3 tools/pc/cards_pt_br.py           # write text/cards.txt
       python3 tools/pc/cards_pt_br.py --check   # say whether it is up to date

Every text shown must fit the card's box: lines of 20 letters, broken at
spaces as the game's own texts are (notes/translation.md), and 8 lines. A
text that does not, a letter the game cannot draw, or a card missing is an
error: nothing is written.
"""
import argparse
import json
import sys
import textwrap
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MOD = ROOT / "mods" / "cards-pt-br"
TABLE = MOD / "cards.json"
LISTING = MOD / "text" / "cards.txt"
CARD_COUNT = 722
LINE_LETTERS = 20
TEXT_LINES = 8
# The game's font has these besides letters and digits, Greek ones among them
# (notes/translation.md, "The listing"); the port draws accented letters, º ª °.
PUNCTUATION = set(" !\"#$%&'()*+,-./:;<=>?«»ºª°·αβγ")
SHOW = ("official", "text")
BANKS = ("names", "descriptions", "dialog")

HEADER = """\
# Cards in Portuguese (mods/cards-pt-br): written by tools/pc/cards_pt_br.py
# from cards.json. Do not edit; edit the table and run it again.
"""


def drawable(letter):
    """Whether the game draws `letter`: its font's, or one the port composes."""
    if letter.isascii():
        return letter.isalnum() or letter in PUNCTUATION
    if letter in PUNCTUATION:
        return True
    base = unicodedata.normalize("NFD", letter)[0]
    return base.isascii() and base.isalpha()


def wrap(text):
    """The text in the box's lines, as the game breaks them."""
    lines = []
    for paragraph in text.split("\n"):
        lines += textwrap.wrap(paragraph, LINE_LETTERS, break_long_words=False) or [""]
    return lines


def problems_of(card, errors):
    where = f"card {card.get('id')}"
    name = card.get("name")
    if not isinstance(name, str) or not name.strip():
        errors.append(f"{where}: no name")
        name = ""
    show = card.get("show")
    if show not in SHOW:
        errors.append(f"{where}: \"show\" is {show!r}, not one of {SHOW}")
        return
    shown = card.get(show)
    if not isinstance(shown, str) or not shown.strip():
        errors.append(f"{where}: shows its {show} text, and has none")
        return
    for key, value in (("name", name), (show, shown)):
        bad = sorted({letter for letter in value if not drawable(letter)})
        if bad:
            errors.append(f"{where}: the {key} has letters the game cannot draw: {''.join(bad)}")
        if value != unicodedata.normalize("NFC", value):
            errors.append(f"{where}: the {key} is not composed (NFC)")
    lines = wrap(shown)
    if len(lines) > TEXT_LINES or any(len(line) > LINE_LETTERS for line in lines):
        errors.append(f"{where}: the {show} text takes {len(lines)} lines of up to {LINE_LETTERS} letters "
                      f"(the box has {TEXT_LINES}): {shown}")


def listing(table):
    """The text listing, or a list of errors."""
    errors = []
    cards = table.get("cards", [])
    ids = [card.get("id") for card in cards]
    missing = sorted(set(range(1, CARD_COUNT + 1)) - set(ids))
    if missing:
        errors.append(f"cards missing: {missing[:20]}{'...' if len(missing) > 20 else ''}")
    if len(ids) != len(set(ids)):
        errors.append("a card is listed twice")
    for card in cards:
        problems_of(card, errors)
    for string in table.get("strings", []):
        if string.get("bank") not in BANKS or not string.get("ids") or not isinstance(string.get("text"), str):
            errors.append(f"string {string.get('ids')}: needs a bank of {BANKS}, ids and a text")
    if errors:
        return None, errors
    out = [HEADER]
    by_bank = {bank: [s for s in table.get("strings", []) if s["bank"] == bank] for bank in BANKS}
    out.append("@bank names\n")
    for card in sorted(cards, key=lambda c: c["id"]):
        out.append(f"\n[{0x8000 + card['id']:04X}]  # {card['id']}\n{card['name']}{{end}}\n")
    for string in by_bank["names"]:
        out.append(f"\n[{string['ids']}]\n{string['text']}{{end}}\n")
    out.append("\n@bank descriptions\n")
    for card in sorted(cards, key=lambda c: c["id"]):
        text = "\n".join(wrap(card[card["show"]]))
        out.append(f"\n[{0xD100 + card['id']:04X}]  # {card['name']}\n{text}{{end}}\n")
    for string in by_bank["descriptions"]:
        out.append(f"\n[{string['ids']}]\n{string['text']}{{end}}\n")
    if by_bank["dialog"]:
        out.append("\n@bank dialog\n")
        for string in by_bank["dialog"]:
            out.append(f"\n[{string['ids']}]\n{string['text']}\n")
    return "".join(out), []


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="only say whether text/cards.txt is up to date")
    args = parser.parse_args(argv)
    table = json.loads(TABLE.read_text(encoding="utf-8"))
    text, errors = listing(table)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        print(f"{len(errors)} error(s); nothing written", file=sys.stderr)
        return 1
    shown = sum(1 for card in table["cards"] if card["show"] == "official")
    if args.check:
        current = LISTING.read_text(encoding="utf-8") if LISTING.exists() else ""
        if current != text:
            print(f"{LISTING.relative_to(ROOT)} is not up to date: run tools/pc/cards_pt_br.py", file=sys.stderr)
            return 1
        print(f"{LISTING.relative_to(ROOT)} is up to date")
        return 0
    LISTING.parent.mkdir(parents=True, exist_ok=True)
    LISTING.write_text(text, encoding="utf-8", newline="\n")
    print(f"{len(table['cards'])} cards ({shown} with Konami's text) -> {LISTING.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
