import json
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build" / "voyager"


def run(*args):
    return subprocess.run(
        [str(BINARY), *args], cwd=ROOT, text=True, capture_output=True, check=False
    )


class AnalyzerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["make"], cwd=ROOT, check=True, capture_output=True, text=True)

    def test_exact_record_returns_json(self):
        result = run("voyager1", "1977-Sep-05", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload["mode"], "exact")
        self.assertEqual(payload["spacecraft"], "voyager1")
        self.assertAlmostEqual(payload["distance_au"], 1.0, delta=0.1)

    def test_between_records_is_interpolated(self):
        result = run("voyager1", "1977-Sep-20", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["mode"], "interpolated")

    def test_outside_range_is_estimated(self):
        result = run("voyager1", "2100-Jan-01", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["mode"], "estimated")

    def test_rejects_unknown_spacecraft(self):
        result = run("pioneer", "2024-Jan-01")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unknown spacecraft", result.stderr)

    def test_rejects_impossible_date(self):
        result = run("voyager1", "2024-Feb-30")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Invalid date", result.stderr)


if __name__ == "__main__":
    unittest.main()
