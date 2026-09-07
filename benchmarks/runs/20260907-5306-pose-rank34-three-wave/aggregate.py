#!/usr/bin/env python3
"""Report all three predeclared pairs, never a best-of timing."""
import json
from pathlib import Path
import re
import statistics

C = Path(__file__).resolve().parent
manifest = json.loads((C / "manifest.json").read_text())
rows = [json.loads((C / "results" / f"{i:03d}.json").read_text()) for i in range(6)]
for i, row in enumerate(rows):
    assert row["task"] == i and row["arm"] == ("candidate" if i % 2 else "shipped")
    assert row["valid"] and row["artifact_deleted"] and row["solve"]["exit_code"] == 10
    assert row["binary_sha256"] == manifest["arms"][row["arm"]]["sha256"]
    assert row["model_check"]["result"] == "verified"
    topology = row["runtime_context"]["topology"]
    assert len(topology) == 12
    assert len({entry["l3_shared_cpus"] for entry in topology}) == 1

ratios = [rows[i + 1]["solve"]["wall_seconds"] / rows[i]["solve"]["wall_seconds"]
          for i in range(0, 6, 2)]
arms = {}
for name in ("shipped", "candidate"):
    selected = [r for r in rows if r["arm"] == name]
    times = [r["solve"]["wall_seconds"] for r in selected]
    statistics_text = "\n".join(selected[0]["solver_statistics"])
    counters = {}
    for key in ("conflicts", "decisions", "propagations"):
        match = re.search(r"^c\s+" + key + r":?\s+(\d+)\b", statistics_text, re.M)
        counters[key] = int(match[1]) if match else None
    instructions = []
    for r in selected:
        for line in r["runtime_context"]["perf_statistics"]:
            fields = line.split("\t")
            if len(fields) > 2 and fields[2] == "instructions":
                instructions.append(int(fields[0]))
    arms[name] = dict(seconds=times, median_seconds=statistics.median(times),
                      relative_range=(max(times) - min(times)) / statistics.median(times),
                      median_rss_kib=statistics.median(r["solve"]["peak_rss_kb"] for r in selected),
                      median_instructions=statistics.median(instructions), counters=counters,
                      nodes=[r["node"] for r in selected])
print(json.dumps(dict(valid_checked_clean=6, paired_ratios=ratios,
                     median_paired_ratio=statistics.median(ratios), arms=arms,
                     absolute_timing_stable=all(a["relative_range"] <= .02 for a in arms.values()),
                     limitation="Scheduler grouped arms by node; placement was not forced or crossed. All arms own one L3 domain and report approximately 3.8 GHz."), indent=2))
