#!/usr/bin/env python3
"""Summarize the block-2 anti-vacuity readout over a Tier B array.

One line per PRICING ROW, not per counter: for each row of the phase-4 table it
prints on how many instances the mechanism the row's arm removes actually fired,
which instances those are, and the resulting verdict —

  LIVE  the arm can differ from the shipped configuration; it earns its window
  VOID  the counter is zero tier-wide, so the arm is bit-identical and pricing it
        would bank a negative that measures nothing
  N/A   the counter cannot answer the question from a SHIPPED run (the mechanism
        ships OFF, so its zero is structural) — read it from the arm's own run

A row is judged on the WHOLE tier: every counter here is driven by the cumulative
clock, so a zero on the instances that finish quickly says nothing about the ones
that run to the cap ([[ab-arm-identical-artefacts]], and the 2026-08-07 D3/E3
triage that two retirements did not survive).
"""
import glob
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
D = os.path.join(ROOT, "benchmarks/probes/scratch/block2-counters")

# (row, what the arm removes, regex over the run's text, "sum"|"max" of the
# groups, note). `ships_off` rows are marked N/A: their counter is zero in a
# shipped run by construction, so a shipped-config probe cannot price them.
SPECS = [
    # C6 reads `subsumed`, not `tried`: deleting a clause is the sub-step's only
    # non-local effect, so `subsumed == 0` is exactly bit-identity.
    ("C6", "eager subsumption (clauses actually deleted)",
     r"^c eagersub\s+(\d+) subsumed of", "sum", False),
    # C7 reads the SECOND field. A sequence can start and divert no decision at
    # all when every splitmix64 try lands on an assigned or retired variable, so
    # `sequences == 0` is sufficient for bit-identity but not necessary.
    ("C7", "random decisions actually taken",
     r"^c randec\s+\d+ sequences, (\d+) random", "sum", False),
    ("C8", "BVE ITE+XOR gate extraction",
     r"^c bve-gates\s+\d+ equiv, \d+ andor, (\d+) ite, (\d+) xor", "sum", False),
    ("C9", "binary-reason jumping (ships OFF)",
     r"^c jumpreason\s+(\d+) binary", "sum", True),
    ("D1", "the authored slot order",
     r"^c slot-order\s+(\S+)", "text", False),
    ("D2", "the two slot clocks",
     r"^c slot-clock\s+probe phases (\d+) next \d+ blocked \d+ \| "
     r"eliminate phases (\d+)", "sum", False),
    ("D3", "the slot watermarks",
     r"^c slot-clock\s+probe phases \d+ next \d+ blocked (\d+) \| "
     r"eliminate phases \d+ next \d+ blocked (\d+)", "sum", False),
    ("E3", "the effort ceiling (clamps; read `global ticks` beside it)",
     r"^c slot\s+\S+.*maxeff \d+ clamped (\d+)", "sum", False),
    # F1's witness is the SEVEN `*_mark_skipped` counters, not `marks`: the
    # producers run in both arms (kernel_touch_var is outside every touch_filter
    # guard), so `marks` is identical with the filter off. Five of the seven
    # consumers report on `c touch3` / `c cce2` / `c touch4`, so reading `c touch`
    # alone would let a tier-wide zero on five of them pass unseen.
    ("F1", "the touched-epoch substrate (7 consumers' skips)",
     r"^c touch\s+\d+ marks, \d+ epoch; C1 (\d+) skipped .*?, C2 (\d+) skipped"
     r"|^c touch3\s+BCE (\d+) skipped / \d+ rearmed, X2 (\d+) skipped / \d+ "
     r"rearmed, X4 (\d+) skipped"
     r"|^c cce2\s+(\d+) skipped"
     r"|^c touch4\s+Factor (\d+) seeds skipped", "sum", False),
    ("F2", "elimination rounds held open by C1/C2",
     r"^c subhold\s+(\d+) elimination", "sum", False),
    # Two rows, deliberately. `touchred` counts the POPULATION the band would
    # admit and is counted whether the band is on or off, so it prices the ON
    # arm's cost from a shipped run; `f7band` is the band's own yield and is
    # structurally zero until the arm is set.
    ("F6/F7", "the redundant band's population (cost side, arm-independent)",
     r"^c touchred\s+(\d+) redundant creations", "sum", False),
    ("F6/F7y", "the redundant band's yield (ships OFF)",
     r"^c f7band\s+C2 (\d+) band candidates", "sum", True),
    ("G3", "Vivify trail reuse",
     r"^c vivify2\s+(\d+) reused", "sum", False),
    ("G4", "the Y1 unit sweep",
     r"^c congy1\s+(\d+) found, (\d+) units", "sum", False),
    ("N1", "the post-shrink glue clamp (disagreements only)",
     r"^c shrinkglue\s+(\d+) promoted", "sum", False),
    ("N2", "first-activation decision order",
     r"^c tumble\s+(\d+) vars seeded", "sum", False),
    ("R9", "HyperBinaryRes resolvents",
     r"^c hbr\s+\d+ rounds, \d+ probes, (\d+) binaries", "sum", False),
    ("R9t", "HyperBinaryRes cost (ticks)",
     r"^c hbr\s+\d+ rounds, \d+ probes, \d+ binaries, \d+ subsumed, "
     r"\d+ declined, (\d+) ticks", "sum", False),
    ("R10", "BVE definition extraction",
     r"^c bve-gates\s+\d+ equiv, \d+ andor, \d+ ite, \d+ xor, (\d+) definition",
     "sum", False),
    ("R10c", "BVE definition sub-solve ceiling hits",
     r"^c bve-def\s+\d+ checked, \d+ ticks, (\d+) hit", "sum", False),
]


