#!/usr/bin/env python3
"""Verify the CG v2 representation, evidence bindings, and optional inputs."""

import argparse
import csv
import hashlib
import json
from collections import Counter
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
    assert manifest["schema"] == "tactix-cg-v2" and manifest["name"] == "CG"
    assert manifest["count"] == 23 and manifest["soft_budget"] == 25
    assert len(entries) == 23 and len({e["tier_d_id"] for e in entries}) == 23
    assert len({e["instance"] for e in entries}) == 23
    assert len({e["family"] for e in entries}) == 16
    assert max(Counter(e["family"] for e in entries).values()) <= 3
    assert Counter(e["verdict"] for e in entries) == {"SAT": 18, "UNSAT": 5}
    assert all(e["cms_seconds"] <= 600 and e["generated_artifacts_deleted"] for e in entries)

    with (HERE / "unsat-selection.tsv").open(newline="") as stream:
        rows = list(csv.DictReader(
            (line for line in stream if line.strip() and not line.startswith("#")),
            delimiter="\t"))
    unsat = {entry["instance"]: entry for entry in entries
             if entry["verdict"] == "UNSAT"}
    assert len(rows) == len(unsat) == 5
    assert {row["instance"] for row in rows} == set(unsat)
    assert [int(row["coverage_rank"]) for row in rows] == list(range(1, 6))
    assert all(row["verdict"] == "UNSAT" and
               int(row["original_tierd_rank"]) ==
               unsat[row["instance"]]["tier_d_id"] + 1
               for row in rows)

    with (HERE / "mdp-unsat-selection.tsv").open(newline="") as stream:
        mdp_rows = list(csv.DictReader(
            (line for line in stream if line.strip() and not line.startswith("#")),
            delimiter="\t"))
    mdp = {instance: entry for instance, entry in unsat.items()
           if entry["family"] == "mdp"}
    assert len(mdp_rows) == len(mdp) == 3
    assert {row["instance"] for row in mdp_rows} == set(mdp)
    assert [int(row["coverage_rank"]) for row in mdp_rows] == [1, 2, 3]
    assert all(row["verdict"] == "UNSAT" and
               int(row["original_tierd_rank"]) ==
               mdp[row["instance"]]["tier_d_id"] + 1
               for row in mdp_rows)

    pins = manifest["pins"]
    for group in ("proof_free_campaign_manifests", "unsat_verification_campaign_manifests"):
        for campaign, expected in pins[group].items():
            path = ROOT / "benchmarks/runs" / campaign / "manifest.json"
            assert digest(path) == expected, path
    for key in ("saved_seven_arm_comparison", "cache_manifest"):
        pin = pins[key]
        assert digest(ROOT / pin["path"]) == pin["sha256"], pin["path"]

    proof_free = pins["proof_free_campaign_manifests"]
    screening_name = "20261018-cms-tierd-proof-free-4core-5000s"
    repair_name = "20261018-cms-tierd-proof-free-4core-5000s-wall-repair"
    assert set(proof_free) == {screening_name, repair_name}
    screening = ROOT / "benchmarks/runs" / screening_name
    repair = ROOT / "benchmarks/runs" / repair_name
    screening_manifest = json.loads((screening / "manifest.json").read_text())
    repair_manifest = json.loads((repair / "manifest.json").read_text())
    binding = repair_manifest["repair_of"]
    reused = set(binding["reused_source_tasks"])
    rerun = set(binding["rerun_source_tasks"])
    assert reused & rerun == set() and reused | rerun == set(range(1166))
    paths = {task: screening / "results" / f"{task:04d}.json" for task in reused}
    for local_task, row in enumerate(repair_manifest["instances"]):
        paths[row["source_task"]] = repair / "results" / f"{local_task:04d}.json"
    assert set(paths) == set(range(1166)) and all(path.is_file() for path in paths.values())
    combined = hashlib.sha256()
    for source_task, path in sorted(paths.items()):
        combined.update(f"{source_task} {path.relative_to(ROOT)} {digest(path)}\n".encode())
    assert combined.hexdigest() == pins["proof_free_combined_inventory_sha256"]

    for entry in entries:
        result_path = ROOT / entry["cms_result"]
        assert digest(result_path) == entry["cms_result_sha256"], result_path
        result = json.loads(result_path.read_text())
        assert result["schema"] == "cms-proof-free-tierd-result-v1"
        assert result.get("source_task", result["task"]) == entry["tier_d_id"]
        assert result["instance"] == entry["instance"]
        assert result["verdict"] == entry["verdict"]
        assert result["proof_production"] is False and result["proof_path"] is None
        assert result["scratch_deleted"] is True and result["cnf_deleted"] is True
        assert not result["cleanup_pending"] and not result.get("error")

        verification_path = ROOT / entry["verification_result"]
        assert digest(verification_path) == entry["verification_result_sha256"]
        verification = json.loads(verification_path.read_text())
        assert verification["instance"] == entry["instance"]
        assert verification["verdict"] == entry["verdict"]
        if entry["verdict"] == "SAT":
            assert verification_path == result_path
            assert result["valid"] is True and result["expected_match"] is True
            check = result["model_check"]
            assert check["verified"] is True and check["exit_code"] == 0
            assert not check["timed_out"] and entry["certificate_sha256"] is None
        else:
            assert result["valid"] is False
            assert result["verification"] == "unchecked-proof-free"
            assert verification["valid"] is True and verification["proof_ok"] is True
            assert verification["certificate_sha256"] == entry["certificate_sha256"]
            check = verification["proof_check"]
            assert check["result"] == "verified"
            assert [stage["name"] for stage in check["stages"]] == ["frat-xor", "cake-xlrup"]
            assert all(stage["result"] == "verified" and not stage["timed_out"]
                       for stage in check["stages"])
            assert verification["scratch_deleted"] is True
            assert verification["artifact_deleted"] is True
            assert verification["derived_artifacts_deleted"] is True
        if args.hash_inputs:
            input_path = ROOT / entry["compressed_input"]
            assert digest(input_path) == entry["compressed_input_sha256"], input_path

    with (HERE / "baseline-runtimes.csv").open(newline="") as stream:
        runtimes = list(csv.DictReader(stream))
    assert len(runtimes) == 23 * 8
    by_id = {}
    for row in runtimes:
        by_id.setdefault(int(row["tier_d_id"]), set()).add(row["solver"])
    expected_solvers = {"cryptominisat-5.14.7", "tactix-0f7c10174", "tactix-no-mab",
                        "kissat-4.0.4", "kissat-sc2024", "ae-kissat-mab",
                        "satsuma-kissat", "lymphosat"}
    assert set(by_id) == {e["tier_d_id"] for e in entries}
    assert all(solvers == expected_solvers for solvers in by_id.values())
    print(f"CG verified: 23 instances, 16 families, 184 runtime rows, hash_inputs={args.hash_inputs}")


if __name__ == "__main__":
    main()
