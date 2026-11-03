"""The card fusions for the site (gasparjs.com.br, /lab/forbidden-memories/fusoes): every pair of cards of
the user's disc that fuses, and what it makes, as Duel_CheckFusion reads the fusion table, the "glitch"
pairs too (they fuse in the game), marked apart. Nothing of the disc but those facts is written.

usage: python3 tools/pc/site/export_card_fusions.py --disc game/rpg-yfm.bin \\
           --out <site>/src/content/lab/fm-fusions.json
"""
import argparse, json, sys
from pathlib import Path


def fusions(pairs, glitch):
    """Each pair as [smaller id, larger id, result], by the pair, and the glitch pairs as [a, b]."""
    return {"fusions": [[a, b, r] for (a, b), r in sorted(pairs.items())],
            "glitch": [[a, b] for a, b in sorted(glitch)]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--disc", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from fm_editor import disc, gamedata
    data = gamedata.load_game(disc.load(args.disc))
    result = fusions(data.fusions, data.glitch_fusions)
    Path(args.out).write_text(json.dumps(result, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"{len(result['fusions'])} fusions ({len(result['glitch'])} glitch) -> {args.out}")


if __name__ == "__main__":
    main()
