#!/usr/bin/env python3
"""Per-rank rule gap, split into unit cost and work divergence.

`rule_gap_table.py` answers "is this rule's tier-wide gap unit cost?".  This
answers the question that decides where to work: WHICH RANKS carry a rule's
gap, and on each of them, did Tactix do the same work?  A rank whose time
ratio exceeds its work ratio is an implementation gap and belongs to this
goal; a rank whose work ratio matches its time ratio is a scheduling
divergence and does not.

  rank_rule_gap.py <tactix-campaign> [--arm cand] [--rule Factor]

Phase times come from the accepted Kissat baseline (never from the verbose
run, whose `-v` perturbs timing); work counters come from the verbose run,
which is the only place Kissat prints them.
"""

import argparse
import collections
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KISSAT_TIME = ROOT / "benchmarks/runs/20260829-1930-tierd-ranks1-40-accepted-fresh"
KISSAT_STATS = ROOT / "benchmarks/runs/20260905-5064-kissat-verbose-stats-ranks1-40"

# rule -> (Kissat phase, Tactix counter regex, Kissat statistic, label)
RULES = {
    "Substitute": ("substitute", r"^c substitute\s+.*?(\d+) vars substituted",
                   "substituted", "vars"),
    "Factor": ("factor", r"^c factor\s+.*?(\d+) ticks", "factor_ticks", "ticks"),
    "Vivify": ("vivify", r"^c vivify2\s+\d+ reused, (\d+) probes",
               "vivify_probes", "probes"),
    "BVE": ("eliminate",
            r"^c bve\s+\d+ rounds, \d+ eliminated, (\d+) resolutions",
            "eliminate_resolutions", "resolutions"),
    "Congruence": ("congruence", None, None, None),
    "Sweep": ("sweep", r"^c sweep\s+.*?(\d+) equivalences",
              "sweep_equivalences", "equivalences"),
    "Backbone": ("backbone", None, None, None),
    "TransitiveReduce": ("transitive", None, None, None),
}


def tactix_rows(campaign, arm):
    out = {}
    for sub in ("results", "provisional"):
        for path in glob.glob(str(Path(campaign) / sub / "*.json")):
            row = json.loads(Path(path).read_text())
            if row.get("arm") != arm or row.get("rank") is None:
                continue
            if row["rank"] in out:
                continue
            stats = row.get("solver_statistics") or []
            timing = collections.Counter()
            for line in stats:
                m = re.match(r"c timing\s+(\S+)\s+(\d+) ms", line)
                if m:
                    timing[m.group(1)] += int(m.group(2)) / 1000.0
            if timing:
                out[row["rank"]] = (timing, stats)
    return out


def kissat_times():
    out = {}
    for path in glob.glob(str(KISSAT_TIME / "results/*.json")):
        row = json.loads(Path(path).read_text())
        if row.get("arm") != "kissat" or row.get("rank") is None:
            continue
        phases = collections.Counter()
        for line in row.get("solver_statistics") or []:
            m = re.match(r"c\s+([\d.]+)\s+[\d.]+ %\s+(\w+)\s*$", line)
            if m:
                phases[m.group(2)] += float(m.group(1))
        if phases:
            out[row["rank"]] = phases
    return out


def kissat_stats():
    out = {}
    for path in glob.glob(str(KISSAT_STATS / "out/rank*.stdout")):
        rank = int(re.search(r"rank(\d+)", Path(path).name).group(1))
        values = {}
        for line in Path(path).read_text().split("\n"):
            m = re.match(r"c (\w+):\s+(\d+)", line)
            if m:
                values[m.group(1)] = int(m.group(2))
        out[rank] = values
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("campaign")
    ap.add_argument("--arm", default="cand")
    ap.add_argument("--rule", default=None)
    args = ap.parse_args()
    tac = tactix_rows(args.campaign, args.arm)
    ktime, kstat = kissat_times(), kissat_stats()
    ranks = sorted(set(tac) & set(ktime))
    if not ranks:
        sys.exit("no ranks in common")
    print("ranks: %d\n" % len(ranks))
    for rule, (phase, pattern, stat, label) in RULES.items():
        if args.rule and rule != args.rule:
            continue
        rows = []
        for rank in ranks:
            t, k = tac[rank][0][rule], ktime[rank][phase]
            work = None
            if pattern and stat:
                tv = kv = None
                for line in tac[rank][1]:
                    m = re.search(pattern, line)
                    if m:
                        tv = int(m.group(1))
                        break
                kv = kstat.get(rank, {}).get(stat)
                if tv is not None and kv:
                    work = tv / kv
            rows.append((t - k, rank, t, k, work))
        rows.sort(reverse=True)
        # A rank is an implementation gap when Tactix is slower at work that
        # is not larger; 1.05 leaves room for counter-definition drift.
        unit = sum(g for g, _, _, _, w in rows if g > 0 and w is not None and w <= 1.05)
        surplus = sum(g for g, _, _, _, w in rows if g > 0 and w is not None and w > 1.05)
        unknown = sum(g for g, _, _, _, w in rows if g > 0 and w is None)
        print("%-17s net %+7.1f | unit-cost %+.1f  surplus-work %+.1f  unknown %+.1f"
              % (rule, sum(r[0] for r in rows), unit, surplus, unknown))
        for gap, rank, t, k, w in rows[:5]:
            if gap <= 0:
                break
            tag = "work %.2f" % w if w is not None else "work n/a"
            print("      r%-3d %+7.1f  (%6.1f / %6.1f)  %s%s"
                  % (rank, gap, t, k, tag,
                     "  <-- surplus work" if w is not None and w > 1.05 else ""))
    print("\n%s legend: `work` is Tactix/Kissat on the rule's own counter." % " ")


if __name__ == "__main__":
    main()
