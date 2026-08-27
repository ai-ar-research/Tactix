#!/usr/bin/env python3
"""Validate and summarize the matched Congruence-admission regression."""

import csv
import json
from pathlib import Path
import sys


CAMPAIGN = Path(__file__).resolve().parent
TIMEOUT = 1200.0


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    with open(temporary, "x") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")
    temporary.replace(path)


def main():
    with open(CAMPAIGN / "matrix.tsv", newline="") as source:
        matrix = list(csv.DictReader(source, delimiter="\t"))
    results = {}
    malformed = []
    for path in sorted((CAMPAIGN / "results").glob("*.json")):
        try:
            row = json.loads(path.read_text())
            task = int(row["task"])
            if task in results:
                malformed.append(f"duplicate task {task}")
            results[task] = row
        except Exception as error:
            malformed.append(f"{path.name}: {error!r}")

    missing = [int(row["task"]) for row in matrix
               if int(row["task"]) not in results]
    cleanup_bad = [task for task, row in results.items()
                   if row.get("artifact_deleted") is not True or
                   row.get("derived_artifacts_deleted") is not True or
                   row.get("cleanup_pending")]
    residue = [str(path.relative_to(CAMPAIGN))
               for path in CAMPAIGN.rglob("*") if path.is_file() and
               path.suffix.lower() in {".drat", ".lrat", ".proof", ".dpr",
                                       ".model", ".cnf", ".xz"}]

    by_key = {(row["instance"], row["arm"]): results[int(row["task"])]
              for row in matrix if int(row["task"]) in results}
    instances = sorted({row["instance"] for row in matrix})
    tiers = sorted({row["tier"] for row in matrix})
    arms = {}
    for arm in ("shipped", "candidate"):
        rows = [by_key[(instance, arm)] for instance in instances
                if (instance, arm) in by_key]
        valid = [row for row in rows if row.get("valid") is True]
        arms[arm] = {
            "results": len(rows),
            "valid": len(valid),
            "sat": sum(row.get("verdict") == "SAT" for row in valid),
            "unsat": sum(row.get("verdict") == "UNSAT" for row in valid),
            "solve_seconds_total": round(sum(
                float(row["solve"]["wall_seconds"]) for row in valid), 3),
            "par2": round(sum(
                float(row["solve"]["wall_seconds"])
                if row.get("valid") is True else 2 * TIMEOUT
                for row in rows), 3),
            "peak_rss_gib_max": max((
                row["solve"]["peak_rss_kb"] / 1024 / 1024 for row in rows
                if (row.get("solve") or {}).get("peak_rss_kb") is not None),
                default=None),
        }

    regressions = []
    gains = []
    for instance in instances:
        shipped = by_key.get((instance, "shipped"))
        candidate = by_key.get((instance, "candidate"))
        if not shipped or not candidate:
            continue
        item = {
            "instance": instance,
            "tier": candidate["tier"],
            "expected": candidate["expected"],
            "shipped_verdict": shipped.get("verdict"),
            "candidate_verdict": candidate.get("verdict"),
            "shipped_seconds": (shipped.get("solve") or {}).get("wall_seconds"),
            "candidate_seconds": (candidate.get("solve") or {}).get("wall_seconds"),
        }
        if shipped.get("valid") is True and candidate.get("valid") is not True:
            regressions.append(item)
        if shipped.get("valid") is not True and candidate.get("valid") is True:
            gains.append(item)

    invalid = [{"task": task, "instance": row.get("instance"),
                "arm": row.get("arm"), "verdict": row.get("verdict"),
                "error": row.get("error")}
               for task, row in sorted(results.items())
               if row.get("valid") is not True]
    complete = (len(results) == len(matrix) and not missing and not malformed)
    candidate_par2_ratio = (
        arms["candidate"]["par2"] / arms["shipped"]["par2"]
        if arms["shipped"]["par2"] else None
    )
    passed = (complete and not cleanup_bad and not residue and
              not regressions and not invalid and
              candidate_par2_ratio is not None and
              candidate_par2_ratio <= 1.10)
    summary = {
        "schema": "tactix-matched-debug-regression-summary-v1",
        "complete": complete,
        "passed": passed,
        "task_count": len(matrix),
        "result_count": len(results),
        "instance_count": len(instances),
        "tiers": tiers,
        "resource_contract": {
            "solver_timeout_seconds": 1200,
            "scheduler_memory": "48000M",
            "memory_enforcement": "slurm_task_cgroup",
            "address_space_limit": None,
            "cpus_per_task": 12,
            "physical_cores_per_task": 6,
            "nodes": sorted({row.get("node") for row in results.values()
                             if row.get("node")}),
            "array_throttle": None,
        },
        "arms": arms,
        "candidate_par2_ratio": candidate_par2_ratio,
        "candidate_par2_limit": 1.10,
        "regressions": regressions,
        "gains": gains,
        "invalid": invalid,
        "missing_tasks": missing,
        "malformed": malformed,
        "cleanup_bad_tasks": cleanup_bad,
        "certificate_residue": residue,
    }
    atomic_json(CAMPAIGN / "summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
