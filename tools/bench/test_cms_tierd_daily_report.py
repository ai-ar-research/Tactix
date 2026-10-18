import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from cms_tierd_daily_report import merged_campaign_state


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class MergedDailyReportTests(unittest.TestCase):
    def test_merges_disjoint_base_and_repair_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, repair = root / "base", root / "repair"
            for campaign in (base, repair):
                for name in ("results", "pending", "claims"):
                    (campaign / name).mkdir(parents=True, exist_ok=True)
            base_manifest = {"tasks": [0, 1, 2]}
            (base / "manifest.json").write_text(json.dumps(base_manifest))
            repair_manifest = {
                "tasks": [0, 1, 2],
                "repair_of": {"campaign": "base",
                              "manifest_sha256": sha(base / "manifest.json")},
            }
            (repair / "manifest.json").write_text(json.dumps(repair_manifest))
            (base / "results/00000.json").write_text("{}")
            (repair / "results/00002.json").write_text("{}")
            state = merged_campaign_state(base, [repair])
            self.assertEqual(state[1], 3)
            self.assertEqual(set(state[2]), {0, 2})

    def test_duplicate_result_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base, repair = root / "base", root / "repair"
            for campaign in (base, repair):
                for name in ("results", "pending", "claims"):
                    (campaign / name).mkdir(parents=True, exist_ok=True)
            (base / "manifest.json").write_text(json.dumps({"tasks": [0]}))
            (repair / "manifest.json").write_text(json.dumps({
                "tasks": [0], "repair_of": {"campaign": "base",
                "manifest_sha256": sha(base / "manifest.json")}}))
            (base / "results/00000.json").write_text("{}")
            (repair / "results/00000.json").write_text("{}")
            with self.assertRaisesRegex(RuntimeError, "duplicate"):
                merged_campaign_state(base, [repair])


if __name__ == "__main__":
    unittest.main()
