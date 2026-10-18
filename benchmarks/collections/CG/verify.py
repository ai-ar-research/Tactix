#!/usr/bin/env python3
"""Verify the frozen CG manifest, checked result evidence, and optional inputs."""

import argparse
import csv
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--hash-inputs", action="store_true")
    args = parser.parse_args()
    manifest = json.loads((HERE / "manifest.json").read_text())
    entries = manifest["entries"]
    assert manifest["schema"] == "tactix-cg-v1"
    assert manifest["name"] == "CG" and manifest["count"] == 25
    assert len(entries) == 25 and len({e["tier_d_id"] for e in entries}) == 25
    assert len({e["instance"] for e in entries}) == 25
    assert all(e["cms_seconds"] <= 600 and e["generated_artifacts_deleted"] for e in entries)

    for campaign, expected in manifest["pins"]["campaign_manifests"].items():
        path = ROOT / "benchmarks/runs" / campaign / "manifest.json"
        assert digest(path) == expected, path
    for key in ("saved_seven_arm_comparison", "cache_manifest"):
        pin = manifest["pins"][key]
        assert digest(ROOT / pin["path"]) == pin["sha256"], pin["path"]

    for entry in entries:
        result_path = ROOT / entry["cms_result"]
        assert digest(result_path) == entry["cms_result_sha256"], result_path
        result = json.loads(result_path.read_text())
        assert result["task"] == entry["tier_d_id"]
        assert result["instance"] == entry["instance"]
        assert result["verdict"] == entry["verdict"] and result["valid"] is True
        assert result["certificate_sha256"] == entry["certificate_sha256"]
        assert result["scratch_deleted"] is True and result["artifact_deleted"] is True
        assert result["derived_artifacts_deleted"] is True
        if result["verdict"] == "SAT":
            assert result["model_ok"] is True and result["model_check"]["result"] == "verified"
        else:
            assert result["proof_ok"] is True and result["proof_check"]["result"] == "verified"
            assert [s["name"] for s in result["proof_check"]["stages"]] == [
                "frat-xor", "cake-xlrup"]
        if args.hash_inputs:
            input_path = ROOT / entry["compressed_input"]
            assert digest(input_path) == entry["compressed_input_sha256"], input_path

    with (HERE / "baseline-runtimes.csv").open(newline="") as stream:
        runtimes = list(csv.DictReader(stream))
    assert len(runtimes) == 25 * 8
    by_id = {}
    for row in runtimes:
        by_id.setdefault(int(row["tier_d_id"]), set()).add(row["solver"])
    expected_solvers = {"cryptominisat-5.14.7", "tactix-0f7c10174", "tactix-no-mab",
                        "kissat-4.0.4", "kissat-sc2024", "ae-kissat-mab",
                        "satsuma-kissat", "lymphosat"}
    assert set(by_id) == {e["tier_d_id"] for e in entries}
    assert all(solvers == expected_solvers for solvers in by_id.values())
    print(f"CG verified: 25 instances, 200 runtime rows, hash_inputs={args.hash_inputs}")


if __name__ == "__main__":
    main()
