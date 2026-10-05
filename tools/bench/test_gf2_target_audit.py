"""Focused tests for fail-closed supplemental GF(2) target diagnosis."""

import hashlib
import json
import lzma
from pathlib import Path
import tempfile
import unittest

from gf2_target_audit import audit_result
from gf2_target_score import (
    SCHEMA,
    expected_checker_hashes,
    parse_feedback,
    parse_provenance,
    raw_yield,
    sha256,
    sidecar_descriptor,
)
from test_gf2_target_score import synthetic_streams, write_jsonl


CHECKERS = ("tactix", "checkmodel", "modelcheck.py", "dsr-trim",
            "dsr-compress", "srcheck", "drat-trim", "cake_lpr")


def make_campaign(root):
    repo = Path(root)
    campaign = repo / "benchmarks/runs/synthetic"
    for name in ("pins", "results", "telemetry"):
        (campaign / name).mkdir(parents=True, exist_ok=True)
    cache = repo / "benchmarks/cache/tierD"
    cache.mkdir(parents=True)
    cnf = b"p cnf 1 2\n1 0\n-1 0\n"
    compressed = cache / "x.cnf.xz"
    compressed.write_bytes(lzma.compress(cnf))
    input_hash = hashlib.sha256(cnf).hexdigest()
    for name in CHECKERS:
        (campaign / "pins" / name).write_text(name)
    (campaign / "pins/gf2-affine.profile").write_text("synthetic profile\n")
    (campaign / "matrix.tsv").write_text(
        "task\trank\tarm\ttier\tinstance\texpected\n"
        "0\t7\ttactix\tD\ttierD/x.cnf.xz\tUNKNOWN\n")
    authority = repo / "authority.json"
    authority.write_text("{}\n")
    binary_hash = sha256(campaign / "pins/tactix")
    feedback_path, provenance_path, statistics = synthetic_streams(
        campaign / "telemetry", binary_hash, input_hash)
    feedback_path.rename(campaign / "telemetry/000.feedback.jsonl")
    provenance_path.rename(campaign / "telemetry/000.provenance.jsonl")
    feedback_path = campaign / "telemetry/000.feedback.jsonl"
    provenance_path = campaign / "telemetry/000.provenance.jsonl"
    manifest = {
        "schema": "tactix-experimental-single-tierd-v4-gf2-telemetry",
        "arms": {"tactix": {
            "path": "pins/tactix", "sha256": binary_hash,
            "certificate": "dsr", "arguments": [
                "--dsr", "--controller-policy=1",
                "--controller-families=AffineConsequence",
                "--controller-native-producers=gf2-affine",
                "--controller-gf2-affine-profile=" +
                str(campaign / "pins/gf2-affine.profile"),
            ],
        }},
        "input_sha256": {"tierD/x.cnf.xz": sha256(compressed)},
        "dataset_authority": {"path": str(authority),
                              "sha256": sha256(authority)},
        "selection_authority": None,
        "gf2_telemetry": {
            "schema": SCHEMA, "directory": "telemetry",
            "feedback_max_bytes": 1 << 20,
            "structural_provenance_max_bytes": 1 << 20,
        },
        "pin_sha256": {
            **{"pins/" + name: sha256(campaign / "pins" / name)
               for name in CHECKERS},
            "pins/gf2-affine.profile": sha256(
                campaign / "pins/gf2-affine.profile"),
            "matrix.tsv": sha256(campaign / "matrix.tsv"),
        },
    }
    (campaign / "manifest.json").write_text(json.dumps(manifest) + "\n")
    manifest_hash = sha256(campaign / "manifest.json")
    (campaign / "launch.json").write_text(
        json.dumps({"manifest_sha256": manifest_hash}) + "\n")
    arm = manifest["arms"]["tactix"]
    result = {
        "schema": "tactix-experimental-gf2-runtime-result-v1",
        "task": 0, "rank": 7, "arm": "tactix", "tier": "D",
        "instance": "tierD/x.cnf.xz", "expected": "UNKNOWN",
        "verdict": "UNSAT", "expected_match": True, "valid": True,
        "solve": {"exit_code": 20, "timed_out": False, "wall_seconds": 1.0},
        "proof_check": {"result": "verified", "timed_out": False,
                        "pipeline": "synthetic", "stages": []},
        "model_check": None,
        "solver_statistics": statistics,
        "gf2_telemetry": {
            "schema": SCHEMA,
            "bindings": {
                "manifest_sha256": manifest_hash,
                "compressed_input_sha256": sha256(compressed),
                "decompressed_input_sha256": input_hash,
                "binary_sha256": binary_hash,
                "checker_sha256": expected_checker_hashes(manifest, arm),
            },
            "sidecars": {},
        },
    }
    result_path = campaign / "results/000.json"
    refresh(campaign, result, feedback_path, provenance_path)
    result_path.write_text(json.dumps(result) + "\n")
    return campaign, result_path, feedback_path, provenance_path


