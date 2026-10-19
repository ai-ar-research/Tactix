#!/usr/bin/env python3
"""Freeze the preserved paper release as the controller regression target.

This reads saved evidence only.  The paper manifests, results, pins, and the
older baseline they used remain unchanged and are bound by hashes.  A candidate
may be at most ten percent slower in aggregate than the paper measurement.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PAPER_COMMIT = "0f7c10174faa13e41e3ab60654bf8fc3322f4041"
PAPER_BINARY = "a7c66babcd568a3497e0b46271b0d25941c59042333b252527f63e40247a4907"
RUNTIME_FACTOR = 1.10
SETS = {
    "b28c5": ("20261015-prod-samewindow-regression-b28c5", 33),
    "rank1-50": ("20261015-prod-samewindow-regression-rank1-50", 50),
    "wv100": ("20261015-prod-samewindow-regression-wv100", 100),
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def independently_verified(row):
    if row["verdict"] == "SAT":
        check = row.get("model_check") or {}
        return (check.get("result") == "verified" and
                check.get("exit_code") == 0 and not check.get("timed_out"))
    check = row.get("proof_check") or {}
    return (check.get("result") == "verified" and
            not check.get("timed_out") and check.get("stages") and
            all(stage.get("result") == "verified"
                for stage in check["stages"]))


def clean_censored_proof(row):
    check = row.get("proof_check") or {}
    stages = check.get("stages") or []
    return (row.get("verdict") == "UNSAT" and check.get("timed_out") and
            check.get("result") != "verified" and
            any(stage.get("timed_out") for stage in stages) and
            all(stage.get("result") == "verified" or stage.get("timed_out")
                for stage in stages))


def collect():
    snapshot = {
        "schema": "tactix-controller-paper-baseline-v1",
        "target_source_commit": PAPER_COMMIT,
        "target_solver_sha256": PAPER_BINARY,
        "runtime_tolerance_fraction": 0.10,
        "regression_contract": "paper release plus ten percent aggregate solver time",
        "sets": {},
    }
    for name, (folder_name, expected_count) in SETS.items():
        relative = Path("benchmarks/runs") / folder_name
        folder = ROOT / relative
        manifest_path = folder / "manifest.json"
        manifest = json.loads(manifest_path.read_text())
        assert manifest["set"] == name
        assert len(manifest["arms"]) == 1
        arm_name, arm = next(iter(manifest["arms"].items()))
        assert arm["source_commit"] == PAPER_COMMIT
        assert arm["sha256"] == PAPER_BINARY
        assert arm["arguments"] == ["--auto-proof"]
        for pin, expected_hash in manifest["pin_sha256"].items():
            assert sha256(folder / pin) == expected_hash, folder / pin
        assert sha256(folder / "matrix.tsv") == manifest["pin_sha256"]["matrix.tsv"]
        with (folder / "matrix.tsv").open() as source:
            matrix = list(csv.DictReader(source, delimiter="\t"))
        assert len(matrix) == expected_count
        files = sorted((folder / "results").glob("*.json"))
        assert len(files) == expected_count, (name, len(files))
        rows = []
        independently_checked = 0
        solve_times = []
        for task, path in enumerate(files):
            row = json.loads(path.read_text())
            expected = matrix[task]
            assert row["task"] == task
            assert row["arm"] == arm_name
            assert row["binary_sha256"] == PAPER_BINARY
            assert str(row["rank"]) == expected["rank"]
            assert all(row[key] == expected[key]
                       for key in ("instance", "expected"))
            assert row["artifact_deleted"] and row["derived_artifacts_deleted"]
            assert not row["cleanup_pending"] and not row.get("error")
            solve = row["solve"]
            timed_out = bool(solve.get("timed_out"))
            if timed_out:
                assert row.get("verdict") == "UNKNOWN"
                assert solve.get("exit_code") is None
                assert 0 <= solve["wall_seconds"] <= (
                    manifest["resource_contract"]["solve_timeout"] + 1)
            else:
                assert row["verdict"] in ("SAT", "UNSAT")
                assert row["verdict"] == row["expected"] and row["expected_match"]
                assert solve["exit_code"] == {"SAT": 10, "UNSAT": 20}[row["verdict"]]
                assert independently_verified(row) or clean_censored_proof(row)
                independently_checked += independently_verified(row)
                solve_times.append(solve["wall_seconds"])
            retained = {key: row[key] for key in (
                "instance", "rank", "expected", "verdict", "expected_match",
                "valid", "solve", "model_check", "proof_check",
                "certificate_sha256", "certificate_bytes") if key in row}
            retained.update(
                source=str(relative / "results" / path.name),
                source_sha256=sha256(path),
                require_solver_retention=not timed_out,
            )
            rows.append(retained)
        paper_seconds = round(math.fsum(solve_times), 6)
        ceiling = round(paper_seconds * RUNTIME_FACTOR, 6)
        snapshot["sets"][name] = {
            "source_manifest": str(relative / "manifest.json"),
            "source_manifest_sha256": sha256(manifest_path),
            "matrix_sha256": sha256(folder / "matrix.tsv"),
            "resource_contract": manifest["resource_contract"],
            "pin_sha256": manifest["pin_sha256"],
            "input_sha256": manifest["input_sha256"],
            "expected_rows": expected_count,
            "independently_verified_rows": independently_checked,
            "paper_solver_seconds": paper_seconds,
            "runtime_tolerance_fraction": 0.10,
            "acceptance_runtime_seconds": ceiling,
            "rows": rows,
        }
    return snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path,
        default=ROOT / "benchmarks/baselines/controller-paper-v1/results.json")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    snapshot = collect()
    if args.verify:
        assert json.loads(args.output.read_text()) == snapshot
        rows = sum(value["expected_rows"] for value in snapshot["sets"].values())
        checked = sum(value["independently_verified_rows"]
                      for value in snapshot["sets"].values())
        print(f"Verified paper controller target: {rows} rows, {checked} independently checked; source hashes match.")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x") as target:
            json.dump(snapshot, target, indent=2, sort_keys=True)
            target.write("\n")
        print(args.output)


if __name__ == "__main__":
    main()