DARM = os.path.join(ROOT, "benchmarks/probes/scratch/block2-armreach")

# The ships-OFF rows, whose witness must come from the ARM's own run. Each entry
# is (arm label, row, witness regex, what a zero means).
ARM_SPECS = [
    ("c9on", "C9", r"^c jumpreason\s+(\d+) binary",
     "the jump never fired: the ON arm is bit-identical and the row is UNRUN"),
    ("f67on", "F6/F7 producer",
     r"^c touchred\s+\d+ redundant creations in scope \(dynamic\) vs \d+ "
     r"\(static tier2=6\), (\d+) band marks",
     "nothing was stamped: the producer half of the arm did not run"),
    ("f67on", "F6/F7 consumer", r"^c f7band\s+C2 (\d+) band candidates",
     "C2 never looked at the band: only the producer's cost was measured"),
    ("d2on", "D2", r"^c slot-clock\s+probe phases (\d+) next \d+ blocked \d+ \| "
     r"eliminate phases (\d+)",
     "no slot phase ran at all, so collapsing the clocks changes nothing"),
]


def read_runs():
    runs = {}
    for f in sorted(glob.glob(os.path.join(D, "*_*.out"))):
        t = open(f, errors="replace").read()
        m = re.search(r"stem=(\S+)", t)
        if not m:
            runs[os.path.basename(f)] = None
            continue
        runs[m.group(1)] = t
    return runs


def read_arm_runs():
    runs = {}
    for f in sorted(glob.glob(os.path.join(DARM, "*_*.out"))):
        t = open(f, errors="replace").read()
        m = re.search(r"stem=(\S+).*?arm=(\S+)", t, re.S)
        if not m:
            runs[("?", os.path.basename(f))] = None
            continue
        runs[(m.group(2), m.group(1))] = t
    return runs


def arms_main():
    runs = {k: v for k, v in read_arm_runs().items() if v}
    labels = sorted({a for a, _ in runs})
    print(f"read {len(runs)} arm-runs over {len(labels)} arm(s): {labels}\n")
    for label, row, rx, zero_means in ARM_SPECS:
        got = {s: value(t, rx, "sum") for (a, s), t in runs.items() if a == label}
        if not got:
            print(f"{row:<16} {label:<8} NO RUNS")
            continue
        hits = {s: v for s, v in got.items() if v}
        verdict = "LIVE" if hits else "VOID"
        top = ", ".join(f"{s}={v}" for s, v in
                        sorted(hits.items(), key=lambda kv: -kv[1])[:4])
        print(f"{row:<16} {label:<8} {verdict:<6} {len(hits)}/{len(got)} instances")
        if top:
            print(f"                 top: {top}")
        else:
            print(f"                 zero tier-wide — {zero_means}")
    return 0


def value(text, rx, mode):
    ms = re.findall(rx, text, re.M)
    if not ms:
        return None
    if mode == "text":
        return ms[0] if isinstance(ms[0], str) else ms[0][0]
    tot = 0
    for g in ms:
        gs = g if isinstance(g, tuple) else (g,)
        # An alternation yields "" for every branch that did not match; those are
        # absences, not zeros, and summing them as ints is a crash, not a reading.
        tot += sum(int(x) for x in gs if x != "")
    return tot


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--arms":
        return arms_main()
    runs = read_runs()
    bad = [k for k, v in runs.items() if v is None]
    ok = {k: v for k, v in runs.items() if v}
    print(f"read {len(ok)} instances ({len(bad)} unreadable: {bad})")
    ticks = {k: value(v, r"global ticks (\d+)", "sum") or 0 for k, v in ok.items()}
    if ticks:
        hi = sorted(ticks.items(), key=lambda kv: -kv[1])[:3]
        print("largest global-tick runs (the ones a subsample drops): "
              + ", ".join(f"{k} {v:,}" for k, v in hi))
    verdicts = {}
    for row, what, rx, mode, ships_off in SPECS:
        hits, missing = [], []
        for stem, text in sorted(ok.items()):
            v = value(text, rx, mode)
            if v is None:
                missing.append(stem)
            elif mode == "text" or (isinstance(v, int) and v > 0):
                hits.append((stem, v))
        if ships_off:
            verdict = "N/A-ships-off"
        elif hits:
            verdict = "LIVE"
        elif missing and len(missing) == len(ok):
            verdict = "NO-COUNTER"
        else:
            verdict = "VOID"
        verdicts[row] = verdict
        top = ", ".join(f"{s}={v}" for s, v in
                        sorted(hits, key=lambda h: -h[1] if isinstance(h[1], int) else 0)[:4])
        print(f"\n{row:<6} {verdict:<14} {len(hits)}/{len(ok)} instances — {what}")
        if top:
            print(f"       top: {top}")
        if missing:
            print(f"       counter line ABSENT on {len(missing)}: {missing[:4]}"
                  f"{' …' if len(missing) > 4 else ''}")
    print("\nVOID rows must be dropped from the pricing batch or re-armed; "
          "N/A rows need the counter read from their OWN arm, never from a "
          "shipped run.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
