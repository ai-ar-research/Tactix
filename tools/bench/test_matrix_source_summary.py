import json
from pathlib import Path
import tempfile
import unittest

from matrix_source_summary import summarize
from runtime_task import matrix_source_summary_contract


INPUT = "a" * 64


def equation(ordinal, variables, rhs=0):
    return {"ordinal": ordinal, "variables": variables, "rhs": rhs}


class MatrixSourceSummaryTest(unittest.TestCase):
    def write(self, rows):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "provenance.jsonl"
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))
        return path

    def test_mixed_cache_and_source_overlap(self):
        path = self.write([
            {"kind": "session", "version": 1, "input_file_sha256": INPUT},
            {"kind": "affine-matrix-session"},
            {"kind": "affine-matrix-reason", "source_ordinals": [0, 2, 4],
             "sources": [equation(0, [1, 2]), equation(2, [2, 3]),
                         equation(4, [3, 4])],
             "proof_cache": {"status": "insert", "entry_id": 7},
             "proof_events": [{"kind": "rat", "clause": [1, -2]},
                              {"kind": "rup", "clause": [3]}]},
            {"kind": "affine-matrix-reason", "sources_ref": {"cache_entry": 7},
             "proof_cache": {"status": "hit"},
             "proof_events": [{"kind": "rup", "clause": [4]}]},
            {"kind": "affine-matrix-reason", "source_ordinals": [0, 2, 5],
             "sources": [equation(0, [1, 2]), equation(2, [2, 3]),
                         equation(5, [3, 5])],
             "proof_cache": {"status": "disabled"},
             "proof_events": [{"kind": "rup", "clause": [5]}]},
            {"kind": "affine-matrix-reason", "source_ordinals": [0, 2, 4, 6],
             "sources": [equation(0, [1, 2]), equation(2, [2, 3]),
                         equation(4, [3, 4]), equation(6, [6])],
             "proof_cache": {"status": "disabled"},
             "proof_events": [{"kind": "rup", "clause": [6]}]},
        ])
        result = summarize(path, 100000, INPUT)
        self.assertEqual(result["reasons"], 4)
        self.assertEqual(result["distinct_source_sets"], 3)
        self.assertEqual(result["cache_hits"], 1)
        self.assertEqual(result["cache_inserts"], 1)
        self.assertEqual(result["distinct_row_signatures"], 3)
        self.assertEqual(result["same_row_different_sources"], 0)
        self.assertEqual(result["exact_repeat"], 1)
        self.assertEqual(result["total_sources"], 13)
        self.assertEqual(result["best_prefix_sources"], 8)
        self.assertEqual(result["best_intersection_sources"], 8)
        self.assertEqual(result["best_prior_subset_sources"], 6)
        self.assertEqual(result["reasons_with_prior_subset"], 2)
        self.assertEqual(result["nearest_prior_delta_sources"], 3)
        self.assertEqual(result["nearest_prior_sources"], 9)
        self.assertEqual(result["reasons_with_prior"], 3)
        self.assertEqual(result["proof_additions"], 5)
        self.assertEqual(result["rat_additions"], 1)
        self.assertEqual(result["rup_additions"], 4)
        self.assertEqual(result["added_clause_literals"], 6)
        self.assertEqual(result["maximum_reason_additions"], 2)
        self.assertEqual(result["addition_size_buckets"], {1: 3, 2: 1})
        self.assertEqual(result["additions_by_source_width"], {3: 4, 4: 1})

    def test_detects_same_row_from_different_sources(self):
        path = self.write([
            {"kind": "session", "version": 1, "input_file_sha256": INPUT},
            {"kind": "affine-matrix-reason", "source_ordinals": [0, 1],
             "sources": [equation(0, [1, 2]), equation(1, [2, 3])],
             "proof_cache": {"status": "disabled"},
             "proof_events": [{"kind": "rup", "clause": [1]}]},
            {"kind": "affine-matrix-reason", "source_ordinals": [2],
             "sources": [equation(2, [1, 3])],
             "proof_cache": {"status": "disabled"},
             "proof_events": [{"kind": "rup", "clause": [3]}]},
        ])
        result = summarize(path, 100000, INPUT)
        self.assertEqual(result["distinct_source_sets"], 2)
        self.assertEqual(result["distinct_row_signatures"], 1)
        self.assertEqual(result["same_row_different_sources"], 1)

    def test_rejects_wrong_input_and_missing_cache_source(self):
        path = self.write([
            {"kind": "session", "version": 1, "input_file_sha256": INPUT},
            {"kind": "affine-matrix-reason", "sources_ref": {"cache_entry": 99},
             "proof_cache": {"status": "hit"}, "proof_events": []},
        ])
        with self.assertRaisesRegex(ValueError, "session/input mismatch"):
            summarize(path, 100000, "b" * 64)
        with self.assertRaisesRegex(ValueError, "cache hit lacks"):
            summarize(path, 100000, INPUT)
        with self.assertRaisesRegex(ValueError, "declared cap"):
            summarize(path, 10, INPUT)

    def test_contract_rejects_unbounded_or_conflicting_telemetry(self):
        contract = {"schema": "tactix-matrix-source-overlap-v1",
                    "maximum_bytes": 256 << 20}
        self.assertEqual(matrix_source_summary_contract(
            {"matrix_source_summary": contract}), contract)
        with self.assertRaisesRegex(RuntimeError, "invalid matrix"):
            matrix_source_summary_contract({"matrix_source_summary":
                {**contract, "maximum_bytes": (256 << 20) + 1}})
        with self.assertRaisesRegex(RuntimeError, "invalid matrix"):
            matrix_source_summary_contract({"matrix_source_summary": contract,
                                            "gf2_telemetry": {}})


if __name__ == "__main__":
    unittest.main()
