#!/usr/bin/env python3
"""Freeze the accepted AM549 rows for controller regression comparisons.

This reads saved evidence only. Original manifests, results and pins remain
authoritative and are bound by hashes; certificates are not regenerated.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SETS = {"rank1-50": (50, 50), "b28c5": (33, 33),
        "wv100": (100, 99), "wv2-89": (89, 79)}
BINARY = "2fc7421f4d66ed013756e176bd2d6cd0f0bda4876270830704ea2029ed0eb970"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def collect():
    snapshot = {"schema": "tactix-controller-regression-baseline-v1",
                "accepted_solver_sha256": BINARY,
                "regression_contract": "DEVELOPMENT_PLAN.md#84-flexibility",
                "sets": {}}
    pin_hashes = {}
    for name, (count, checked) in SETS.items():
        relative = Path("benchmarks/runs") / ("20260928-am549-no-mab-hybrid-" + name)
        folder = ROOT / relative
        manifest = json.loads((folder / "manifest.json").read_text())
        assert len(manifest["arms"]) == 1
        arm_name, arm = next(iter(manifest["arms"].items()))
        assert arm["sha256"] == BINARY
        for pin, expected_hash in manifest["pin_sha256"].items():
            path = folder / pin
            stat = path.stat()
            identity = (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns)
            if identity not in pin_hashes:
                pin_hashes[identity] = sha256(path)
            assert pin_hashes[identity] == expected_hash, path
        assert sha256(folder / "matrix.tsv") == manifest["pin_sha256"]["matrix.tsv"]
        files = sorted((folder / "results").glob("*.json"))
        assert len(files) == count, (name, len(files))
        rows, identities = [], set()
        for path in files:
            row = json.loads(path.read_text())
            assert row["arm"] == arm_name
            assert row["binary_sha256"] == BINARY
            assert row["instance"] not in identities
            identities.add(row["instance"])
            assert row["artifact_deleted"] and row["derived_artifacts_deleted"]
            assert not row["cleanup_pending"]
            retained = {key: row[key] for key in (
                "instance", "rank", "expected", "verdict", "expected_match",
                "valid", "solve", "model_check", "proof_check",
                "certificate_sha256", "certificate_bytes") if key in row}
            retained["source"] = str(relative / "results" / path.name)
            retained["source_sha256"] = sha256(path)
            retained["require_solver_retention"] = row["valid"]
            if row["valid"]:
                assert row["expected_match"]
                check = row.get("model_check") or row.get("proof_check")
                assert check and check["result"] == "verified", (name, row["rank"])
            else:
                check = row.get("proof_check") or {}
                assert row["solve"].get("timed_out") or check.get("timed_out"), (name, row["rank"])
            rows.append(retained)
        assert sum(row["valid"] for row in rows) == checked
        snapshot["sets"][name] = {
            "source_manifest": str(relative / "manifest.json"),
            "source_manifest_sha256": sha256(folder / "manifest.json"),
            "matrix_sha256": sha256(folder / "matrix.tsv"),
            "resource_contract": manifest["resource_contract"],
            "pin_sha256": manifest["pin_sha256"],
            "input_sha256": manifest["input_sha256"],
            "expected_rows": count, "accepted_checked_rows": checked,
            "rows": rows,
        }
    return snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=ROOT / "benchmarks/baselines/controller-foundations-v1/results.json")
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    snapshot = collect()
    if args.verify:
        assert json.loads(args.output.read_text()) == snapshot
        print("Verified frozen controller baseline: 272 rows, 261 checked; source hashes match.")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x") as target:
            json.dump(snapshot, target, indent=2, sort_keys=True)
            target.write("\n")
        print(args.output)


if __name__ == "__main__":
    main()
