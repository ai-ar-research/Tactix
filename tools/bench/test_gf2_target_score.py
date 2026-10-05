"""Synthetic acceptance and corruption tests for GF(2) campaign telemetry."""
import gzip
import json
import lzma
from pathlib import Path
import tempfile
import unittest

from gf2_target_score import (AFFINE_COUNTERS, LEGACY_SCHEMA,
                              _validate_modern_feedback, classify,
                              expected_checker_hashes, parse_feedback,
                              parse_provenance, raw_yield, score_campaign, sha256,
                              sidecar_descriptor, validate_arm_contract,
                              validate_result,
                              _verify_logical_descriptor)
from experimental_tierd import solver_arguments
from runtime_task import solver_command


def metadata(build_hash, input_hash):
    fields = {
        "build-sha256": build_hash,
        "input-sha256": input_hash,
        "policy": "random-controller",
        "bve-gates": "0",
        "proof-format": "1",
        "proof-enabled": "1",
        "configuration": "synthetic-gf2",
        "structural-provenance": "1",
    }
    return "tactix-frontend-feedback-v1\n" + "".join(
        f"{name}={len(value)}:{value}\n" for name, value in fields.items())


def observation(native, strategy=0):
    return {
        "version": 1,
        "state": {"arena": 1, "touch": 1, "strategy": strategy},
        "sequence": strategy,
        "variables": 3,
        "unassigned": 3,
        "active_unassigned": 3,
        "level": 0,
        "trail": 0,
        "root_population": 0,
        "irredundant_clauses": 2,
        "inconsistent": False,
        "conflict": False,
        "fixpoint": True,
        "searching": False,
        "have_configuration": False,
        "configuration": "vmtf/adaptive-glue/saved",
        "rotation_intervals": 0,
        "next_restart_conflicts": 1,
        "reluctant": [1, 1, 1024],
        "fast_glue": 0,
        "slow_glue": 0,
        "last_action": None,
        "native": native,
    }


def outcome_record(before, after):
    return {
        "status": 1,
        "result": 1,
        "budget_status": 1,
        "unit": 0,
        "work": 8,
        "applications": 1,
        "step": 1,
        "instance": 1,
        "progress": True,
        "discovery_progress": 0,
        "discovery_entry_required": False,
        "discovery_exhausted": False,
        "selection_complete": True,
        "revision": 1,
        "before": before["state"],
        "after": after["state"],
        "composition_overshoot": 0,
        "resumed_search": False,
    }


def write_jsonl(path, rows):
    path.write_text("".join(json.dumps(row, separators=(",", ":")) + "\n"
                            for row in rows))


def delta(left, right):
    changed = {name: value for name, value in right.items()
               if name != "native" and left.get(name) != value}
    native_delta = {name: value for name, value in right["native"].items()
                    if left["native"].get(name) != value}
    result = {"set": changed}
    if native_delta:
        result["native"] = native_delta
    return result


def gzip_v2(path):
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    rows[0]["version"] = 2
    current = rows[1]["observation"]
    for row in rows[2:]:
        if row["type"] == "action":
            before, after = row.pop("before"), row.pop("after")
            row["version"] = 2
            row["before_delta"] = delta(current, before)
            row["after_delta"] = delta(before, after)
            current = after
        elif row["type"] == "session_end":
            final = row.pop("observation")
            row["observation_delta"] = delta(current, final)
            current = final
    logical = b"".join(json.dumps(row, separators=(",", ":")).encode() + b"\n"
                       for row in rows)
    target = path.with_suffix(".jsonl.gz")
    target.write_bytes(gzip.compress(logical, mtime=0))
    return target, logical, rows


