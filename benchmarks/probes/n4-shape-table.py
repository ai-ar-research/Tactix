#!/usr/bin/env python3
"""Turn the n4-shape probe outputs into the per-technique OCCASION/YIELD table.

    n4-shape-table.py <scratch-dir>

Reads every `<jobid>_<task>.out` written by n4-shape.sh and emits, per instance,
one row per technique with the two solvers side by side. Occasions and yields
only -- no wall-clock field is parsed, because none was measured.

COUNTER SCOPE, read at the source before comparing ([[reference-counter-scope]]):
  * tactix `c slot ... ok N` is incremented ONLY in run_batch_entry, so for the
    DOSED entries (Probe, Backbone, TransitiveReduce, BVE) it is always 0 and
    must not be read as "never productive". `adm` is the occasion count that
    matters for every entry.
  * tactix `c <rule> N rounds` is the RULE-side apply count. For a dosed entry
    the policy applies one instance at a time, so `rounds` counts applies (or
    stays 0 where the rule counts only canonical rounds) -- again, use `adm`.
  * kissat `probings`/`eliminations`/`closures`/`sweep`/`vivifications`/
    `substitutions`/`backbone_computations`/`transitive_reductions`/
    `factorizations`/`forward_subsumptions` each count CALLS of that technique;
    their secondary column is conflicts/call, i.e. the realized mean interval.
  * kissat `congruent` counts MERGED VARIABLES, not gates; `congruent_gates` is
    the gate volume. Comparing our `merges` to their `congruent` is like-for-like.
  * kissat `sweep_variables` counts swept variables (seeds); our nearest is
    `sweep_environments` (a seed whose environment was built and solved).
"""
import os
import re
import sys
from collections import defaultdict

# tactix `c slot <name> occ N adm N ok N | dec dis .. | handed .. spent ..`
SLOT = re.compile(
    r"^c slot (\S+)\s+occ\s+(\d+) adm\s+(\d+) ok\s+(\d+) \| dec "
    r"dis (\d+) cap (\d+) pre (\d+) grd (\d+) bud (\d+) dly (\d+) mat (\d+) "
    r"warm (\d+) dns (\d+) one (\d+) nv (\d+) \| handed (\d+) spent (\d+) "
    r"completed (\d+)")
CLOCK = re.compile(
    r"^c slot-clock\s+probe phases (\d+) next (\d+) blocked (\d+) \| "
    r"eliminate phases (\d+) next (\d+) blocked (\d+)")
KSTAT = re.compile(r"^c (\w+):\s+(\d+)")
HDR = re.compile(r"^=== arm=(\S+) tier=(\S+) instance=(\S+) cap=(\d+)s")

# tactix single-line yields we quote verbatim in the report
TAC_YIELD_PREFIXES = (
    "c congruence ", "c congy1 ", "c conggates ", "c sweep ", "c sweepemit ",
    "c vivify ", "c vivify2 ", "c bve ", "c bve-gates ", "c factor ",
    "c substitute ", "c backbone ", "c probe ", "c transitive ", "c subsume ",
    "c strengthen ", "c ternary ", "c instantiate ", "c bce ", "c cce ",
    "c walk ", "c reductions ", "c restarts ", "c conflicts ", "c inprocess ",
    "c tiers ", "c meters ",
)
KISSAT_KEYS = [
    "conflicts", "search_ticks", "probings", "eliminations", "closures",
    "substitutions", "backbone_computations", "vivifications", "sweep",
    "transitive_reductions", "factorizations", "forward_subsumptions", "walks",
    "reductions", "restarts",
    # yields
    "eliminated", "eliminate_resolutions", "congruent", "congruent_gates",
    "substituted", "backbone_units", "backbone_probes", "backbone_rounds",
    "vivified", "vivify_checks", "sweep_equivalences", "sweep_units",
    "sweep_variables", "sweep_completed", "sweep_solved",
    "transitive_reduced", "transitive_units", "transitive_probes",
    "factored", "subsumed", "strengthened", "forward_checks", "units",
]


