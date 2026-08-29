#!/usr/bin/env python3
"""Build a live/final Rank 1--40 comparison without rerunning comparators."""

import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CAMPAIGN = Path(__file__).resolve().parent
PREVIOUS = ROOT / "benchmarks/runs/20260829-1754-tierd-ranks1-40-congruence-import-order"
REFERENCE = ROOT / "benchmarks/runs/20260828-1125-tierd-ranks1-40-walk-eliminate-anchor"
TIMEOUT = 1200.0


def load_ranked(directory, arm=None):
    ranked = {}
    for path in sorted(directory.glob("*.json")):
        result = json.loads(path.read_text())
        if arm is not None and result.get("arm") != arm:
            continue
        ranked[result["rank"]] = result
    return ranked


def seconds(result):
    return result.get("solve", {}).get("wall_seconds") if result else None


def rss(result):
    if not result:
        return None
    solve = result.get("solve", {})
    return solve.get("sampled_process_tree_rss_peak_kb") or solve.get("peak_rss_kb")


def solved(result):
    return bool(result and result.get("expected_match"))


def ratio(numerator, denominator):
    if numerator is None or denominator in (None, 0):
        return None
    return numerator / denominator


def arm_summary(results):
    present = len(results)
    solved_count = sum(solved(result) for result in results.values())
    raw_seconds = sum(seconds(result) or 0.0 for result in results.values())
    par2 = sum((seconds(results.get(rank)) if solved(results.get(rank)) else 2 * TIMEOUT)
               for rank in range(1, 41))
    rss_values = [rss(result) for result in results.values() if rss(result) is not None]
    return {
        "present": present,
        "solved": solved_count,
        "pending": 40 - present,
        "solve_seconds_total_present": round(raw_seconds, 3),
        "par2_40": round(par2, 3),
        "peak_process_tree_rss_kib": max(rss_values) if rss_values else None,
    }


def fmt_seconds(value):
    return "--" if value is None else f"{value:.3f}"


def fmt_rss(value):
    return "--" if value is None else f"{value / 1024:.1f}"


def fmt_ratio(value):
    return "--" if value is None else f"{value:.2f}x"


def main():
    candidate = load_ranked(CAMPAIGN / "provisional")
    previous = load_ranked(PREVIOUS / "provisional")
    kissat = load_ranked(REFERENCE / "results", arm="kissat")
    rows = []
    comparable_previous = []
    comparable_kissat = []
    for rank in range(1, 41):
        current = candidate.get(rank)
        prior = previous.get(rank)
        reference = kissat.get(rank)
        current_seconds = seconds(current)
        prior_seconds = seconds(prior)
        kissat_seconds = seconds(reference)
        current_ok = solved(current)
        previous_ratio = ratio(current_seconds, prior_seconds) if current_ok and solved(prior) else None
        kissat_ratio = ratio(current_seconds, kissat_seconds) if current_ok and solved(reference) else None
        if previous_ratio is not None:
            comparable_previous.append(previous_ratio)
        if kissat_ratio is not None:
            comparable_kissat.append(kissat_ratio)
        rows.append({
            "rank": rank,
            "expected": (current or prior or reference).get("expected"),
            "candidate_verdict": current.get("verdict") if current else "PENDING",
            "candidate_expected_match": current.get("expected_match") if current else None,
            "candidate_seconds": current_seconds,
            "candidate_rss_kib": rss(current),
            "previous_verdict": prior.get("verdict") if prior else None,
            "previous_seconds": prior_seconds,
            "previous_rss_kib": rss(prior),
            "candidate_over_previous": previous_ratio,
            "kissat_verdict": reference.get("verdict") if reference else None,
            "kissat_seconds": kissat_seconds,
            "kissat_rss_kib": rss(reference),
            "candidate_over_kissat": kissat_ratio,
        })
    summary = {
        "schema": "tactix-tierd-ranks1-40-watch-lifecycle-comparison-v1",
        "timeout_seconds": TIMEOUT,
        "candidate": arm_summary(candidate),
        "previous": arm_summary(previous),
        "kissat": arm_summary(kissat),
        "geomean_candidate_over_previous_on_joint_solves": (
            math.prod(comparable_previous) ** (1 / len(comparable_previous))
            if comparable_previous else None),
        "geomean_candidate_over_kissat_on_joint_solves": (
            math.prod(comparable_kissat) ** (1 / len(comparable_kissat))
            if comparable_kissat else None),
        "rows": rows,
    }
    (CAMPAIGN / "comparison-live.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n")

    lines = [
        "# Live Rank 1--40 comparison",
        "",
        "Times are solver wall seconds. RSS is sampled peak process-tree MiB. "
        "Previous Tactix and Kissat values are frozen; no comparator was rerun.",
        "",
        "| Rank | Exp | Candidate | sec | MiB | Previous sec | MiB | Kissat sec | MiB | C/P | C/K |",
        "|---:|:---:|:---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['rank']} | {row['expected']} | {row['candidate_verdict']} | "
            f"{fmt_seconds(row['candidate_seconds'])} | {fmt_rss(row['candidate_rss_kib'])} | "
            f"{fmt_seconds(row['previous_seconds'])} | {fmt_rss(row['previous_rss_kib'])} | "
            f"{fmt_seconds(row['kissat_seconds'])} | {fmt_rss(row['kissat_rss_kib'])} | "
            f"{fmt_ratio(row['candidate_over_previous'])} | "
            f"{fmt_ratio(row['candidate_over_kissat'])} |")
    lines.extend([
        "",
        f"Candidate present/solved: {summary['candidate']['present']}/"
        f"{summary['candidate']['solved']} of 40; live PAR-2 = "
        f"{summary['candidate']['par2_40']:.3f}.",
        f"Previous solved/PAR-2: {summary['previous']['solved']}/40, "
        f"{summary['previous']['par2_40']:.3f}.",
        f"Kissat solved/PAR-2: {summary['kissat']['solved']}/40, "
        f"{summary['kissat']['par2_40']:.3f}.",
    ])
    (CAMPAIGN / "comparison-live.md").write_text("\n".join(lines) + "\n")
    print(json.dumps(summary["candidate"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
