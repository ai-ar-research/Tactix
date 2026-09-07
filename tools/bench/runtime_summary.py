#!/usr/bin/env python3
"""Compare checked runtime records without inferring work from timing bands."""
import argparse
import csv
import json
from pathlib import Path
import re
import sys


def counters(row):
    text = "\n".join(row.get("solver_statistics", []))
    result = {}
    for name in ("conflicts", "decisions", "propagations"):
        match = re.search(r"^c\s+" + name + r":?\s+(\d+)\b", text, re.M)
        result[name] = int(match[1]) if match else None
    return result


def work_class(tactix, kissat):
    if tactix["conflicts"] == kissat["conflicts"] == 0:
        return "zero-conflict"
    if any(value is None for row in (tactix, kissat) for value in row.values()):
        return "counters-missing"
    return "equal-counters" if tactix == kissat else "different-counters"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("campaign", type=Path)
    args = ap.parse_args()
    rows = {}
    final = []
    final_keys = set()
    for kind in ("provisional", "results"):
        for path in sorted((args.campaign / kind).glob("*.json")):
            row = json.loads(path.read_text())
            rows[row["rank"], row["arm"]] = row
            if kind == "results":
                final.append(row)
                final_keys.add((row["rank"], row["arm"]))
    writer = csv.writer(sys.stdout, delimiter="\t")
    writer.writerow(("rank", "tactix_s", "kissat_s", "ratio", "delta_s", "conflict_ratio",
                     "propagation_ratio", "tactix_rss_kib", "kissat_rss_kib", "work", "validation"))
    totals = [0., 0.]
    groups = {}
    pairs = 0
    def ratio(a, b):
        return f"{a / b:.4f}" if a is not None and b else "NA"
    for rank in range(1, 41):
        t, k = rows.get((rank, "tactix")), rows.get((rank, "kissat"))
        if not t or not k or "solve" not in t or "solve" not in k:
            continue
        if any((rank, r["arm"]) in final_keys and
               not (r.get("valid") and r.get("artifact_deleted"))
               for r in (t, k)):
            print(f"# rank {rank}: invalid final validation; excluded", file=sys.stderr)
            continue
        if any(not r.get("expected_match") or r["solve"]["timed_out"] or
               r["solve"]["exit_code"] != (10 if r["expected"] == "SAT" else 20)
               for r in (t, k)):
            print(f"# rank {rank}: invalid solver result; excluded", file=sys.stderr)
            continue
        ts, ks = t["solve"]["wall_seconds"], k["solve"]["wall_seconds"]
        tc, kc = counters(t), counters(k)
        group = work_class(tc, kc)
        validation = "checked" if all(r.get("valid") and r.get("artifact_deleted") for r in (t, k)) else "provisional"
        writer.writerow((rank, ts, ks, ratio(ts, ks), round(ts - ks, 3),
                         ratio(tc["conflicts"], kc["conflicts"]),
                         ratio(tc["propagations"], kc["propagations"]),
                         t["solve"]["peak_rss_kb"], k["solve"]["peak_rss_kb"], group, validation))
        totals[0] += ts
        totals[1] += ks
        pairs += 1
        subtotal = groups.setdefault(group, [0, 0., 0.])
        subtotal[0] += 1
        subtotal[1] += ts
        subtotal[2] += ks
    print(f"# {pairs}/40 paired solver results; total {totals[0]:.3f}/{totals[1]:.3f} = {ratio(*totals)}")
    valid = sum(r.get("valid", False) and r.get("artifact_deleted", False) for r in final)
    print(f"# final checked/clean {valid}/80; final invalid {len(final) - valid}; pending {80 - len(final)}")
    for group, (count, ts, ks) in groups.items():
        print(f"# {group}: {count} ranks; {ts:.3f}/{ks:.3f} = {ratio(ts, ks)}; delta {ts - ks:+.3f}s")
    print("# Counter equality is a diagnostic screen, not proof of equal trajectory or causal cost attribution.")


if __name__ == "__main__":
    main()
