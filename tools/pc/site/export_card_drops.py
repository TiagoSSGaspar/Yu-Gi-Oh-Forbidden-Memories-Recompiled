"""The card drops for the site (gasparjs.com.br, /lab/forbidden-memories/drops): which duelist gives each
card of the user's disc when beaten, and its weight out of 2048 in each of the three prize pools
(S/A-POW, S/A-TEC, B/C/D), as JSON facts. The deck pools, the unused duelist 0 and everything else of
the disc are left out. With --mod, a drops mod's own weights too (Drop missing cards gives the cards no
duelist drops a duelist), apart, as the weights it lists: a listed card gets exactly its weight.

usage: python3 tools/pc/site/export_card_drops.py --disc game/rpg-yfm.bin \\
           --mod mods/drop-missing-cards/mod.json --out <site>/src/content/lab/fm-drops.json
"""
import argparse, json, sys
from pathlib import Path

PRIZES = ("pow", "tec", "bcd")


def _by_card(weights):
    """(duelist, prize, card, weight) facts as each card by number with the duelists that drop it:
    their weight in each prize pool, the likeliest duelist first (then by number)."""
    by_card = {}
    for duelist, prize, card, weight in weights:
        row = by_card.setdefault(card, {}).setdefault(duelist, dict.fromkeys(PRIZES, 0))
        row[prize] = weight
    out = []
    for card in sorted(by_card):
        rows = [{"duelist": d, **w} for d, w in by_card[card].items()]
        rows.sort(key=lambda r: (-max(r[p] for p in PRIZES), r["duelist"]))
        out.append({"id": card, "drops": rows})
    return out


def drops(pools, names, cards):
    """The duelists (1 up) by number and name, and each card that drops anywhere."""
    weights = [(duelist, prize, card, weight) for duelist in range(1, len(pools)) for prize in PRIZES
               for card, weight in pools[duelist][prize].items() if weight > 0 and card in cards]
    duelists = [{"id": d, "name": names[d]} for d in range(1, len(pools))]
    return {"duelists": duelists, "cards": _by_card(weights)}


def mod_drops(mod, names, cards):
    """A drops mod's weights, by card, named as the mod names them (duelist and card names, the
    prize keys pow/tec/bcd). Only a mod that lists cards' weights, short of the whole pool, is read:
    a "replace" or a pool listed up to 2048 changes every card's chance, not just the listed ones."""
    duelist_ids = {name: i for i, name in enumerate(names) if i > 0}
    card_ids = {card.name: i for i, card in cards.items()}
    weights = []
    for duelist, pools in mod["drops"].items():
        for prize, listed in pools.items():
            if "replace" in listed or sum(listed.values()) >= 2048:
                raise ValueError(f"{duelist} {prize}: not a list of weights short of the pool")
            for card, weight in listed.items():
                if weight > 0:
                    weights.append((duelist_ids[duelist], prize, card_ids[card], weight))
    return {"id": mod["id"], "cards": _by_card(weights)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--disc", required=True)
    parser.add_argument("--mod")
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from fm_editor import disc, gamedata
    data = gamedata.load_game(disc.load(args.disc))
    result = drops(data.pools, gamedata.DUELIST_NAMES, data.cards)
    if args.mod:
        mod = json.loads(Path(args.mod).read_text(encoding="utf-8"))
        result["mod"] = mod_drops(mod, gamedata.DUELIST_NAMES, data.cards)
    Path(args.out).write_text(json.dumps(result, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{len(result['cards'])} cards, {len(result['duelists'])} duelists -> {args.out}")


if __name__ == "__main__":
    main()
