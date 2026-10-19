"""Contract boundaries for the paper-target full Tier-D scorer."""

import unittest

from tierd_paper_regression import checked_result


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


if __name__ == "__main__":
    unittest.main()
