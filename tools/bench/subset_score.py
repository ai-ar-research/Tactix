#!/usr/bin/env python3
"""Re-score an existing Tier C run over a subset list — no solver runs.

T3's parity half (plan §8.3) is judged on the *rule-neutral subset*: the
instances where the still-missing phase-4 rules do not materially help the
baselines. That subset is a scoring view over Tier C, not a separate tier —
running it separately would produce numbers not comparable to the full-tier
window they came from. So: run Tier C once with every arm, then score it twice.

    tools/bench/subset_score.py benchmarks/runs/<stamp>-C-<name> [--subset C-ruleneutral]

Prints PAR-2 + #solved per arm over the subset and over the full tier, plus the
ratio to each baseline arm, so both halves of T3 are readable from one window.

PAR-2 and the solved/unsolved rule are taken verbatim from run.py's convention:
an instance counts solved only when the runner scored it solved, which for a
scored arm includes the proof check (plan §8.1).
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TIERS = os.path.join(ROOT, "tools", "bench", "tiers")

# Reference-solver names, a last resort only (see split_arms). Matching an arm
# by name is wrong in both directions: `--baseline label=<command>` gives a
# baseline any label it likes (`k_nosweep`, `ref`), which would then be scored
# as a candidate and corrupt the T3 ratio, while a `--checked-arm
# cadical-tuned=...` candidate would be taken for a denominator.
BASELINE_ARMS = ("kissat", "cadical", "minisat", "glucose")


def split_arms(rundir, run, arms):
    """Split arms into (baselines, candidates): a candidate is an arm run.py
    scored.

    Two recorded discriminators, either authoritative: manifest.json carries
    `score_proofs` per solver (true for a candidate, false for `--baseline`,
    plan §8.1), and results.json["oracle"]["arms"] lists the scored arms
    (present only when a run had 2+ of them).

    A run predating those fields falls back to the name list, deliberately:
    every such run named its baselines `kissat`/`cadical` literally, and an
    archived number must keep reading the way it read when it was banked.
    """
    manifest = os.path.join(rundir, "manifest.json")
    if os.path.exists(manifest):
        solvers = json.load(open(manifest)).get("solvers") or []
        flag = {s["name"]: bool(s["score_proofs"]) for s in solvers
                if isinstance(s, dict) and "score_proofs" in s}
        # All-or-nothing: a manifest describing only some arms says nothing
        # about the rest, so fall through rather than mix two rules.
        if all(a in flag for a in arms):
            return ([a for a in arms if not flag[a]],
                    [a for a in arms if flag[a]], "manifest score_proofs")
    orc = run.get("oracle")
    if isinstance(orc, dict) and orc.get("arms"):
        scored = set(orc["arms"])
        return ([a for a in arms if a not in scored],
                [a for a in arms if a in scored], "results.json oracle arms")
    return ([a for a in arms if a.lower() in BASELINE_ARMS],
            [a for a in arms if a.lower() not in BASELINE_ARMS],
            "solver-name fallback (run predates the score_proofs flag)")


def read_subset(name):
    path = os.path.join(TIERS, f"{name}.list")
    if not os.path.exists(path):
        sys.exit(f"error: subset list {path} does not exist")
    keep = set()
    for line in open(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        keep.add(line.split()[0])
    return keep


def par2(records, timeout):
    if not records:
        return float("nan"), 0, 0
    total = sum(r["time"] if r["solved"] else 2 * timeout for r in records)
    solved = sum(1 for r in records if r["solved"])
    return total / len(records), solved, len(records)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rundir")
    ap.add_argument("--subset", default="C-ruleneutral")
    args = ap.parse_args()

    results = os.path.join(args.rundir, "results.json")
    if not os.path.exists(results):
        sys.exit(f"error: {results} not found (run --aggregate first?)")
    run = json.load(open(results))
    timeout = run["timeout"]
    keep = read_subset(args.subset)

    present = {r["instance"] for arm in run["results"].values() for r in arm}
    missing = keep - present
    if missing:
        print(f"warning: {len(missing)} subset instances absent from this run:")
        for m in sorted(missing):
            print(f"  {m}")

    rows = {}
    for arm, recs in run["results"].items():
        rows[arm] = (par2([r for r in recs if r["instance"] in keep], timeout),
                     par2(recs, timeout))

    print(f"run    {os.path.basename(args.rundir)}   git {run.get('git')}   "
          f"timeout {timeout:g}s")
    print(f"subset {args.subset} ({len(keep & present)} of {len(present)} instances)\n")
    w = max(len(a) for a in rows)
    print(f"{'arm':<{w}}  {'subset PAR-2':>13} {'solved':>8}   "
          f"{'full PAR-2':>11} {'solved':>8}")
    for arm, ((sp, ss, sn), (fp, fs, fn)) in rows.items():
        print(f"{arm:<{w}}  {sp:13.2f} {f'{ss}/{sn}':>8}   "
              f"{fp:11.2f} {f'{fs}/{fn}':>8}")

    # T3 reads as a ratio against each baseline, on both views.
    baselines, cands, how = split_arms(args.rundir, run, list(rows))
    print(f"\narms   {len(cands)} ours, {len(baselines)} baseline "
          f"[{how}]")
    if baselines and cands:
        print()
        for b in baselines:
            (bs, _, _), (bf, _, _) = rows[b]
            for c in cands:
                (cs, _, _), (cf, _, _) = rows[c]
                print(f"  {c} / {b}:  subset {cs / bs:.3f}x   full {cf / bf:.3f}x")
    return 0


if __name__ == "__main__":
    sys.exit(main())
