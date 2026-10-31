"""Validate workbook masks retained for native DEFs, without game data or Excel."""
import importlib.util
import pathlib
import sys
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[1] / "tools" / "generate_obstacle_buttons.py"
SPEC = importlib.util.spec_from_file_location("obstacle_generator_test", SOURCE)
GEN = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = GEN
SPEC.loader.exec_module(GEN)


class WorkbookMaskTests(unittest.TestCase):
    headers = ["Button", "Def", "ID", "SubID", "Source", "Leveraged RandTrn", "Random Mask", "Is Custom"]

    def parse(self, mask, extra=()):
        row = ["Example", "example.def", "134", "0", "hota", "self", mask, "0"]
        return GEN.parse_obstacles([self.headers, row, *extra], {"Example": 0}, {})

    def test_native_mask_is_kept_without_overlay(self):
        metadata, overrides = self.parse("000000000101")
        self.assertEqual(metadata[0].terrain_mask, 5)
        self.assertEqual(overrides, [])

    def test_invalid_native_masks_are_rejected(self):
        for mask in ["", "101", "00000000000x", "000000000000", "001000000000"]:
            with self.subTest(mask=mask), self.assertRaises(ValueError):
                self.parse(mask)

    def test_conflicting_masks_for_same_def_are_rejected(self):
        duplicate = ["Example", "EXAMPLE.DEF", "134", "0", "hota", "self", "000000000100", "0"]
        with self.assertRaisesRegex(ValueError, "conflicting metadata"):
            self.parse("000000000101", [duplicate])


if __name__ == "__main__":
    unittest.main()
