#!/usr/bin/env python3
"""Compare the pinned Tactix Tier-D pass with frozen Kissat 4.0.4."""

import argparse
import csv
import json
import math
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "benchmarks/baselines/sc24-26-32g-5000s-v1"
TACTIX = ROOT / "benchmarks/runs/20260813-tierd-tactix-sc24-26-32g-5000s"
OUTPUT = ROOT / "benchmarks/reports/2026-08-13-tier-d-tactix-vs-kissat"
TIMEOUT = 5000.0


def load_results(directory, solver=None):
    rows = {}
    for path in sorted((directory / "results").glob("*.json")):
        row = json.loads(path.read_text())
        if solver is not None and row.get("solver") != solver:
            continue
        rows[row["instance"]] = row
    return rows


def accepted(row):
    return row is not None and row.get("valid") is True


def solve_time(row):
    return float(row["solve"]["wall_seconds"])


def peak_gib(row, field="solve"):
    value = (row.get(field) or {}).get("peak_rss_kb")
    return None if value is None else value / 1024 / 1024


def check_time(row):
    proof = row.get("proof_check") or {}
    model = row.get("model_check") or {}
    return proof.get("wall_seconds", model.get("wall_seconds"))


def median(values):
    values = [value for value in values if value is not None]
    return statistics.median(values) if values else None


def geometric_mean(values):
    values = [value for value in values if value > 0]
    return math.exp(sum(math.log(value) for value in values) / len(values)) if values else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=BASELINE)
    parser.add_argument("--tactix", type=Path, default=TACTIX)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()

    baseline_manifest = json.loads((args.baseline / "manifest.json").read_text())
    tactix_manifest = json.loads((args.tactix / "manifest.json").read_text())
    if tactix_manifest["baseline_manifest_sha256"] != __import__("hashlib").sha256(
            (args.baseline / "manifest.json").read_bytes()).hexdigest():
        raise SystemExit("Tactix campaign does not reference this baseline manifest")
    if baseline_manifest["dataset_sha256"] != tactix_manifest["dataset_sha256"]:
        raise SystemExit("dataset hash mismatch")

    memberships = {}
    with open(args.baseline / "year-membership.tsv", newline="") as source:
        for row in csv.DictReader(source, delimiter="\t"):
            memberships[row["hash"]] = [year for year in ("2024", "2025", "2026")
                                       if row[f"SC{year}"] == "1"]
    baseline = load_results(args.baseline, "kissat-4.0.4")
    tactix = load_results(args.tactix)
    universe = [row["rel"] for row in baseline_manifest["instances"]]

    paired = []
    for instance in universe:
        kissat_row, tactix_row = baseline.get(instance), tactix.get(instance)
        kissat_ok, tactix_ok = accepted(kissat_row), accepted(tactix_row)
        if kissat_row is None or tactix_row is None:
            category = "incomplete"
        elif kissat_ok and tactix_ok:
            category = "shared-solve"
        elif kissat_ok:
            category = "coverage-loss"
        elif tactix_ok:
            category = "coverage-win"
        else:
            category = "shared-unsolved"
        ratio = (solve_time(tactix_row) / solve_time(kissat_row)
                 if kissat_ok and tactix_ok and solve_time(kissat_row) > 0 else None)
        paired.append({
            "instance": instance,
            "expected": next(row["expected"] for row in baseline_manifest["instances"]
                             if row["rel"] == instance),
            "years": ",".join(memberships.get(Path(instance).name.split("-", 1)[0], [])),
            "category": category,
            "kissat_valid": kissat_ok,
            "tactix_valid": tactix_ok,
            "kissat_verdict": None if kissat_row is None else kissat_row.get("verdict"),
            "tactix_verdict": None if tactix_row is None else tactix_row.get("verdict"),
            "kissat_solve_s": None if kissat_row is None else solve_time(kissat_row),
            "tactix_solve_s": None if tactix_row is None else solve_time(tactix_row),
            "time_ratio": ratio,
            "kissat_solve_peak_gib": None if kissat_row is None else peak_gib(kissat_row),
            "tactix_solve_peak_gib": None if tactix_row is None else peak_gib(tactix_row),
            "kissat_check_s": None if kissat_row is None else check_time(kissat_row),
            "tactix_check_s": None if tactix_row is None else check_time(tactix_row),
        })

    complete_pairs = [row for row in paired if row["category"] != "incomplete"]
    shared = [row for row in paired if row["category"] == "shared-solve"]
    summary = {
        "schema": 1,
        "complete": len(complete_pairs) == len(universe),
        "universe": len(universe),
        "paired_results": len(complete_pairs),
        "kissat_results": len(baseline),
        "tactix_results": len(tactix),
        "categories": {name: sum(row["category"] == name for row in paired)
                       for name in ("coverage-loss", "coverage-win", "shared-solve",
                                    "shared-unsolved", "incomplete")},
        "shared_solve_geomean_ratio": geometric_mean(
            [row["time_ratio"] for row in shared]),
        "shared_solve_median_ratio": median([row["time_ratio"] for row in shared]),
        "par2": {
            "kissat": sum(solve_time(row) if accepted(row) else 2 * TIMEOUT
                          for row in baseline.values()) if len(baseline) == len(universe) else None,
            "tactix": sum(solve_time(row) if accepted(row) else 2 * TIMEOUT
                          for row in tactix.values()) if len(tactix) == len(universe) else None,
        },
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    fields = list(paired[0])
    with open(args.output / "paired.tsv", "w", newline="") as target:
        writer = csv.DictWriter(target, fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(paired)

    losses = sorted((row for row in paired if row["category"] == "coverage-loss"),
                    key=lambda row: row["kissat_solve_s"] or math.inf)
    slow = sorted((row for row in shared if row["time_ratio"] is not None),
                  key=lambda row: row["time_ratio"], reverse=True)
    report = ["# Tier D: Tactix versus Kissat 4.0.4", "",
              f"Status: {'complete' if summary['complete'] else 'running'}; "
              f"{len(complete_pairs)}/{len(universe)} paired results.", "",
              "## Current paired picture", "",
              "```json", json.dumps(summary, indent=2), "```", "",
              "## Coverage losses (Kissat-valid, Tactix-unsolved)", ""]
    report += [f"- `{row['instance']}`: {row['expected']}, Kissat "
               f"{row['kissat_solve_s']:.3f}s, years {row['years']}"
               for row in losses[:40]] or ["None in completed pairs."]
    report += ["", "## Largest shared-solve slowdowns", ""]
    report += [f"- `{row['instance']}`: {row['time_ratio']:.2f}x "
               f"({row['tactix_solve_s']:.3f}s / {row['kissat_solve_s']:.3f}s), "
               f"{row['expected']}, years {row['years']}" for row in slow[:40]] \
              or ["No shared solves yet."]
    report += [""]
    (args.output / "report.md").write_text("\n".join(report))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
