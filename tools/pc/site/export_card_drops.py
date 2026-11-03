"""The card drops for the site (gasparjs.com.br, /lab/forbidden-memories/drops): which duelist gives each
card of the user's disc when beaten, and its weight out of 2048 in each of the three prize pools
(S/A-POW, S/A-TEC, B/C/D), as JSON facts. The deck pools, the unused duelist 0 and everything else of
the disc are left out.

usage: python3 tools/pc/site/export_card_drops.py --disc game/rpg-yfm.bin \\
           --out <site>/src/content/lab/fm-drops.json
"""
import argparse, json, sys
from pathlib import Path

PRIZES = ("pow", "tec", "bcd")


def drops(pools, names, cards):
    """The duelists (1 up) by number and name, and each card that drops anywhere, by number, with the
    duelists that drop it: their weight in each prize pool, the likeliest duelist first (then by
    number)."""
    by_card = {}
    for duelist in range(1, len(pools)):
        for prize in PRIZES:
            for card, weight in pools[duelist][prize].items():
                if weight <= 0 or card not in cards:
                    continue
                row = by_card.setdefault(card, {}).setdefault(duelist, dict.fromkeys(PRIZES, 0))
                row[prize] = weight
    out_cards = []
    for card in sorted(by_card):
        rows = [{"duelist": d, **w} for d, w in by_card[card].items()]
        rows.sort(key=lambda r: (-max(r[p] for p in PRIZES), r["duelist"]))
        out_cards.append({"id": card, "drops": rows})
    duelists = [{"id": d, "name": names[d]} for d in range(1, len(pools))]
    return {"duelists": duelists, "cards": out_cards}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--disc", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from fm_editor import disc, gamedata
    data = gamedata.load_game(disc.load(args.disc))
    result = drops(data.pools, gamedata.DUELIST_NAMES, data.cards)
    Path(args.out).write_text(json.dumps(result, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{len(result['cards'])} cards, {len(result['duelists'])} duelists -> {args.out}")


if __name__ == "__main__":
    main()
