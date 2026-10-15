#!/usr/bin/env python3
"""Pair the C-routed default strategy (tactix-public) with the C++ build.

Reads the same-window regression gates and the paired Tier A run and writes
`interface-overhead.csv` (instances both arms solved) and `summary.json`.
Run from the repository root.
"""
import csv
import glob
import json
import statistics
from pathlib import Path

RUNS = Path("benchmarks/runs")
OUT = Path(__file__).resolve().parent
GATES = {"B28+C5": "b28c5", "Rank 1-50": "rank1-50", "WV100": "wv100", "WV2-89": "wv2-89"}
C_API = "20261015-public-port-v3-regression-{}"
CPP = "20261015-prod-samewindow-regression-{}"
TIER_A = "20261015-120822-A-tactix-public"


def instructions(row):
    for line in (row.get("runtime_context") or {}).get("perf_statistics", []):
        fields = line.split("\t")
        if len(fields) > 2 and fields[2] == "instructions":
            return int(fields[0])
    return None


def solved(row):
    # An answer returned within the limit; proof checking is reported separately.
    return row["verdict"] in ("SAT", "UNSAT") and not row["solve"].get("timed_out")


def answered(row):
    # run.py's `solved` also requires an accepted proof; the plot counts answers.
    return row["verdict"] in ("SAT", "UNSAT")


def load(directory):
    rows = {}
    for path in glob.glob(str(RUNS / directory / "results" / "*.json")):
        row = json.loads(Path(path).read_text())
        rows[row["instance"]] = row
    return rows


def gate(name, key):
    api, cpp = load(C_API.format(key)), load(CPP.format(key))
    pairs = []
    identical = differing = 0
    for instance in sorted(set(api) & set(cpp)):
        a, c = api[instance], cpp[instance]
        same = a["verdict"] == c["verdict"] and (
            a["verdict"] != "UNSAT" or a.get("certificate_sha256") == c.get("certificate_sha256"))
        identical += same
        differing += not same
        if solved(a) and solved(c):
            pairs.append(dict(set=name, instance=instance.split("/")[-1], verdict=a["verdict"],
                              cpp_seconds=c["solve"]["wall_seconds"],
                              c_api_seconds=a["solve"]["wall_seconds"],
                              cpp_instructions=instructions(c), c_api_instructions=instructions(a)))
    return pairs, dict(rows=len(set(api) & set(cpp)), identical=identical, differing=differing,
                       solved_c_api=sum(solved(r) for r in api.values()),
                       solved_cpp=sum(solved(r) for r in cpp.values()))


def tier_a():
    run = RUNS / TIER_A
    manifest = json.loads((run / "manifest.json").read_text())
    names = [solver["name"] for solver in manifest["solvers"]]
    by_arm = {name: {} for name in names}
    for index, task in enumerate(manifest["tasks"]):
        path = run / "results" / f"{index:05d}.json"
        if path.exists():
            by_arm[names[task["solver"]]][task["rel"]] = json.loads(path.read_text())
    api, cpp = by_arm["tactix-public"], by_arm["tactix-prod"]
    pairs = []
    identical = differing = 0
    for instance in sorted(set(api) & set(cpp)):
        a, c = api[instance], cpp[instance]
        # tactix-public prints no per-rule `reached` block; compare the answer
        # and the certificate size (Tier A records no certificate hash).
        same = a["verdict"] == c["verdict"] and a.get("proof_bytes") == c.get("proof_bytes")
        identical += same
        differing += not same
        if answered(a) and answered(c):
            pairs.append(dict(set="Tier A", instance=instance.split("/")[-1], verdict=a["verdict"],
                              cpp_seconds=c["time"], c_api_seconds=a["time"],
                              cpp_instructions=None, c_api_instructions=None))
    return pairs, dict(rows=len(set(api) & set(cpp)), identical=identical, differing=differing,
                       solved_c_api=sum(answered(r) for r in api.values()),
                       solved_cpp=sum(answered(r) for r in cpp.values()),
                       checked_c_api=sum(r["solved"] for r in api.values()),
                       checked_cpp=sum(r["solved"] for r in cpp.values()))


def summarize(pairs, counts):
    ratios = [p["c_api_seconds"] / p["cpp_seconds"] for p in pairs if p["cpp_seconds"] > 0]
    cpp_total = sum(p["cpp_seconds"] for p in pairs)
    api_total = sum(p["c_api_seconds"] for p in pairs)
    with_instructions = [p for p in pairs if p["cpp_instructions"] and p["c_api_instructions"]]
    instruction_ratio = (sum(p["c_api_instructions"] for p in with_instructions)
                         / sum(p["cpp_instructions"] for p in with_instructions)
                         if with_instructions else None)
    return dict(counts, paired_solved=len(pairs),
                median_time_ratio=statistics.median(ratios) if ratios else None,
                aggregate_time_ratio=api_total / cpp_total if cpp_total else None,
                cpp_seconds=cpp_total, c_api_seconds=api_total,
                aggregate_instruction_ratio=instruction_ratio,
                max_instruction_ratio=max((p["c_api_instructions"] / p["cpp_instructions"]
                                           for p in with_instructions), default=None))


def main():
    rows, summary = [], {}
    pairs, counts = tier_a()
    rows += pairs
    summary["Tier A"] = summarize(pairs, counts)
    for name, key in GATES.items():
        pairs, counts = gate(name, key)
        rows += pairs
        summary[name] = summarize(pairs, counts)
    with (OUT / "interface-overhead.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
