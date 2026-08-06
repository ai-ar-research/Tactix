#!/usr/bin/env python3
"""SCHED-A2b: every SCHEDULED inprocessing rule must have a governor.

WHY THIS EXISTS. In the S4 arrangement, REGISTERING a rule IS scheduling it:
`DefaultPolicy` builds the inprocessing slot by walking the registry and taking
every `RuleTier::Inprocess` entry (default_policy.cpp, "The S4 arrangement
enables every Inprocess-tier rule"). The effort ledger is a SEPARATE table —
`EFFORT_SPECS` — keyed by rule NAME, and a slot entry whose name is absent from
it gets `meter = nullptr, permille = 0`: it runs with no meter and no budget,
one full canonical round on every inprocessing phase, forever.

The gap is structural — two tables, one registry line, no link between them —
and no other check covers it: `check-config-wiring.py` catches the MIRROR image
(a config field we own and never write), the unit tests call a rule's `apply`
directly and never traverse the slot, and fuzz stays green because an unmetered
rule is a PERFORMANCE regression, not a soundness one.

    METERED             has an `EFFORT_SPECS` row. The default expectation, and
                        what a new inprocessing rule should almost always be.
    ALTERNATE-GOVERNOR  deliberately unmetered; something ELSE bounds it, named
                        here, and its mechanism must still be present in the
                        source (`evidence`). Legitimate only for a rule whose
                        single round is inherently bounded — the unmetered
                        governors bound how OFTEN a round starts, never how much
                        one round does.

As in check-config-wiring.py, the second half matters as much as the first: a
scheduled rule that is NOT in the table fails, so classification happens when
the rule is registered rather than after a tier run notices the damage.

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
# ALL of src/, not just src/rules + src/policies/ops: the registry accepts a
# registration line from anywhere, and a conformance check with a directory
# blind spot is exactly the kind of "green because it did not look" this file
# exists to prevent.
RULE_DIRS = [os.path.join(ROOT, "src")]

METERED = "METERED"
ALT = "ALTERNATE-GOVERNOR"

# Both unmetered governors live on the `!e.meter` path in the slot loop, so a
# rule reaches them precisely by having no EFFORT_SPECS row. These are the
# `evidence` regexes an ALTERNATE-GOVERNOR row must still match: deleting the
# governor must fail HERE, not silently promote the rule to unbounded.
EV_MATERIAL = r"!e\.meter && k\.stats\.conflicts > 0 &&[^;]*e\.binaries_mark"
EV_DELAY = r"closure_delay_ && !e\.meter"

# Every rule the slot schedules, classified. The justification is the point of
# the row — for ALTERNATE-GOVERNOR it must NAME the mechanism.
TABLE = {
    # --- metered: an EFFORT_SPECS row is the governor ---------------------
    "BVE":              (METERED, "eliminateeffort 100 permille, resolution currency"),
    "Probe":            (METERED, "probing-family 20 permille, tick currency"),
    "Backbone":         (METERED, "probing-family 20 permille, tick currency"),
    "TransitiveReduce": (METERED, "probing-family 20 permille"),
    "Vivify":           (METERED, "vivifyeffort 100 permille + cadical vivifythresh 20"),
    "Strengthen":       (METERED, "100 permille; budget_aware since D-STRENGTHEN-BUDGET"),
    "SubsumeForward":   (METERED, "forwardeffort 100 permille; budget_aware likewise"),
    "Factor":           (METERED, "factoreffort 50 permille + factorthresh 7"),
    "HyperTernaryRes":  (METERED, "cadical ternaryeffort 8 permille + ternarythresh 6"),
    "BCE":              (METERED, "20 permille, bce_ticks currency"),
    "CCE":              (METERED, "cadical covereffort 4 permille, cce_ticks currency"),
    "Instantiate":      (METERED, "20 permille, tick currency"),
    "Sweep":            (METERED, "kissat sweepeffort 100 permille, sweep_ticks currency"),
    # --- deliberately unmetered: a different mechanism governs them --------
    # Both are CLOSURES with no titratable instance surface: one canonical pass
    # over the current formula, whose cost is bounded by the formula rather
    # than by how long we let it run — so "at most one pass per new-material
    # occasion, decayed by the delay ladder when unproductive" is a real bound
    # on the work. That argument does NOT transfer to a search-shaped rule
    # (Sweep), which is why Sweep is METERED and not parked here.
    # KNOWN WEAKNESS, recorded rather than papered over: this bounds how OFTEN
    # a round starts, not how much one round does, and a single Congruence
    # application has been priced in the tens of seconds (default_policy.cpp
    # :1044). Metering it is an open task, not an accepted shape -- see
    # PROGRESS, SCHED-A2b.
    "Congruence":       (ALT, "new-material watermark + closure delay ladder "
                              "(the two `!e.meter` declines in the slot loop, "
                              "default_policy.cpp:1379/1390); no instance surface",
                         [EV_MATERIAL, EV_DELAY]),
    "Substitute":       (ALT, "new-material watermark + closure delay ladder "
                              "on the same `!e.meter` path; canonical instance only",
                         [EV_MATERIAL, EV_DELAY]),
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
                           "retry_advice": f[3], "budget_aware": f[4]}
    return out


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
            if not re.fullmatch(r"[1-9][0-9]*", s["permille"]):
                ok = False
                fails.append(f"`{n}`'s EFFORT_SPECS row has permille=`{s['permille']}`; "
                             f"a zero or unparsed share is not a budget.")
            if s["dosed"] != "true" and s["budget_aware"] != "true":
                ok = False
                fails.append(f"`{n}`'s EFFORT_SPECS row is neither `dosed` nor "
                             f"`budget_aware`, so its share bounds only how OFTEN a "
                             f"round starts, never how much ONE round does — the "
                             f"D-STRENGTHEN-BUDGET shape (implementation-notes.md).")
            if ok:
                notes.append(f"METERED {n:17} <- {s['permille']} permille, "
                             f"{s['meter']}"
                             f"{', dosed' if s['dosed'] == 'true' else ''}"
                             f"{', budgeted' if s['budget_aware'] == 'true' else ''}")
        else:  # ALTERNATE-GOVERNOR
            if n in specs:
                fails.append(f"`{n}` is tagged ALTERNATE-GOVERNOR ({why}) but DOES "
                             f"have an EFFORT_SPECS row — it is metered, so the "
                             f"unmetered `!e.meter` governors named here never see "
                             f"it. Retag it METERED.")
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