def parse(path):
    arm = tier = inst = None
    tac_slots, tac_lines, kstats, clock = {}, {}, {}, None
    verdict = None
    for line in open(path, errors="replace"):
        line = line.rstrip("\n")
        m = HDR.match(line)
        if m:
            arm, tier, inst = m.group(1), m.group(2), m.group(3)
            continue
        if line.startswith("s "):
            verdict = line.split()[1]
        m = SLOT.match(line)
        if m:
            g = m.groups()
            tac_slots[g[0]] = dict(
                occ=int(g[1]), adm=int(g[2]), ok=int(g[3]), dis=int(g[4]),
                cap=int(g[5]), pre=int(g[6]), grd=int(g[7]), bud=int(g[8]),
                dly=int(g[9]), mat=int(g[10]), warm=int(g[11]),
                dns=int(g[12]), one=int(g[13]), nv=int(g[14]),
                handed=int(g[15]), spent=int(g[16]), completed=int(g[17]))
            continue
        m = CLOCK.match(line)
        if m:
            clock = dict(probe_phases=int(m.group(1)),
                         probe_blocked=int(m.group(3)),
                         elim_phases=int(m.group(4)),
                         elim_blocked=int(m.group(6)))
            continue
        for p in TAC_YIELD_PREFIXES:
            if line.startswith(p):
                tac_lines[p.strip()] = line
                break
        m = KSTAT.match(line)
        if m and m.group(1) in KISSAT_KEYS:
            kstats[m.group(1)] = int(m.group(2))
    if arm is None:
        return None
    return dict(arm=arm, tier=tier, inst=inst, verdict=verdict,
                slots=tac_slots, lines=tac_lines, kstats=kstats, clock=clock)


