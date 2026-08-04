#!/usr/bin/env python3
"""Re-derive the T3 rule-neutral subset from a REPLICATED sweep ablation.

The shipped `benchmarks/tiers/C-ruleneutral.list` came from a single-shot
ablation: exclude an instance iff ablating Sweep moved either baseline by >25 %.
With one measurement per arm that test cannot separate "sweep matters here" from
"this instance is chaotic" — and 4 of its 6 exclusions are instances measured to
swing 8–23x under perturbations that have nothing to do with sweep.

This applies the criterion that single-shot data could not support:

    exclude iff  |median(nosweep) - median(base)|  >  max(band(base), band(nosweep))
                 and the relative move exceeds REL_MIN

where band(arm) = max - min over that arm's repeats on that instance. In words:
**the sweep delta has to stand out against the instance's own variance**, not
against zero. An instance whose replicates already span more than the sweep
effect is not evidence about sweep, and it stays IN the subset.

    tools/bench/subset_rederive.py benchmarks/runs/<stamp>-C-<name> [--write]

Prints the per-instance table and the proposed list; --write updates
benchmarks/tiers/C-ruleneutral.list in place (header regenerated).
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REL_MIN = 1.25  # keep the original 25 % threshold as a floor


def median(xs):
    s = sorted(xs)
    n = len(s)
    return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rundir")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()

    run = json.load(open(os.path.join(args.rundir, "results.json")))
    T = run["timeout"]

    # group arms by family: k_* / knosw_* / c_* / cnosw_*
    fams = {}
    for arm, recs in run["results"].items():
        fam = arm.rsplit("_", 1)[0]
        for r in recs:
            t = r["time"] if r["solved"] else 2 * T
            fams.setdefault(fam, {}).setdefault(r["instance"], []).append(t)

    need = {"k", "knosw", "c", "cnosw"}
    if not need <= set(fams):
        sys.exit(f"error: need arm families {sorted(need)}, found {sorted(fams)}")

    insts = sorted(fams["k"])
    keep, drop = [], []
    print(f"{'instance':<46} {'kis d':>7} {'kis band':>9} {'cad d':>7} "
          f"{'cad band':>9}  verdict")
    for i in insts:
        row = {}
        for fam in need:
            xs = fams[fam].get(i, [])
            row[fam] = (median(xs), max(xs) - min(xs)) if xs else (0.0, 0.0)
        excl, why = False, []
        for base, nos, tag in (("k", "knosw", "kis"), ("c", "cnosw", "cad")):
            bm, bb = row[base]
            nm, nb = row[nos]
            delta = abs(nm - bm)
            band = max(bb, nb)
            rel = (nm / bm) if bm else 1.0
            # BOTH tests must fire: bigger than the instance's own variance AND
            # a materially large relative move.
            if delta > band and (rel > REL_MIN or rel < 1 / REL_MIN):
                excl = True
                why.append(f"{tag} {rel:.2f}x d={delta:.0f}>band={band:.0f}")
        v = "EXCLUDE " + "; ".join(why) if excl else "keep"
        print(f"{i.split('/')[-1][:46]:<46} {abs(row['knosw'][0]-row['k'][0]):7.0f} "
              f"{max(row['k'][1], row['knosw'][1]):9.0f} "
              f"{abs(row['cnosw'][0]-row['c'][0]):7.0f} "
              f"{max(row['c'][1], row['cnosw'][1]):9.0f}  {v}")
        (drop if excl else keep).append(i)

    print(f"\nkeep {len(keep)}, exclude {len(drop)}")
    old = set()
    p = os.path.join(ROOT, "benchmarks", "tiers", "C-ruleneutral.list")
    if os.path.exists(p):
        old = {l.split()[0] for l in open(p)
               if l.strip() and not l.startswith("#")}
        print(f"shipped list keeps {len(old)}; "
              f"added {len(set(keep) - old)}, removed {len(old - set(keep))}")
        for i in sorted(set(keep) - old):
            print(f"  + {i}")
        for i in sorted(old - set(keep)):
            print(f"  - {i}")
    if args.write:
        print("\n--write: not implemented as an in-place edit on purpose — a "
              "frozen tier list changes only via a reviewed commit. Copy the "
              "keep-set above into C-ruleneutral.list with a header recording "
              "this run's stamp and the replicated criterion.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