def synthetic_streams(directory, build_hash="b" * 64, input_hash="1" * 64):
    before_native = {"decisions": 0, "conflicts": 0, "root_assignments": 0,
                     **{name: 0 for name in AFFINE_COUNTERS}}
    deltas = {
        "affine_consequence_attempts": 1,
        "affine_consequence_applied": 1,
        "affine_consequence_snapshot_clauses": 2,
        "affine_consequence_snapshot_literals": 4,
        "affine_consequence_snapshot_work": 2,
        "affine_consequence_equations": 1,
        "affine_consequence_recognition_work": 3,
        "affine_consequence_preflight_additions": 2,
        "affine_consequence_preflight_deletions": 1,
        "affine_consequence_additions": 2,
        "affine_consequence_deletions": 1,
        "affine_consequence_published": 1,
        "affine_consequence_journal_bytes": 10,
        "affine_consequence_candidate_variables": 2,
        "affine_consequence_basis_work": 5,
        "affine_consequence_rows": 1,
        "affine_consequence_definitions": 1,
        "affine_consequence_extent_growth": 2,
        "affine_consequence_session_work": 1,
        "affine_consequence_transaction_keys": 1,
        "affine_consequence_committed_keys": 1,
    }
    after_native = dict(before_native)
    after_native.update(deltas)
    before, after = observation(before_native), observation(after_native, 1)
    feedback_rows = [
        {"type": "header", "version": 1,
         "metadata": metadata(build_hash, input_hash)},
        {"type": "session_begin", "observation": before},
        {"type": "action", "version": 1, "sequence": 1, "kind": 0,
         "identity": "AffineConsequence", "instance": 1,
         "request": {"version": 1, "sequence": 1, "type": 0,
                     "expected": before["state"],
                     "search": "vmtf/adaptive-glue/saved",
                     "search_components": [0, 0, 0],
                     "invocation": "synthetic-affine",
                     "producer_effort": [{"family": "AffineConsequence",
                                          "status": 0, "complete": True,
                                          "preparation_work": 5,
                                          "enumeration_work": 3,
                                          "total_work": 8, "candidates": 1,
                                          "offers": 1}]},
         "allowance": {"unit": 0, "amount": 1, "native_mode": 2,
                       "native_amount": 10000000},
         "before": before, "after": after,
         "outcome": outcome_record(before, after), "stop": 0},
        {"type": "elapsed_nanos", "value": 100},
        {"type": "session_end", "verdict": 20, "stop": 8,
         "observation": after},
    ]
    feedback = directory / "feedback.jsonl"
    write_jsonl(feedback, feedback_rows)
    common = {"action": 1, "semantic": 1, "owner": 1}
    provenance_rows = [
        {"kind": "session", "version": 1, "input_file_sha256": input_hash},
        {**common, "kind": "affine-basis-begin", "equations": 1, "rows": 1,
         "definitions": 1, "propagation": "deferred-controller"},
        {**common, "kind": "affine-basis-source", "occurrence": 1,
         "source_id": 10, "clause": [1, 2]},
        {**common, "kind": "affine-basis-row", "row": 0, "rhs": False,
         "variables": [1, 2], "sources": [0], "premises": [1]},
        {**common, "kind": "affine-basis-definition-rat", "id": 11,
         "install": True, "clause": [3, -1]},
        {**common, "kind": "affine-basis-proof-rup", "id": 12,
         "install": False, "clause": [3]},
        {**common, "kind": "affine-basis-proof-delete", "id": 11,
         "install": False, "clause": [3, -1]},
        {**common, "kind": "affine-basis-theory-variable", "output": 3,
         "meaning": "output iff first XOR second", "first": 1, "second": 2,
         "projection": "omit-extension",
         "reconstruction": "evaluate-operand-xor"},
        {**common, "kind": "affine-basis-proof-helper", "output": 4,
         "meaning": "retired transition identity fixed true",
         "projection": "omit-extension", "reconstruction": "assign-true"},
        {**common, "kind": "affine-basis-complete", "additions": 2,
         "deletions": 1, "published": 1, "extent_growth": 2},
    ]
    provenance = directory / "provenance.jsonl"
    write_jsonl(provenance, provenance_rows)
    statistics = [
        "c controller-producer AffineConsequence calls=1 work=8 candidates=1 offers=1",
        "c controller-rule AffineConsequence requests=1 applications=1 effective=1 "
        "exhausted=0 stale=0 interrupted=0 work=8 overshoot=0",
    ]
    return feedback, provenance, statistics