def refresh(campaign, result, feedback_path, provenance_path):
    manifest = json.loads((campaign / "manifest.json").read_text())
    contract = manifest["gf2_telemetry"]
    feedback = parse_feedback(feedback_path, contract["feedback_max_bytes"])
    provenance = parse_provenance(
        provenance_path, contract["structural_provenance_max_bytes"])
    telemetry = result["gf2_telemetry"]
    telemetry.pop("integrity_error", None)
    telemetry["sidecars"] = {
        "feedback": sidecar_descriptor(feedback_path, campaign),
        "structural_provenance": sidecar_descriptor(provenance_path, campaign),
    }
    telemetry["feedback"] = feedback
    telemetry["structural_provenance"] = provenance
    telemetry["raw_yield"] = raw_yield(
        feedback, provenance, result["solver_statistics"])


def read_rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


class Gf2TargetAuditTests(unittest.TestCase):
    def test_checked_sat_is_separate_from_producer_classification(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, feedback_path, provenance_path = make_campaign(directory)
            result = json.loads(result_path.read_text())
            result.update(expected="UNKNOWN", verdict="SAT", expected_match=True,
                          valid=True, solve={"exit_code": 10, "timed_out": False,
                                             "wall_seconds": 1.0},
                          proof_check=None,
                          model_check={"result": "verified", "timed_out": False})
            rows = read_rows(feedback_path)
            rows[-1].update(verdict=10, stop=7)
            write_jsonl(feedback_path, rows)
            refresh(campaign, result, feedback_path, provenance_path)
            result_path.write_text(json.dumps(result) + "\n")

            report = audit_result(campaign, result_path)
            self.assertTrue(report["evidence_complete"])
            self.assertEqual(report["solve"]["disposition"], "solver-sat")
            self.assertEqual(report["check"]["disposition"], "checked-sat")
            self.assertEqual(report["producer_action"]["frozen_classification"],
                             "checked-recovery")
            self.assertEqual(report["producer_action"]["producer_statuses"],
                             {"complete": 1})
            self.assertEqual(report["producer_action"]["action_statuses"],
                             {"scoped-fixpoint": 1})
            self.assertFalse(report["terminal_x9_refutation"]["recognized"])

    def test_checked_unsat_direct_refutation_overrides_zero_effect_diagnostically(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, feedback_path, provenance_path = make_campaign(directory)
            result = json.loads(result_path.read_text())
            feedback_rows = read_rows(feedback_path)
            action = feedback_rows[2]
            action["outcome"]["status"] = 8
            action["outcome"]["result"] = 4
            action["outcome"]["progress"] = False
            write_jsonl(feedback_path, feedback_rows)
            provenance_rows = read_rows(provenance_path)
            proof_event = next(row for row in provenance_rows
                               if row.get("kind") == "affine-basis-proof-rup")
            proof_event["install"] = True
            proof_event["clause"] = []
            provenance_rows[-1]["published"] = 2
            write_jsonl(provenance_path, provenance_rows)
            feedback_rows = read_rows(feedback_path)
            feedback_rows[2]["after"]["native"][
                "affine_consequence_published"] = 2
            feedback_rows[-1]["observation"]["native"][
                "affine_consequence_published"] = 2
            write_jsonl(feedback_path, feedback_rows)
            result["solver_statistics"][1] = result["solver_statistics"][1].replace(
                "effective=1", "effective=0")
            refresh(campaign, result, feedback_path, provenance_path)
            result_path.write_text(json.dumps(result) + "\n")

            report = audit_result(campaign, result_path)
            self.assertTrue(report["evidence_complete"])
            self.assertEqual(report["producer_action"]["frozen_classification"],
                             "applied-no-effect")
            self.assertEqual(
                report["producer_action"]["supplemental_classification"],
                "checked-terminal-x9-refutation")
            self.assertTrue(report["terminal_x9_refutation"]["recognized"])
            self.assertTrue(report["terminal_x9_refutation"]["independently_checked"])

    def test_solve_timeout_remains_distinct_from_x9_yield(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, feedback_path, provenance_path = make_campaign(directory)
            result = json.loads(result_path.read_text())
            result.update(verdict="UNKNOWN", expected_match=False, valid=False,
                          solve={"exit_code": None, "timed_out": True,
                                 "wall_seconds": 1200.0},
                          proof_check=None)
            rows = read_rows(feedback_path)
            rows[-1].update(verdict=0, stop=5)
            write_jsonl(feedback_path, rows)
            refresh(campaign, result, feedback_path, provenance_path)
            result_path.write_text(json.dumps(result) + "\n")

            report = audit_result(campaign, result_path)
            self.assertTrue(report["evidence_complete"])
            self.assertEqual(report["solve"]["disposition"], "solver-timeout")
            self.assertEqual(report["check"]["disposition"],
                             "not-run-after-solver-timeout")
            self.assertEqual(report["producer_action"]["frozen_classification"],
                             "effective-unchecked")

    def test_checker_failure_and_timeout_are_distinct(self):
        for timeout, expected in ((False, "checker-failure"),
                                  (True, "checker-timeout")):
            with self.subTest(timeout=timeout), tempfile.TemporaryDirectory() as directory:
                campaign, result_path, _, _ = make_campaign(directory)
                result = json.loads(result_path.read_text())
                result["valid"] = False
                result["proof_check"] = {
                    "result": "not-verified", "timed_out": timeout,
                    "deadline_expired": timeout, "pipeline": "synthetic",
                    "stages": [{"name": "checker", "result": (
                        "timeout" if timeout else "rejected"),
                        "exit_code": None if timeout else 1,
                        "timed_out": timeout}],
                }
                result_path.write_text(json.dumps(result) + "\n")
                report = audit_result(campaign, result_path)
                self.assertTrue(report["evidence_complete"])
                self.assertEqual(report["check"]["disposition"], expected)
                self.assertEqual(report["producer_action"]["frozen_classification"],
                                 "effective-unchecked")

    def test_missing_and_incomplete_telemetry_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, _, _ = make_campaign(directory)
            result = json.loads(result_path.read_text())
            del result["gf2_telemetry"]
            result_path.write_text(json.dumps(result) + "\n")
            report = audit_result(campaign, result_path)
            self.assertFalse(report["evidence_complete"])
            self.assertEqual(report["telemetry_disposition"], "missing-telemetry")

        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, feedback_path, provenance_path = make_campaign(directory)
            result = json.loads(result_path.read_text())
            data = feedback_path.read_bytes()
            feedback_path.write_bytes(data[:-8])
            refresh(campaign, result, feedback_path, provenance_path)
            result_path.write_text(json.dumps(result) + "\n")
            report = audit_result(campaign, result_path)
            self.assertFalse(report["evidence_complete"])
            self.assertEqual(report["telemetry_disposition"], "incomplete-telemetry")

    def test_provenance_native_action_count_mismatch_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, feedback_path, provenance_path = make_campaign(directory)
            result = json.loads(result_path.read_text())
            rows = read_rows(feedback_path)
            rows[2]["after"]["native"]["affine_consequence_applied"] = 2
            rows[-1]["observation"]["native"]["affine_consequence_applied"] = 2
            write_jsonl(feedback_path, rows)
            refresh(campaign, result, feedback_path, provenance_path)
            result_path.write_text(json.dumps(result) + "\n")
            report = audit_result(campaign, result_path)
            self.assertFalse(report["evidence_complete"])
            self.assertEqual(report["telemetry_disposition"],
                             "supplemental-validation-failed")
            self.assertEqual(report["issues"][0]["code"],
                             "provenance-action-count-mismatch")

    def test_feedback_runtime_verdict_mismatch_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, _, _ = make_campaign(directory)
            result = json.loads(result_path.read_text())
            result.update(verdict="SAT", expected_match=True, valid=True,
                          solve={"exit_code": 10, "timed_out": False,
                                 "wall_seconds": 1.0},
                          proof_check=None,
                          model_check={"result": "verified", "timed_out": False})
            result_path.write_text(json.dumps(result) + "\n")
            report = audit_result(campaign, result_path)
            self.assertFalse(report["evidence_complete"])
            self.assertEqual(report["issues"][0]["code"],
                             "feedback-result-verdict-mismatch")

    def test_cap_loss_and_missing_sidecar_are_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, _, _ = make_campaign(directory)
            result = json.loads(result_path.read_text())
            telemetry = result["gf2_telemetry"]
            telemetry["sidecars"] = {}
            telemetry["integrity_error"] = (
                "feedback: telemetry byte cap exceeded: feedback.jsonl")
            for name in ("feedback", "structural_provenance", "raw_yield"):
                telemetry.pop(name, None)
            result_path.write_text(json.dumps(result) + "\n")
            report = audit_result(campaign, result_path)
            self.assertFalse(report["evidence_complete"])
            self.assertEqual(report["telemetry_disposition"], "cap-loss")

        with tempfile.TemporaryDirectory() as directory:
            campaign, result_path, _, provenance_path = make_campaign(directory)
            provenance_path.unlink()
            report = audit_result(campaign, result_path)
            self.assertFalse(report["evidence_complete"])
            self.assertEqual(report["telemetry_disposition"], "missing-sidecars")


if __name__ == "__main__":
    unittest.main()
