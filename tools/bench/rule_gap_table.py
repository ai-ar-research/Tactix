#!/usr/bin/env python3
"""Per-rule time gap against Kissat, beside the work counters that decide it.

A rule that spends longer than Kissat is only an implementation gap when it
does the same work; when it does more, the fix is a scheduling port instead.
Deciding that from a profile costs hours, so this reads it off two campaigns:
a Tactix gate (its `c timing` rows) and a Kissat baseline (its phase profile),
plus the counter pairs below, each of which was checked at its increment site
in the reference before being listed here.

  tools/bench/rule_gap_table.py <tactix-campaign> [--kissat <campaign>]

Counters print as Tactix/Kissat with their ratio, so a rule whose time ratio
exceeds its work ratio is unit cost and a rule whose work ratio matches its
time ratio is not.  Kissat prints a statistic only when it is non-zero and
only under `-v`, so a missing pair is reported rather than silently dropped.
"""

import argparse
import collections
import glob
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KISSAT_DEFAULT = ROOT / "benchmarks/runs/20260905-5064-kissat-verbose-stats-ranks1-40"
# Fallback: the accepted baseline carries phase times but only a subset of
# the statistics, because it was not run under `-v`.
KISSAT_PHASES_ONLY = ROOT / "benchmarks/runs/20260829-1930-tierd-ranks1-40-accepted-fresh"

# Tactix rule timer -> Kissat profile phase.  `Strengthen` is deliberately
# absent: Kissat runs forward subsumption inside `eliminate`, not in its
# `subsume` phase, so the honest pairing is BVE+Strengthen against
# eliminate+subsume and is reported separately below.
RULE_PHASE = {
    "Vivify": "vivify",
    "BVE": "eliminate",
    "Substitute": "substitute",
    "Congruence": "congruence",
    "Factor": "factor",
    "Sweep": "sweep",
    "Backbone": "backbone",
    "TransitiveReduce": "transitive",
}

# Work counters, Tactix regex -> Kissat statistic.  Each was read at its
# increment site in the reference; see the unit-cost report, section 3.23.
WORK = {
    "BVE": [
        (r"^c bve\s+\d+ rounds, (\d+) eliminated", "eliminated", "eliminated"),
        (r"^c bve\s+\d+ rounds, \d+ eliminated, (\d+) resolutions",
         "eliminate_resolutions", "resolutions"),
    ],
    "Substitute": [
        (r"^c substitute\s+.*?(\d+) vars substituted", "substituted",
         "variables substituted"),
    ],
    "Vivify": [
        (r"^c vivify\s+\d+ rounds, (\d+) vivified", "vivified", "vivified"),
        (r"^c vivify2\s+\d+ reused, (\d+) probes", "vivify_probes", "probes"),
    ],
    "Sweep": [
        (r"^c sweep\s+.*?(\d+) equivalences", "sweep_equivalences",
         "equivalences"),
    ],
    "Strengthen": [
        (r"^c strengthen\s+\d+ rounds \(\d+ checked, (\d+) visits",
         "forward_steps", "forward steps"),
    ],
    "Factor": [
        (r"^c factor\s+.*?(\d+) ticks", "factor_ticks", "ticks"),
    ],
}


def tactix_rows(campaign, arm=None):
    """rank -> (rule timings in seconds, statistics lines).

    A paired campaign carries two Tactix arms, so `arm` selects one; without
    it a single-arm campaign is read as before.
    """
    out = {}
    for sub in ("provisional", "results"):
        for path in glob.glob(str(Path(campaign) / sub / "*.json")):
            row = json.loads(Path(path).read_text())
            if row.get("arm") == "kissat":
                continue
            if arm is not None and row.get("arm") != arm:
                continue
            rank = row.get("rank")
            if rank is None or rank in out:
                continue
            stats = row.get("solver_statistics") or []
            timing = collections.Counter()
            for line in stats:
                m = re.match(r"c timing\s+(\S+)\s+(\d+) ms", line)
                if m:
                    timing[m.group(1)] += int(m.group(2)) / 1000.0
            if timing:
                out[rank] = (timing, stats)
    return out


def parse_kissat(lines):
    phases, stats = collections.Counter(), {}
    for line in lines:
        m = re.match(r"c\s+([\d.]+)\s+[\d.]+ %\s+(\w+)\s*$", line)
        if m:
            phases[m.group(2)] += float(m.group(1))
            continue
        m = re.match(r"c (\w+):\s+(\d+)", line)
        if m:
            stats[m.group(1)] = int(m.group(2))
    return phases, stats