class Gf2TargetScoreTests(unittest.TestCase):
    def test_affine_arm_allows_only_explicit_bookkeeping_followups(self):
        with tempfile.TemporaryDirectory() as directory:
            campaign = Path(directory)
            (campaign / "pins").mkdir()
            (campaign / "pins/gf2-affine.profile").write_text("profile\n")
            profile = ("--controller-gf2-affine-profile=" +
                       str(campaign / "pins/gf2-affine.profile"))
            arguments = [
                "--dsr", "--controller-policy=1",
                "--controller-families="
                "AffineConsequence,Propagate,SemanticVariableAdvice",
                "--controller-native-producers=gf2-affine", profile,
            ]
            manifest = {
                "schema": "tactix-experimental-single-tierd-v4-gf2-telemetry",
                "arms": {"tactix": {"certificate": "dsr",
                                      "arguments": arguments}},
                "pin_sha256": {"pins/gf2-affine.profile": "0" * 64},
            }
            validate_arm_contract(campaign, manifest)
            arguments[2] = "--controller-families=AffineConsequence,Lucky"
            with self.assertRaisesRegex(ValueError, "bookkeeping follow-ups"):
                validate_arm_contract(campaign, manifest)

    def test_v2_gzip_stream_bounds_and_delta_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            feedback, _, _ = synthetic_streams(Path(directory))
            compressed, logical, rows = gzip_v2(feedback)
            parsed = parse_feedback(compressed, compressed.stat().st_size,
                                    len(logical), len(rows))
            self.assertTrue(parsed["complete"])
            self.assertEqual(parsed["wire_version"], 2)
            self.assertEqual(parsed["compression"], "gzip")
            self.assertEqual(parsed["bytes"], len(logical))
            descriptor = sidecar_descriptor(compressed, Path(directory), parsed)
            _verify_logical_descriptor(descriptor, parsed)
            descriptor["records"] += 1
            with self.assertRaisesRegex(ValueError, "logical descriptor"):
                _verify_logical_descriptor(descriptor, parsed)
            for stored, decoded, records, message in (
                    (compressed.stat().st_size - 1, len(logical), len(rows), "stored"),
                    (compressed.stat().st_size, len(logical) - 1, len(rows), "decoded"),
                    (compressed.stat().st_size, len(logical), len(rows) - 1, "record")):
                with self.assertRaisesRegex(ValueError, message):
                    parse_feedback(compressed, stored, decoded, records)
            truncated = Path(directory) / "truncated.gz"
            truncated.write_bytes(compressed.read_bytes()[:-4])
            with self.assertRaisesRegex(ValueError, "gzip"):
                parse_feedback(truncated, 1 << 20, 1 << 20, 100)
            rows[2]["after_delta"]["set"]["unknown"] = 1
            tampered = Path(directory) / "tampered.gz"
            raw = b"".join(json.dumps(row, separators=(",", ":")).encode() + b"\n"
                           for row in rows)
            tampered.write_bytes(gzip.compress(raw, mtime=0))
            with self.assertRaisesRegex(ValueError, "unknown observation delta"):
                parse_feedback(tampered, 1 << 20, 1 << 20, 100)
            plain = Path(directory) / "plain-v2.jsonl"
            plain.write_bytes(logical)
            plain_summary = parse_feedback(plain, 1 << 20, 1 << 20, 100)
            with self.assertRaisesRegex(ValueError, "compression"):
                _validate_modern_feedback(plain_summary)

    def test_opt_in_reserves_paths_while_default_command_is_unchanged(self):
        arm = {"arguments": ["--dsr", "--controller-policy=1"]}
        self.assertEqual(solver_command("solver", arm, "in", "proof"),
                         ["solver", "--dsr", "--controller-policy=1", "in", "proof"])
        self.assertEqual(solver_arguments("dsr", ["--feedback=caller"], False),
                         ["--dsr", "--feedback=caller"])
        for argument in ("--feedback=x", "--feedback-replay=x",
                         "--structural-provenance=x",
                         "--controller-gf2-affine-profile=x"):
            with self.subTest(argument=argument), self.assertRaisesRegex(
                    ValueError, "task-local"):
                solver_arguments("dsr", [argument], True)

    def test_positive_streams_preserve_raw_basis_work_and_checked_class(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            feedback_path, provenance_path, statistics = synthetic_streams(root)
            feedback = parse_feedback(feedback_path, 1 << 20)
            provenance = parse_provenance(provenance_path, 1 << 20)
            raw = raw_yield(feedback, provenance, statistics)
            self.assertTrue(feedback["complete"])
            self.assertTrue(provenance["complete"])
            self.assertEqual(raw["work"]["affine_consequence_basis_work"], 5)
            self.assertEqual(raw["work"]["affine_consequence_proof_work"], 0)
            self.assertEqual(raw["structural"]["published_clauses"], 1)
            self.assertEqual(classify(raw, True), "checked-recovery")

    def test_zero_yield_is_a_diagnostic_class(self):
        raw = {"producer": {"calls": 1, "offers": 1},
               "action": {"applications": 1, "effective": 0}}
        self.assertEqual(classify(raw, False), "applied-no-effect")

    def test_duplicate_key_and_nonmonotone_counter_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            feedback, _, _ = synthetic_streams(root)
            feedback.write_text('{"type":"header","type":"header"}\n')
            with self.assertRaisesRegex(ValueError, "duplicate JSON field"):
                parse_feedback(feedback, 1 << 20)
            feedback, _, _ = synthetic_streams(root)
            rows = [json.loads(line) for line in feedback.read_text().splitlines()]
            rows[2]["before"]["native"]["affine_consequence_attempts"] = 2
            with feedback.open("w") as output:
                for row in rows:
                    output.write(json.dumps(row) + "\n")
            with self.assertRaisesRegex(ValueError, "non-monotone"):
                parse_feedback(feedback, 1 << 20)

    def test_corrupt_structural_counts_and_caps_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            feedback, provenance, _ = synthetic_streams(root)
            with self.assertRaisesRegex(ValueError, "byte cap exceeded"):
                parse_feedback(feedback, 10)
            rows = [json.loads(line) for line in provenance.read_text().splitlines()]
            rows[-1]["published"] = 2
            write_jsonl(provenance, rows)
            with self.assertRaisesRegex(ValueError, "published count mismatch"):
                parse_provenance(provenance, 1 << 20)

    def test_truncated_final_record_is_retained_as_incomplete_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            feedback, provenance, _ = synthetic_streams(root)
            feedback.write_bytes(feedback.read_bytes()[:-7])
            provenance.write_bytes(provenance.read_bytes()[:-7])
            self.assertFalse(parse_feedback(feedback, 1 << 20)["complete"])
            self.assertFalse(parse_provenance(provenance, 1 << 20)["complete"])

    def test_end_to_end_result_and_sidecar_tamper(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            campaign = repo / "benchmarks/runs/synthetic"
            for name in ("pins", "results", "telemetry"):
                (campaign / name).mkdir(parents=True, exist_ok=True)
            cache = repo / "benchmarks/cache/tierD"
            cache.mkdir(parents=True)
            cnf = b"p cnf 1 2\n1 0\n-1 0\n"
            compressed = cache / "x.cnf.xz"
            compressed.write_bytes(lzma.compress(cnf))
            input_hash = __import__("hashlib").sha256(cnf).hexdigest()
            checker_names = ("tactix", "checkmodel", "modelcheck.py", "dsr-trim",
                             "dsr-compress", "srcheck", "drat-trim", "cake_lpr")
            for name in checker_names:
                (campaign / "pins" / name).write_text(name)
            (campaign / "pins/gf2-affine.profile").write_text("synthetic profile\n")
            (campaign / "matrix.tsv").write_text(
                "task\trank\tarm\ttier\tinstance\texpected\n"
                "0\t7\ttactix\tD\ttierD/x.cnf.xz\tUNSAT\n")
            authority = repo / "authority.json"
            authority.write_text("{}\n")
            binary_hash = sha256(campaign / "pins/tactix")
            feedback_source, provenance_source, statistics = synthetic_streams(
                campaign / "telemetry", binary_hash, input_hash)
            feedback_summary = parse_feedback(feedback_source, 1 << 20)
            provenance_summary = parse_provenance(provenance_source, 1 << 20)
            manifest = {
                "schema": "tactix-experimental-single-tierd-v4-gf2-telemetry",
                "arms": {"tactix": {"path": "pins/tactix", "sha256": binary_hash,
                                      "certificate": "dsr", "arguments": [
                                          "--dsr", "--controller-policy=1",
                                          "--controller-families=AffineConsequence",
                                          "--controller-native-producers=gf2-affine",
                                          "--controller-gf2-affine-profile=" +
                                          str(campaign / "pins/gf2-affine.profile")] }},
                "input_sha256": {"tierD/x.cnf.xz": sha256(compressed)},
                "dataset_authority": {"path": str(authority),
                                      "sha256": sha256(authority)},
                "selection_authority": None,
                "gf2_telemetry": {"schema": LEGACY_SCHEMA, "directory": "telemetry",
                                  "feedback_max_bytes": 1 << 20,
                                  "structural_provenance_max_bytes": 1 << 20},
                "pin_sha256": {
                    **{"pins/" + name: sha256(campaign / "pins" / name)
                       for name in checker_names},
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
            telemetry = {
                "schema": LEGACY_SCHEMA,
                "bindings": {"manifest_sha256": manifest_hash,
                             "compressed_input_sha256": sha256(compressed),
                             "decompressed_input_sha256": input_hash,
                             "binary_sha256": binary_hash,
                             "checker_sha256": expected_checker_hashes(manifest, arm)},
                "sidecars": {
                    "feedback": sidecar_descriptor(feedback_source, campaign),
                    "structural_provenance": sidecar_descriptor(provenance_source, campaign),
                },
                "feedback": feedback_summary,
                "structural_provenance": provenance_summary,
                "raw_yield": raw_yield(feedback_summary, provenance_summary, statistics),
            }
            result = {
                "schema": "tactix-experimental-gf2-runtime-result-v1",
                "task": 0, "rank": 7, "arm": "tactix", "tier": "D",
                "instance": "tierD/x.cnf.xz", "expected": "UNSAT",
                "verdict": "UNSAT", "valid": True,
                "solve": {"exit_code": 20, "timed_out": False},
                "proof_check": {"result": "verified"}, "model_check": None,
                "solver_statistics": statistics, "gf2_telemetry": telemetry,
            }
            result_path = campaign / "results/000.json"
            result_path.write_text(json.dumps(result) + "\n")
            scored = validate_result(campaign, result_path)
            self.assertEqual(scored["classification"], "checked-recovery")
            self.assertEqual(score_campaign(campaign)["rows"], [scored])
            saved = result_path.read_text()
            result_path.unlink()
            with self.assertRaisesRegex(ValueError, "result set mismatch"):
                score_campaign(campaign)
            result_path.write_text(saved)
            corrupt = json.loads(saved)
            corrupt["rank"] = 8
            result_path.write_text(json.dumps(corrupt) + "\n")
            with self.assertRaisesRegex(ValueError, "result/matrix identity mismatch"):
                validate_result(campaign, result_path)
            result_path.write_text(saved)
            manifest_saved = (campaign / "manifest.json").read_text()
            corrupt_manifest = json.loads(manifest_saved)
            corrupt_manifest["arms"]["tactix"]["arguments"].remove(
                "--controller-native-producers=gf2-affine")
            (campaign / "manifest.json").write_text(json.dumps(corrupt_manifest) + "\n")
            with self.assertRaisesRegex(ValueError, "gf2-affine native producer"):
                validate_result(campaign, result_path)
            (campaign / "manifest.json").write_text(manifest_saved)
            feedback_source.write_text(feedback_source.read_text() + "{}\n")
            with self.assertRaisesRegex(ValueError, "sidecar (size|hash) mismatch"):
                validate_result(campaign, result_path)


if __name__ == "__main__":
    unittest.main()
