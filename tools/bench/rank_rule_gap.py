#!/usr/bin/env python3
"""Per-rank, per-rule gap: which instance to profile for which rule."""
import glob, json, re, sys, collections
from pathlib import Path
ROOT = Path("/home/hwu/SAT-solving")
KIS = ROOT / "benchmarks/runs/20260829-1930-tierd-ranks1-40-accepted-fresh"
RULE_PHASE = {"Vivify": "vivify", "BVE": "eliminate", "Substitute": "substitute",
              "Congruence": "congruence", "Factor": "factor", "Sweep": "sweep",
              "Backbone": "backbone", "TransitiveReduce": "transitive"}

def tac(campaign, arm):
    out = {}
    for sub in ("results", "provisional"):
        for p in glob.glob(f"{campaign}/{sub}/*.json"):
            r = json.loads(Path(p).read_text())
            if r.get("arm") != arm or r.get("rank") is None:
                continue
            if r["rank"] in out:
                continue
            t = collections.Counter()
            for line in r.get("solver_statistics") or []:
                m = re.match(r"c timing\s+(\S+)\s+(\d+) ms", line)
                if m:
                    t[m.group(1)] += int(m.group(2)) / 1000.0
            if t:
                out[r["rank"]] = t
    return out

def kis():
    out = {}
    for p in glob.glob(str(KIS / "results/*.json")):
        r = json.loads(Path(p).read_text())
        if r.get("arm") != "kissat" or r.get("rank") is None:
            continue
        ph = collections.Counter()
        for line in r.get("solver_statistics") or []:
            m = re.match(r"c\s+([\d.]+)\s+[\d.]+ %\s+(\w+)\s*$", line)
            if m:
                ph[m.group(2)] += float(m.group(1))
        if ph:
            out[r["rank"]] = ph
    return out

T, K = tac(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "cand"), kis()
ranks = sorted(set(T) & set(K))
print("ranks: %d" % len(ranks))
for rule, phase in RULE_PHASE.items():
    rows = sorted(((T[r][rule] - K[r][phase], r, T[r][rule], K[r][phase])
                   for r in ranks), reverse=True)
    total = sum(x[0] for x in rows)
    top = ", ".join("r%d %+.0f (%.0f/%.0f)" % (r, g, t, k) for g, r, t, k in rows[:4])
    print("%-17s net %+8.1f | worst: %s" % (rule, total, top))
