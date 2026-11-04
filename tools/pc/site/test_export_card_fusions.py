import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import export_card_fusions as ex


class Fusions(unittest.TestCase):
    def test_pairs_sorted_with_their_result(self):
        out = ex.fusions({(2, 9): 470, (2, 8): 638, (1, 1): 5}, set())
        self.assertEqual(out["fusions"], [[1, 1, 5], [2, 8, 638], [2, 9, 470]])
        self.assertEqual(out["glitch"], [])

    def test_glitch_pairs_kept_and_listed_apart(self):
        out = ex.fusions({(31, 34): 100, (22, 31): 200}, {(31, 34), (22, 31)})
        self.assertEqual(out["fusions"], [[22, 31, 200], [31, 34, 100]])
        self.assertEqual(out["glitch"], [[22, 31], [31, 34]])


if __name__ == "__main__":
    unittest.main()
