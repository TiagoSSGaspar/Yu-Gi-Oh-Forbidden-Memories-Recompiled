import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import export_card_drops as ex

NAMES = ["Unused", "Simon Muran", "Teana", "Jono"]
CARDS = {1: "Blue-eyes White Dragon", 2: "Mystical Elf", 3: "Hitotsu-me Giant", 4: "Kuriboh"}


def pool(deck=None, pow=None, bcd=None, tec=None):
    return {"deck": deck or {}, "pow": pow or {}, "bcd": bcd or {}, "tec": tec or {}}


POOLS = [
    pool(pow={4: 2048}),                                    # the unused duelist 0: left out
    pool(deck={3: 2048}, pow={2: 1000, 1: 1048}, tec={2: 2048}, bcd={2: 2048}),
    pool(pow={2: 2048}, bcd={1: 0, 2: 2048}, tec={2: 2048}),
    pool(pow={1: 2000, 99: 48}, bcd={2: 2048}, tec={2: 2048}),
]


class Drops(unittest.TestCase):
    def setUp(self):
        self.out = ex.drops(POOLS, NAMES, CARDS)
        self.cards = {c["id"]: c["drops"] for c in self.out["cards"]}

    def test_duelists_from_one(self):
        self.assertEqual(self.out["duelists"], [{"id": 1, "name": "Simon Muran"}, {"id": 2, "name": "Teana"},
                                                {"id": 3, "name": "Jono"}])

    def test_only_cards_that_drop_from_a_prize_pool(self):
        # 3 is only in a deck, 4 only drops from the unused duelist, 99 is not a card.
        self.assertEqual(sorted(self.cards), [1, 2])

    def test_weights_by_prize_and_zero_where_absent(self):
        self.assertEqual(self.cards[1], [{"duelist": 3, "pow": 2000, "tec": 0, "bcd": 0},
                                         {"duelist": 1, "pow": 1048, "tec": 0, "bcd": 0}])

    def test_likeliest_first_then_by_number(self):
        self.assertEqual([r["duelist"] for r in self.cards[2]], [1, 2, 3])
        self.assertEqual(self.cards[2][0], {"duelist": 1, "pow": 1000, "tec": 2048, "bcd": 2048})


if __name__ == "__main__":
    unittest.main()
