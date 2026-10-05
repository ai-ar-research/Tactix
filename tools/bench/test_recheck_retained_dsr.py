import tempfile
from pathlib import Path
import unittest

from recheck_retained_dsr import (validate_checker_spelling,
                                  validate_source_result)


class RetainedDsrRecheckTests(unittest.TestCase):
    def source(self, campaign):
        return {
            "task": 0,
            "rank": 12,
            "instance": "tierD/example.cnf.xz",
            "arm": "tactix",
            "verdict": "UNSAT",
            "valid": False,
            "retained_artifact": str(campaign / "retained-proofs/000.dsr"),
            "proof_check": {"checker_input": {
                "original_sha256": "original",
                "checker_sha256": "checker",
            }},
        }

    def test_binds_source_identity_and_contract_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            campaign = Path(temporary).resolve()
            row = {"rank": "12", "instance": "tierD/example.cnf.xz",
                   "arm": "tactix"}
            retention = {"directory": "retained-proofs"}
            source = self.source(campaign)
            self.assertEqual(
                validate_source_result(campaign, retention, row, 0, source),
                campaign / "retained-proofs/000.dsr")
            source["retained_artifact"] = str(campaign / "elsewhere/000.dsr")
            with self.assertRaisesRegex(RuntimeError, "path does not match"):
                validate_source_result(campaign, retention, row, 0, source)

    def test_binds_both_checker_input_hashes(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = self.source(Path(temporary).resolve())
            validate_checker_spelling(source, {
                "original_sha256": "original", "checker_sha256": "checker"})
            with self.assertRaisesRegex(RuntimeError, "checker-CNF hash"):
                validate_checker_spelling(source, {
                    "original_sha256": "original", "checker_sha256": "wrong"})


if __name__ == "__main__":
    unittest.main()
