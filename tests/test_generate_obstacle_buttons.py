import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from generate_obstacle_buttons import parse_layout


class LayoutCategoryNamesTest(unittest.TestCase):
    def setUp(self):
        self.mapping = {
            "Logs": ("Trees", True, 1, None, ()),
            "Rocks": ("Rocks", True, 1, None, ()),
            "Bones": ("Miscellaneous", True, 1, None, ()),
        }
        self.categories = ["Trees", "Rocks", "Miscellaneous"]
        self.rows = [
            ["Trees"], ["Logs"], ["Rocks"], ["Rocks"],
            ["Miscellaneous"], ["Bones"],
        ]

    def test_category_and_button_share_name(self):
        buttons, categories = parse_layout(self.rows, self.categories, self.mapping)
        self.assertEqual(categories, self.categories)
        self.assertEqual([button.name for button in buttons], ["Logs", "Rocks", "Bones"])
        self.assertEqual((buttons[1].category_index, buttons[1].row, buttons[1].column), (1, 0, 0))

    def test_duplicate_category_still_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate category 'Trees'"):
            parse_layout(self.rows + [["Trees"]], self.categories, self.mapping)

    def test_duplicate_shared_name_button_still_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate button 'Rocks'"):
            parse_layout(self.rows[:4] + [["Rocks"]] + self.rows[4:], self.categories, self.mapping)


if __name__ == "__main__":
    unittest.main()
