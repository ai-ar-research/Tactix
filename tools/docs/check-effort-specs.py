#!/usr/bin/env python3
"""Every scheduled inprocessing rule must have a governor.

In the S4 arrangement, registering a rule is scheduling it: `DefaultPolicy`
builds the inprocessing slot by walking the registry and taking every
`RuleTier::Inprocess` entry. The effort ledger is a separate table,
`EFFORT_SPECS`, keyed by rule name, and a slot entry whose name is absent from
it gets `meter = nullptr, permille = 0` — one full canonical round on every
inprocessing phase, with no meter and no budget.

The gap is structural (two tables, one registry line, no link between them) and
no other check covers it: `check-config-wiring.py` catches the mirror image (an
owned config field nothing writes), the unit tests call a rule's `apply`
directly and never traverse the slot, and fuzz stays green because an unmetered
rule is a performance regression, not a soundness one.

    METERED             has an `EFFORT_SPECS` row. The default expectation, and
                        what a new inprocessing rule should almost always be.
    ALTERNATE-GOVERNOR  deliberately unmetered; something else bounds it, named
                        here, and its mechanism must still be present in the
                        source (`evidence`). Legitimate only for a rule whose
                        single round is inherently bounded — the unmetered
                        governors bound how often a round starts, never how much
                        one round does.

As in check-config-wiring.py, the second half matters as much as the first: a
scheduled rule missing from the table fails, so classification happens when the
rule is registered rather than after a tier run notices the damage.

    tools/docs/check-effort-specs.py           report; exit 1 on any failure
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
POLICY_REL = "src/policies/default_policy.cpp"
POLICY = os.path.join(ROOT, POLICY_REL)
CONFIG_REL = "src/policies/default_strategy_config.hpp"
# All of src/, not just src/rules + src/policies/ops: the registry accepts a
# registration line from anywhere, and a conformance check with a directory
# blind spot goes green because it did not look.
RULE_DIRS = [os.path.join(ROOT, "src")]

METERED = "METERED"
ALT = "ALTERNATE-GOVERNOR"

# The two unmetered governors live on the `e.closure` path in the slot loop.
# `closure` is separate from `!e.meter` because a rule can be both metered and
# closure-gated, and the two bound different things (how much one round does vs
# how often a round starts). A rule with no EFFORT_SPECS row still gets
# `closure = true` by default in the slot build, so an ALTERNATE-GOVERNOR rule
# reaches both declines. These are the `evidence` regexes such a row must match:
# deleting a governor must fail here, not silently unbound the rule.
EV_MATERIAL = r"e\.closure && k\.stats\.conflicts > 0 &&[^;]*e\.binaries_mark"
EV_DELAY = r"closure_delay_ && \(e\.closure \|\| delay_spec"
# And the no-row default is what carries an ALTERNATE-GOVERNOR rule onto that
# path at all, so its disappearance is a failure too.
EV_CLOSURE_DEFAULT = r"e\.closure = true;"
# Congruence's named ladder (kissat congruence.c, `delay.congruence`): the
# DELAY_SPECS row keyed on its merge yield is what `delay_spec(r.name)` finds.
EV_CONGRUENCE_LADDER = (r'\{"Congruence", \[\]\(const Kernel &k\) '
                        r'\{ return k\.stats\.congruence_merges; \}')

# Every rule the slot schedules, classified. The justification is the point of
# the row — for ALTERNATE-GOVERNOR it must NAME the mechanism.
TABLE = {
    # --- metered: an EFFORT_SPECS row is the governor ---------------------
    "BVE":              (METERED, "eliminateeffort 100 permille, resolution currency"),
    "Probe":            (METERED, "probe_permille 8, tick currency"),
    "Backbone":         (METERED, "probing-family 20 permille, tick currency"),
    "TransitiveReduce": (METERED, "probing-family 20 permille"),
    "Vivify":           (METERED, "vivifyeffort 100 permille; the cadical vivifythresh\n                               stand-in is deleted, not merely unused"),
    "Strengthen":       (METERED, "100 permille; budget_aware since D-STRENGTHEN-BUDGET"),
    "SubsumeForward":   (METERED, "forwardeffort 100 permille; budget_aware likewise"),
    "Factor":           (METERED, "factoreffort 50 permille; the factorthresh\n                               stand-in is deleted, not merely unused"),
    "HyperTernaryRes":  (METERED, "cadical ternaryeffort 8 permille + ternarythresh 6"),
    "HyperBinaryRes":   (METERED, "Probe's row transplanted: same schedule, same currency, hyper_binary_permille 8"),
    "BCE":              (METERED, "20 permille, bce_ticks currency"),
    "CCE":              (METERED, "cadical covereffort 4 permille, cce_ticks currency"),
    "Instantiate":      (METERED, "20 permille, tick currency"),
    "Sweep":            (METERED, "kissat sweepeffort 100 permille; the currency is\n                               sweep_effort() = sweep_ticks_sub + sweep_ticks_repr"),
    # Kissat has no Congruence effort row (congruence.c runs one closure to
    # completion).  The source-native new-material watermark and the named
    # productivity delay ladder bound how often a round starts; a single
    # round is bounded by the formula.  An EFFORT_SPECS row here is a
    # non-reference ceiling and fails below, not a governor to restore.
    "Congruence":       (ALT, "new-material watermark plus the exact Kissat "
                              "productivity delay ladder",
                         [EV_MATERIAL, EV_DELAY, EV_CLOSURE_DEFAULT,
                          EV_CONGRUENCE_LADDER]),
    "Substitute":       (METERED, "hand-rolled 10 permille, substitute_ticks "
                                   "currency, no minimum-effort floor; ALSO "
                                   "closure-gated"),
}

# The slot's admission predicate. If this stops being how the slot is built,
# "scheduled" no longer means "Inprocess-tier" and this checker is measuring
# the wrong set — so its disappearance is itself a failure.
SLOT_PREDICATE = r"rules\[i\]\.tier == RuleTier::Inprocess"


def squash(s):
    """Collapse whitespace so the source regexes survive reformatting."""
    return re.sub(r"\s+", " ", s)


def strip_comments(s):
    return re.sub(r"//[^\n]*", "", s)


def registered_rules():
    """name -> (tier, 'relpath:line') for every registry line in the tree."""
    out = {}
    pat = re.compile(r"TACTIX_REGISTER_(?:RULE|POLICY_OP)\(\s*(\w+)\s*,\s*"
                     r"RuleTier::(\w+)", re.S)
    for d in RULE_DIRS:
        for dirpath, _, names in os.walk(d):
            for n in sorted(names):
                if not n.endswith((".cpp", ".cc")):
                    continue
                p = os.path.join(dirpath, n)
                # comments stripped: a commented-out registration line is not a
                # scheduled rule, and sweep.hpp's own prose cites the macro.
                s = strip_comments(open(p).read())
                for m in pat.finditer(s):
                    line = s.count("\n", 0, m.start()) + 1
                    out[m.group(1)] = (m.group(2),
                                       f"{os.path.relpath(p, ROOT)}:{line}")
    return out


def effort_specs():
    """name -> the parsed EFFORT_SPECS row (permille, meter, flags)."""
    s = strip_comments(open(POLICY).read())
    i = s.index("EFFORT_SPECS[]")
    body = s[i:s.index("\n};", i)]
    out = {}
    for m in re.finditer(r'\{\s*"(\w+)"\s*,([^{}]*)\}', body):
        f = [x.strip() for x in m.group(2).split(",")]
        f += [""] * (5 - len(f))
        out[m.group(1)] = {"permille": f[0], "meter": f[1], "dosed": f[2],
                           "budget_aware": f[3]}
    return out


def configured_permille(expr):
    """Resolve the typed strategy field named by an EFFORT_SPECS row."""
    m = re.fullmatch(
        r"&DefaultStrategyConfig::Inprocess::([A-Za-z_]\w*)", expr)
    if not m:
        return None
    src = strip_comments(open(os.path.join(ROOT, CONFIG_REL)).read())
    d = re.search(r"\buint32_t\s+" + re.escape(m.group(1)) +
                  r"\s*=\s*([1-9][0-9]*)\s*;", src)
    return int(d.group(1)) if d else None


def main():
    fails, notes = [], []
    rules = registered_rules()
    src = squash(strip_comments(open(POLICY).read()))
    specs = effort_specs()

    if not re.search(SLOT_PREDICATE, src):
        fails.append(f"the slot's admission predicate `{SLOT_PREDICATE}` is gone "
                     f"from {POLICY_REL}. This checker classifies exactly the "
                     f"Inprocess-tier rules BECAUSE that is how the slot is built; "
                     f"if that changed, re-derive the scheduled set here before "
                     f"trusting a green run.")

    scheduled = sorted(n for n, (tier, _) in rules.items() if tier == "Inprocess")

    # Every registered rule must be visible to the fuzz oracle. `fuzz.py`'s
    # REACH_SPECS maps a rule to the stats counter that proves it fired, and
    # `--require-reached` is part of the phase-3 exit (plan §9), so a rule with
    # no spec is one no leg can be required to reach and whose green legs say
    # nothing. The list is hand-maintained, hence the check.
    reach_src = open(os.path.join(ROOT, "tools/fuzz/fuzz.py")).read()
    reach = set(re.findall(r'^\s*\("([A-Za-z]+)",\s*r"', reach_src, re.M))
    for n in sorted(rules):
        if n not in reach:
            fails.append(
                f"registered rule `{n}` ({rules[n][1]}) has no REACH_SPECS entry "
                f"in tools/fuzz/fuzz.py, so no fuzz leg can be REQUIRED to reach "
                f"it and a green leg says nothing about it. Add the counter that "
                f"proves it fired — and key it on a counter the DOSED path moves, "
                f"not a round counter apply_instance never touches.")

    for n in scheduled:
        if n not in TABLE:
            fails.append(f"UNCLASSIFIED scheduled rule `{n}` ({rules[n][1]}) — it is "
                         f"registered at RuleTier::Inprocess, so the slot RUNS it on "
                         f"every inprocessing phase. Give it an EFFORT_SPECS row and "
                         f"a METERED line in TABLE in "
                         f"{os.path.relpath(__file__, ROOT)}, or classify it "
                         f"ALTERNATE-GOVERNOR and name what bounds it. Registering an "
                         f"Inprocess rule IS scheduling it.")

    for n, row in TABLE.items():
        kind, why = row[0], row[1]
        if n not in rules:
            fails.append(f"TABLE lists `{n}` but no registry line declares it — "
                         f"stale row, delete it.")
            continue
        if rules[n][0] != "Inprocess":
            fails.append(f"TABLE lists `{n}` but it registers at RuleTier::"
                         f"{rules[n][0]} ({rules[n][1]}), so the slot never "
                         f"schedules it — stale row, delete it.")
            continue
        if kind == METERED:
            s = specs.get(n)
            if not s:
                fails.append(f"UNGOVERNED: `{n}` is tagged METERED ({why}) but has NO "
                             f"EFFORT_SPECS row in {POLICY_REL}. The slot will run it "
                             f"with meter=nullptr and permille=0 — one unbudgeted "
                             f"canonical round on EVERY inprocessing phase.")
                continue
            ok = True
            if s["meter"] in ("nullptr", "NULL", ""):
                ok = False
                fails.append(f"`{n}`'s EFFORT_SPECS row has no meter function, so the "
                             f"ledger cannot price it — the row buys nothing.")
            permille = configured_permille(s["permille"])
            if permille is None:
                ok = False
                fails.append(f"`{n}`'s EFFORT_SPECS row has permille=`{s['permille']}`; "
                             f"it must name a positive uint32_t field in "
                             f"{CONFIG_REL}.")
            if s["dosed"] != "true" and s["budget_aware"] != "true":
                ok = False
                fails.append(f"`{n}`'s EFFORT_SPECS row is neither `dosed` nor "
                             f"`budget_aware`, so its share bounds only how OFTEN a "
                             f"round starts, never how much ONE round does — the "
                             f"D-STRENGTHEN-BUDGET shape (implementation-notes.md).")
            if ok:
                notes.append(f"METERED {n:17} <- {permille} permille "
                             f"({s['permille']}), "
                             f"{s['meter']}"
                             f"{', dosed' if s['dosed'] == 'true' else ''}"
                             f"{', budgeted' if s['budget_aware'] == 'true' else ''}")
        else:  # ALTERNATE-GOVERNOR
            if n in specs:
                fails.append(f"`{n}` is tagged ALTERNATE-GOVERNOR ({why}) but DOES "
                             f"have an EFFORT_SPECS row — the row is a governor, so "
                             f"the classification is wrong however the closure flag "
                             f"is set. Retag it METERED (and say in the row's "
                             f"justification if it is ALSO closure-gated).")
                continue
            missing = [e for e in row[2] if not re.search(e, src)]
            if missing:
                fails.append(f"UNGOVERNED: `{n}` is tagged ALTERNATE-GOVERNOR ({why}) "
                             f"but its governor is GONE from {POLICY_REL} — no match "
                             f"for {missing}. With no EFFORT_SPECS row and no "
                             f"alternate governor, the slot runs it unbounded on "
                             f"every phase.")
            else:
                notes.append(f"ALT-GOV {n:17} <- {why.split(';')[0].strip()}")

    for n in specs:
        if n not in rules:
            fails.append(f"EFFORT_SPECS has a row for `{n}` but no registry line "
                         f"declares it — inert row (lookup is by name), delete it.")
        elif rules[n][0] != "Inprocess":
            fails.append(f"EFFORT_SPECS has a row for `{n}` but it registers at "
                         f"RuleTier::{rules[n][0]} — the slot never reads it.")

    for n in notes:
        print(f"  ok    {n}")
    for f in fails:
        print(f"  FAIL  {f}")
    n_alt = sum(1 for r in TABLE.values() if r[0] == ALT)
    print(f"check-effort-specs: {len(scheduled)} scheduled Inprocess rules, "
          f"{len(specs)} EFFORT_SPECS rows, {n_alt} alternate-governor, "
          f"{len(fails)} failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