def kissat_rows(campaign):
    """rank -> (phase seconds, statistic name -> value).

    Accepts a gate campaign (per-rank JSON with a `kissat` arm) or a verbose
    statistics campaign (`out/rank<N>.stdout`).  Only the latter carries every
    statistic, because Kissat prints the full set only under `-v`.
    """
    out = {}
    for path in glob.glob(str(Path(campaign) / "out" / "rank*.stdout")):
        rank = int(re.search(r"rank(\d+)", Path(path).name).group(1))
        phases, stats = parse_kissat(Path(path).read_text().split("\n"))
        if phases:
            out[rank] = (phases, stats)
    if out:
        return out
    for path in glob.glob(str(Path(campaign) / "results" / "*.json")):
        row = json.loads(Path(path).read_text())
        if row.get("arm") != "kissat":
            continue
        rank = row.get("rank")
        if rank is None:
            continue
        phases, stats = parse_kissat(row.get("solver_statistics") or [])
        if phases:
            out[rank] = (phases, stats)
    return out


def sum_counter(rows, ranks, pattern):
    total, seen = 0, 0
    for rank in ranks:
        for line in rows[rank][1]:
            m = re.search(pattern, line)
            if m:
                total += int(m.group(1))
                seen += 1
                break
    return total, seen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tactix", help="Tactix gate campaign directory")
    ap.add_argument("--kissat-stats", default=str(KISSAT_DEFAULT),
                    help="verbose Kissat campaign supplying work counters")
    ap.add_argument("--kissat-time", default=str(KISSAT_PHASES_ONLY),
                    help="accepted Kissat baseline supplying phase times")
    ap.add_argument("--arm", default=None,
                    help="Tactix arm of a paired campaign (e.g. cand)")
    args = ap.parse_args()

    # Phase times and work counters come from different campaigns on purpose:
    # `-v` and the symbol build both perturb timing, so the verbose run is a
    # statistics source only and the accepted baseline remains the clock.
    tac = tactix_rows(args.tactix, args.arm)
    stats_rows = kissat_rows(args.kissat_stats)
    time_rows = kissat_rows(args.kissat_time)
    kis = {r: (time_rows[r][0], stats_rows.get(r, (None, {}))[1])
           for r in time_rows}
    ranks = sorted(set(tac) & set(kis))
    if not ranks:
        sys.exit("no ranks in common; is the Tactix campaign still running?")
    print("ranks compared: %d\n" % len(ranks))

    print("%-18s %9s %9s %9s %6s" % ("rule", "tactix", "kissat", "gap", "ratio"))
    rows, net = [], 0.0
    for rule, phase in RULE_PHASE.items():
        t = sum(tac[r][0][rule] for r in ranks)
        k = sum(kis[r][0][phase] for r in ranks)
        net += t - k
        rows.append((t - k, rule, t, k))
    strengthen = sum(tac[r][0]["Strengthen"] for r in ranks)
    net += strengthen - sum(kis[r][0]["subsume"] for r in ranks)
    for gap, rule, t, k in sorted(rows, reverse=True):
        print("%-18s %9.1f %9.1f %+9.1f %6.2f"
              % (rule, t, k, gap, t / k if k else 0))
    print("%-18s %9.1f %9s %9s %6s"
          % ("Strengthen*", strengthen, "-", "-", "-"))
    print("%-18s %9s %9s %+9.1f" % ("NET", "", "", net))

    # The honest elimination pairing: Kissat subsumes inside `eliminate`.
    t = sum(tac[r][0]["BVE"] + tac[r][0]["Strengthen"] for r in ranks)
    k = sum(kis[r][0]["eliminate"] + kis[r][0]["subsume"] for r in ranks)
    print("\n* BVE+Strengthen vs eliminate+subsume: %.1f vs %.1f = %.2fx"
          % (t, k, t / k if k else 0))

    print("\nwork counters (tactix / kissat)")
    for rule, pairs in WORK.items():
        for pattern, stat, label in pairs:
            tv, seen = sum_counter(tac, ranks, pattern)
            kv = sum(kis[r][1].get(stat, 0) for r in ranks)
            missing = [r for r in ranks if stat not in kis[r][1]]
            note = ""
            if not seen:
                note = "  (tactix counter not found)"
            elif missing:
                note = "  (kissat omits it on %d rank(s); needs -v)" % len(missing)
            ratio = ("%6.2f" % (tv / kv)) if kv else "     -"
            print("  %-12s %-22s %14d / %-14d %s%s"
                  % (rule, label, tv, kv, ratio, note))


if __name__ == "__main__":
    main()
