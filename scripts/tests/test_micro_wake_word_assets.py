import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class MicroWakeWordAssetTests(unittest.TestCase):
    def test_hey_eve_model_is_a_tflite_asset_and_legacy_model_remains(self):
        eve = ROOT / "main/assets/wakewords/hey_eve.tflite"
        legacy = ROOT / "main/assets/wakewords/hey_apolo.tflite"

        self.assertTrue(eve.is_file())
        self.assertTrue(legacy.is_file())

        payload = eve.read_bytes()
        self.assertGreater(len(payload), 1024)
        self.assertEqual(payload[4:8], b"TFL3")


if __name__ == "__main__":
    unittest.main()
