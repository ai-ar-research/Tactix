"""Contract boundaries for the paper-target full Tier-D scorer."""

import csv
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tierd_paper_regression import checked_result, substituted_results


def contract():
    return {"memory_mib": 32768, "solve_timeout": 5000,
            "checker_timeout": 28800}


def base_result(verdict="SAT"):
    result = {
        "instance": "case.cnf.xz", "expected": verdict,
        "verdict": verdict, "expected_match": True, "valid": True,
        "artifact_deleted": True, "derived_artifacts_deleted": True,
        "cleanup_pending": False,
        "solve": {"timed_out": False, "wall_seconds": 10,
                  "peak_rss_kb": 1000,
                  "exit_code": {"SAT": 10, "UNSAT": 20}[verdict]},
    }
    if verdict == "SAT":
        result["model_check"] = {"result": "verified", "exit_code": 0,
                                 "timed_out": False}
    else:
        result["proof_check"] = {
            "pipeline": "veripb-cake-pb", "result": "verified",
            "timed_out": False, "wall_seconds": 20,
            "stages": [{"name": "veripb", "result": "verified"},
                       {"name": "cake-pb-cnf", "result": "verified"}],
        }
    return result


class TierdPaperContractTests(unittest.TestCase):
    def test_independently_checked_sat_and_unsat_pass(self):
        for verdict in ("SAT", "UNSAT"):
            row = {"instance": "case.cnf.xz", "expected": verdict}
            status, seconds, errors = checked_result(
                row, base_result(verdict), contract())
            self.assertEqual((status, seconds, errors),
                             ("verified-" + verdict, 10, []))

    def test_clean_solver_timeout_is_unresolved_not_infrastructure_failure(self):
        row = {"instance": "case.cnf.xz", "expected": "UNSAT"}
        result = base_result("UNSAT")
        result.update(verdict="UNKNOWN", expected_match=False, valid=False,
                      proof_check=None)
        result["solve"].update(timed_out=True, wall_seconds=5000.005,
                               exit_code=None)
        self.assertEqual(checked_result(row, result, contract()),
                         ("solver-timeout", 5000.005, []))

    def test_clean_proof_timeout_is_censored_but_not_verified(self):
        row = {"instance": "case.cnf.xz", "expected": "UNSAT"}
        result = base_result("UNSAT")
        result["valid"] = False
        result["proof_check"] = {
            "result": "not-verified", "timed_out": True,
            "wall_seconds": 28800.001,
            "stages": [{"name": "pipeline-deadline", "result": "timeout",
                        "timed_out": True}],
        }
        status, _, errors = checked_result(row, result, contract())
        self.assertEqual(status, "censored-UNSAT-timeout")
        self.assertEqual(errors, [])

    def test_rejected_proof_and_invalid_timeout_evidence_fail(self):
        row = {"instance": "case.cnf.xz", "expected": "UNSAT"}
        result = base_result("UNSAT")
        result["valid"] = False
        result["proof_check"] = {
            "result": "not-verified", "timed_out": False,
            "wall_seconds": 3,
            "stages": [{"name": "veripb", "result": "rejected",
                        "exit_code": 1, "output_tail": "s INVALID"}],
        }
        self.assertIn("rejected", checked_result(row, result, contract())[2][0])
        result = base_result("UNSAT")
        result.update(verdict="UNKNOWN", valid=False, proof_check=None)
        result["solve"].update(timed_out=True, exit_code=20)
        self.assertIn("invalid solver-timeout evidence",
                      checked_result(row, result, contract())[2])

    def test_multiple_direct_rechecks_overlay_in_argument_order(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "base"
            base.joinpath("results").mkdir(parents=True)
            rows = [{"task": str(i), "rank": str(i + 1), "arm": "tactix",
                     "tier": "D", "instance": f"i{i}.cnf.xz", "expected": "UNSAT"}
                    for i in range(1166)]
            with base.joinpath("matrix.tsv").open("w", newline="") as out:
                writer = csv.DictWriter(out, fieldnames=rows[0], delimiter="\t")
                writer.writeheader(); writer.writerows(rows)
            arm = {"path": "pins/tactix", "sha256": "a" * 64,
                   "arguments": [], "certificate": "pbp"}
            base_manifest = {"pin_sha256": {}, "arms": {"tactix": arm}}
            base.joinpath("manifest.json").write_text(json.dumps(base_manifest))
            for task in (1, 2):
                base.joinpath("results", f"{task:03d}.json").write_text(json.dumps(
                    {"task": task, "rank": task + 1, "arm": "tactix",
                     "binary_sha256": "a" * 64, "instance": f"i{task}.cnf.xz",
                     "marker": "base"}))
            base_manifest_sha = hashlib.sha256(base.joinpath("manifest.json").read_bytes()).hexdigest()

            def make_recheck(name, source_tasks, marker):
                campaign = root / name
                campaign.joinpath("results").mkdir(parents=True)
                matrix = []
                selected = {}
                for local, source_task in enumerate(source_tasks):
                    matrix.append({"task": str(local), "source_task": str(source_task),
                                   "instance": f"i{source_task}.cnf.xz"})
                    source_path = base / "results" / f"{source_task:03d}.json"
                    selected[f"results/{source_task:03d}.json"] = hashlib.sha256(
                        source_path.read_bytes()).hexdigest()
                    campaign.joinpath("results", f"{local:03d}.json").write_text(json.dumps(
                        {"task": local, "arm": "tactix", "binary_sha256": "a" * 64,
                         "instance": f"i{source_task}.cnf.xz", "marker": marker}))
                with campaign.joinpath("matrix.tsv").open("w", newline="") as out:
                    writer = csv.DictWriter(out, fieldnames=("task", "source_task", "instance"),
                                            delimiter="\t")
                    writer.writeheader(); writer.writerows(matrix)
                campaign.joinpath("manifest.json").write_text(json.dumps({
                    "pin_sha256": {}, "arms": {"tactix": arm},
                    "resource_contract": {"memory_mib": 65536, "solve_timeout": 5000,
                                          "checker_timeout": 28800},
                    "recheck_source": {"manifest_sha256": base_manifest_sha,
                                       "selected_result_sha256": selected}}))
                return campaign

            first = make_recheck("first", (1,), "first")
            second = make_recheck("second", (1, 2), "second")
            _, _, results = substituted_results(base, [first, second])
            self.assertEqual(results[1]["marker"], "second")
            self.assertEqual(results[2]["marker"], "second")
            second.joinpath("results/001.json").unlink()
            with self.assertRaisesRegex(ValueError, "recheck result missing: second/001"):
                substituted_results(base, [first, second])


if __name__ == "__main__":
    unittest.main()