def main():
    scratch = sys.argv[1]
    runs = defaultdict(dict)
    for name in sorted(os.listdir(scratch)):
        if not name.endswith(".out"):
            continue
        r = parse(os.path.join(scratch, name))
        if r:
            runs[r["inst"]][r["arm"]] = r

    print("## Cross-check: metrics build vs pinned build (same trajectory?)\n")
    print("| instance | kmet conflicts | kpin conflicts | kmet verdict | kpin verdict | same |")
    print("|---|---|---|---|---|---|")
    for inst, arms in runs.items():
        a, b = arms.get("kmet"), arms.get("kpin")
        if not a or not b:
            continue
        ca = a["kstats"].get("conflicts")
        cb = b["kstats"].get("conflicts")
        print("| %s | %s | %s | %s | %s | %s |" %
              (inst, ca, cb, a["verdict"], b["verdict"],
               "YES" if ca == cb and a["verdict"] == b["verdict"] else "**NO**"))

    print("\n## Occasions per technique, per solve\n")
    print("| instance | verdict t/k | conflicts t/k | probe phases t/k | elim phases t/k |")
    print("|---|---|---|---|---|")
    for inst, arms in runs.items():
        t, k = arms.get("tactix"), arms.get("kmet")
        if not t or not k:
            continue
        tc = t["lines"].get("c conflicts", "")
        tconf = tc.split()[-1] if tc else "?"
        print("| %s | %s/%s | %s/%s | %s/%s | %s/%s |" % (
            inst, t["verdict"], k["verdict"], tconf,
            k["kstats"].get("conflicts"),
            t["clock"]["probe_phases"] if t["clock"] else "?",
            k["kstats"].get("probings"),
            t["clock"]["elim_phases"] if t["clock"] else "?",
            k["kstats"].get("eliminations")))

    # per-technique occasion pairing: (our slot entry, kissat call counter)
    pairs = [("Congruence", "closures"), ("Substitute", "substitutions"),
             ("Backbone", "backbone_computations"), ("Vivify", "vivifications"),
             ("Sweep", "sweep"), ("TransitiveReduce", "transitive_reductions"),
             ("Factor", "factorizations"),
             ("SubsumeForward", "forward_subsumptions")]
    print("\n## Occasions ADMITTED (tactix `adm`) vs kissat CALLS\n")
    hdr = "| instance |" + "".join(" %s |" % p[0] for p in pairs)
    print(hdr)
    print("|---" * (len(pairs) + 1) + "|")
    for inst, arms in runs.items():
        t, k = arms.get("tactix"), arms.get("kmet")
        if not t or not k:
            continue
        cells = []
        for ours, theirs in pairs:
            a = t["slots"].get(ours, {}).get("adm", "-")
            b = k["kstats"].get(theirs, "-")
            cells.append(" %s / %s |" % (a, b))
        print("| %s |%s" % (inst, "".join(cells)))

    # YIELD pairing. Each row names the two counters and why they are
    # comparable; a pair with no honest counterpart is left out rather than
    # invented.
    print("\n## Yield, ours vs kissat's (like-for-like counters only)\n")
    print("| instance | BVE vars elim t/k | Factor vars t/k | Congruence merges t/k "
          "| Congruence gates t/k | Sweep equiv+units t/k | Sweep seeds t/k "
          "| Vivify vivified t/k | Vivify checks t/k | Subst vars t/k "
          "| Backbone units t/k | Transitive deleted t/k | C1 subsumed t/k |")
    print("|---" * 13 + "|")

    def num(line, idx):
        if not line:
            return "-"
        f = line.split()
        try:
            return f[idx]
        except IndexError:
            return "-"

    for inst, arms in runs.items():
        t, k = arms.get("tactix"), arms.get("kmet")
        if not t or not k:
            continue
        L, K = t["lines"], k["kstats"]
        row = [
            # `c bve N rounds, M eliminated, R resolutions`
            "%s / %s" % (num(L.get("c bve"), 4), K.get("eliminated", "-")),
            # `c factor N factorings, ...` -- our nearest to kissat `factored`
            # (variables added) is factorings; both are counted below in detail
            "%s / %s" % (num(L.get("c factor"), 2), K.get("factored", "-")),
            # `c congruence N rounds, G gates, M merges, U units, ...`
            "%s / %s" % (num(L.get("c congruence"), 6), K.get("congruent", "-")),
            "%s / %s" % (num(L.get("c congruence"), 4),
                         K.get("congruent_gates", "-")),
            # `c sweep R rounds, E environments (C clauses), S solves, N
            #  candidates, Q equivalences ...`
            "%s / %s" % (num(L.get("c sweep"), 12),
                         (K.get("sweep_equivalences", 0) +
                          K.get("sweep_units", 0)) if "sweep_equivalences" in K
                         else "-"),
            "%s / %s" % (num(L.get("c sweep"), 4), K.get("sweep_variables", "-")),
            # `c vivify N rounds, V vivified, D deleted, C checked`
            "%s / %s" % (num(L.get("c vivify"), 4), K.get("vivified", "-")),
            "%s / %s" % (num(L.get("c vivify"), 8), K.get("vivify_checks", "-")),
            # `c substitute N rounds, S non-trivial SCCs (L lits), V vars
            #  substituted, ...`
            "%s / %s" % (num(L.get("c substitute"), 9), K.get("substituted", "-")),
            # `c backbone N rounds, P probed, F failed`
            "%s / %s" % (num(L.get("c backbone"), 6), K.get("backbone_units", "-")),
            # `c transitive N rounds, C checked, D deleted, F failed, T ticks`
            "%s / %s" % (num(L.get("c transitive"), 6),
                         K.get("transitive_reduced", "-")),
            # `c subsume N rounds (C checked, V visits, S subsumed)`
            "%s / %s" % (num(L.get("c subsume"), 8).rstrip(")"),
                         K.get("subsumed", "-")),
        ]
        print("| %s | %s |" % (inst, " | ".join(row)))

    print("\n## Raw per-instance detail\n")
    for inst, arms in runs.items():
        print("### %s\n" % inst)
        t, k = arms.get("tactix"), arms.get("kmet")
        if t:
            print("**tactix** (verdict %s)\n```" % t["verdict"])
            for name, s in t["slots"].items():
                print("slot %-18s occ %5d adm %4d ok %4d | dis %d cap %d pre %d "
                      "grd %d bud %d dly %d mat %d warm %d dns %d | handed %d "
                      "spent %d completed %d" %
                      (name, s["occ"], s["adm"], s["ok"], s["dis"], s["cap"],
                       s["pre"], s["grd"], s["bud"], s["dly"], s["mat"],
                       s["warm"], s["dns"], s["handed"], s["spent"],
                       s["completed"]))
            for key in sorted(t["lines"]):
                print(t["lines"][key])
            print("```\n")
        if k:
            print("**kissat 4.0.4 (-DMETRICS)** (verdict %s)\n```" % k["verdict"])
            for key in KISSAT_KEYS:
                if key in k["kstats"]:
                    print("%-24s %d" % (key + ":", k["kstats"][key]))
            print("```\n")


if __name__ == "__main__":
    main()
