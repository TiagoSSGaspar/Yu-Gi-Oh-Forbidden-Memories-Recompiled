"""The card passwords for the site (gasparjs.com.br, /lab/forbidden-memories/passwords): every card of the
user's disc and the cards the Duelist Kingdom rules add, as JSON facts (number, name, kind, ATK/DEF,
password, StarChip price, and the type number the game gives it: 0-19 the monster types, 20-23
magic, trap, ritual, equip). Nothing of the disc but those facts is written.

usage: python3 tools/pc/site/export_card_passwords.py --disc game/rpg-yfm.bin \\
           --mod mods/modern-rules/mod.json --out <site>/src/content/lab/fm-cards.json
"""
import argparse, json, sys
from pathlib import Path

KINDS = {20: "magic", 21: "trap", 22: "ritual", 23: "equip"}


def _kind(type_):
    return KINDS.get(type_, "monster")


def _entry(id_, name, base, password, starchips, mod):
    kind = _kind(base.type)
    monster = kind == "monster"
    return {"id": id_, "name": name, "kind": kind, "type": base.type, "attack": base.attack if monster else None,
            "defense": base.defense if monster else None, "password": password or "", "starchips": starchips,
            "mod": mod}


def cards(data, mod, first_added=723):
    """The disc's cards by number, then the mod's added cards (those with an "id"), numbered from
    first_added in the order the mod lists them, as the game numbers them. An added card without a
    password (the decoy hat) is left out."""
    out = [_entry(i, c.name, c, data.passwords.get(i, ""), data.starchips.get(i, 999999), False)
           for i, c in sorted(data.cards.items())]
    by_name = {c.name: c for c in data.cards.values()}
    prices = mod.get("passwords", {})
    number = first_added
    for card in mod.get("cards", []):
        if "id" not in card:
            continue
        id_, number = number, number + 1
        if not card.get("password"):
            continue
        price = prices.get(f"modern-rules:{card['id']}:1", {}).get("starchips", 999999)
        out.append(_entry(id_, card["name"], by_name[card["copy"]], card["password"], price, True))
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--disc", required=True)
    parser.add_argument("--mod", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from fm_editor import disc, gamedata
    data = gamedata.load_game(disc.load(args.disc))
    mod = json.loads(Path(args.mod).read_text(encoding="utf-8"))
    result = cards(data, mod)
    Path(args.out).write_text(json.dumps(result, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{len(result)} cards -> {args.out}")


if __name__ == "__main__":
    main()
