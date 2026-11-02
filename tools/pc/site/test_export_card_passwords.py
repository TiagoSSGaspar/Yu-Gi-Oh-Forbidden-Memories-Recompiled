import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import export_card_passwords as ex


class Card:
    def __init__(self, name, type_, attack, defense):
        self.name, self.type, self.attack, self.defense = name, type_, attack, defense


class Data:
    cards = {1: Card("Blue-eyes White Dragon", 0, 3000, 2500), 344: Card("Hinotama", 20, 0, 0),
             301: Card("Legendary Sword", 23, 0, 0), 686: Card("Widespread Ruin", 21, 0, 0)}
    passwords = {1: "89631139", 344: "46130346", 301: "61854111", 686: "77754944"}
    starchips = {1: 999999, 344: 55, 301: 800, 686: 999999}


MOD = {
    "cards": [
        {"replace": "Great Moth"},
        {"id": "multiply", "copy": "Legendary Sword", "name": "Multiply", "password": "40703222"},
        {"id": "mirror_force", "copy": "Widespread Ruin", "name": "Mirror Force", "password": "44095762"},
        {"id": "magical_hat", "copy": "Hinotama", "name": "Magical Hat"},
    ],
    "passwords": {"modern-rules:multiply:1": {"password": "40703222", "starchips": 50},
                  "modern-rules:mirror_force:1": {"password": "44095762", "starchips": 500}},
}


class ExportTest(unittest.TestCase):
    def test_disc_cards(self):
        out = ex.cards(Data(), {"cards": []}, first_added=723)
        self.assertEqual([c["id"] for c in out], [1, 301, 344, 686])
        self.assertEqual(out[0], {"id": 1, "name": "Blue-eyes White Dragon", "kind": "monster", "attack": 3000,
                                  "defense": 2500, "password": "89631139", "starchips": 999999, "mod": False})
        self.assertEqual(out[2]["kind"], "magic")
        self.assertIsNone(out[2]["attack"])
        self.assertEqual(out[1]["kind"], "equip")
        self.assertEqual(out[3]["kind"], "trap")

    def test_mod_cards_numbered_after_the_disc_and_priced(self):
        out = ex.cards(Data(), MOD, first_added=723)
        added = [c for c in out if c["mod"]]
        self.assertEqual([(c["id"], c["name"], c["kind"], c["password"], c["starchips"]) for c in added],
                         [(723, "Multiply", "equip", "40703222", 50), (724, "Mirror Force", "trap", "44095762", 500)])


if __name__ == "__main__":
    unittest.main()
