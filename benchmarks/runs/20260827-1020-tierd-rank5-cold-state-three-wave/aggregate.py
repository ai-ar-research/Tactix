#!/usr/bin/env python3
"""Classify three independent paired Rank-5 memory/timing waves."""

import csv
import json
from pathlib import Path
from statistics import median
import sys


CAMPAIGN = Path(__file__).resolve().parent


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    with open(temporary, "x") as target:
        json.dump(value, target, indent=2, sort_keys=True)
        target.write("\n")
    temporary.replace(path)


def spread(values):
    center = median(values)
    return (max(values) - min(values)) / center if center else None


def main():
    with open(CAMPAIGN / "matrix.tsv", newline="") as source:
        matrix = list(csv.DictReader(source, delimiter="\t"))
    results = {}
    malformed = []
    for path in sorted((CAMPAIGN / "results").glob("*.json")):
        try:
            result = json.loads(path.read_text())
            task = int(result["task"])
            if task in results:
                malformed.append(f"duplicate task {task}")
            results[task] = result
        except Exception as error:
            malformed.append(f"{path.name}: {error!r}")
    missing = [int(row["task"]) for row in matrix
               if int(row["task"]) not in results]
    invalid = [task for task, result in sorted(results.items())
               if result.get("valid") is not True]
    cleanup_bad = [task for task, result in sorted(results.items())
                   if result.get("artifact_deleted") is not True or
                   result.get("derived_artifacts_deleted") is not True or
                   result.get("cleanup_pending")]
    residue = [str(path.relative_to(CAMPAIGN))
               for path in CAMPAIGN.rglob("*") if path.is_file() and
               path.suffix.lower() in {".drat", ".lrat", ".proof", ".dpr",
                                       ".model", ".cnf", ".xz"}]

    waves = []
    by_arm = {"tactix": [], "kissat": []}
    memory_by_arm = {"tactix": [], "kissat": []}
    for wave in (1, 2, 3):
        item = {"wave": wave}
        for arm in ("tactix", "kissat"):
            row = next(row for row in matrix
                       if int(row["wave"]) == wave and row["arm"] == arm)
            result = results.get(int(row["task"]))
            seconds = ((result or {}).get("solve") or {}).get("wall_seconds")
            peak_rss_kb = ((result or {}).get("solve") or {}).get("peak_rss_kb")
            item[f"{arm}_seconds"] = seconds
            item[f"{arm}_peak_rss_kb"] = peak_rss_kb
            item[f"{arm}_node"] = (result or {}).get("node")
            item[f"{arm}_valid"] = (result or {}).get("valid") is True
            if seconds is not None and item[f"{arm}_valid"]:
                by_arm[arm].append(float(seconds))
            if peak_rss_kb is not None and item[f"{arm}_valid"]:
                memory_by_arm[arm].append(int(peak_rss_kb))
        if item.get("tactix_seconds") is not None and item.get("kissat_seconds"):
            item["paired_ratio"] = (item["tactix_seconds"] /
                                    item["kissat_seconds"])
        if (item.get("tactix_peak_rss_kb") is not None and
                item.get("kissat_peak_rss_kb")):
            item["paired_rss_ratio"] = (item["tactix_peak_rss_kb"] /
                                        item["kissat_peak_rss_kb"])
        waves.append(item)

    complete = (len(results) == len(matrix) and not missing and not malformed)
    passed = complete and not invalid and not cleanup_bad and not residue
    ratios = [wave["paired_ratio"] for wave in waves
              if "paired_ratio" in wave]
    rss_ratios = [wave["paired_rss_ratio"] for wave in waves
                  if "paired_rss_ratio" in wave]
    arm_spread = {arm: spread(values) if len(values) == 3 else None
                  for arm, values in by_arm.items()}
    timing_stable = (passed and len(ratios) == 3 and
                     all(value is not None and value <= 0.02
                         for value in arm_spread.values()))
    summary = {
        "schema": "tactix-rank5-cold-state-three-wave-summary-v1",
        "complete": complete,
        "passed": passed,
        "timing_decision_eligible": timing_stable,
        "memory_decision_eligible": (passed and len(rss_ratios) == 3 and
                                     all(len(memory_by_arm[arm]) == 3
                                         for arm in memory_by_arm)),
        "result_count": len(results),
        "missing_tasks": missing,
        "malformed": malformed,
        "invalid_tasks": invalid,
        "cleanup_bad_tasks": cleanup_bad,
        "certificate_residue": residue,
        "waves": waves,
        "tactix_seconds": by_arm["tactix"],
        "kissat_seconds": by_arm["kissat"],
        "tactix_peak_rss_kb": memory_by_arm["tactix"],
        "kissat_peak_rss_kb": memory_by_arm["kissat"],
        "tactix_median_seconds": median(by_arm["tactix"]) if by_arm["tactix"] else None,
        "kissat_median_seconds": median(by_arm["kissat"]) if by_arm["kissat"] else None,
        "median_paired_ratio": median(ratios) if ratios else None,
        "tactix_median_peak_rss_kb": median(memory_by_arm["tactix"]) if memory_by_arm["tactix"] else None,
        "kissat_median_peak_rss_kb": median(memory_by_arm["kissat"]) if memory_by_arm["kissat"] else None,
        "median_paired_rss_ratio": median(rss_ratios) if rss_ratios else None,
        "paired_rss_ratio_range": [min(rss_ratios), max(rss_ratios)] if rss_ratios else None,
        "paired_ratio_range": [min(ratios), max(ratios)] if ratios else None,
        "within_arm_spread": arm_spread,
        "stability_limit": 0.02,
        "resource_contract": {
            "cpus_per_task": 12,
            "physical_cores_per_task": 6,
            "scheduler_memory": "48000M",
            "memory_enforcement": "slurm_task_cgroup",
            "address_space_limit": None,
            "node_constraint": None,
            "one_solver_run_per_task": True,
            "proof_mode": "DRAT with independent drat-trim verification",
        },
    }
    atomic_json(CAMPAIGN / "summary.json", summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
