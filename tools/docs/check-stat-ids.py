#!/usr/bin/env python3
"""RV-S11: the §5.6 reward channel — every dosed rule exposes a (yield, effort)
pair, and every id returns the counter its name claims.

WHY THIS EXISTS. `tactix_stat` is the learned policy's ONLY reward channel
(plan §5.6: pull-based, the learner reads counters and nothing else), and it is
a hand-maintained switch of integer cases in `src/api/api.cpp` against a
hand-numbered list of macros in `include/tactix.h`. Two independent failure
modes, neither of which any test could see:

  1. A rule family with NO id at all. Sweep, Congruence, CCE and Substitute
     each shipped with none, so a learner could schedule them and never read
     what the schedule bought. Nothing fails when an id is simply absent.
  2. An id wired to the WRONG counter. The whole test-suite evidence for this
     surface was `tactix_stat(s, id) != UINT64_MAX` for id in 1..31 — a
     PRESENCE check. Swapping `case 26` and `case 27` (BCE yield <-> BCE
     effort) passes every one of those assertions while inverting the learner's
     cost-per-unit-output axis, which is the axis §5.6 says orders these rules.

So this checker does three things a unit test cannot:

  ORDER     header numbering is dense, unique, and identical to api.cpp's case
            order, and every case carries the macro name it implements.
  NAMING    the expression a case returns must mention every significant token
            of its macro's name. That is what catches the 26/27 swap:
            `TACTIX_STAT_BCE_DELETED` returning `k.stats.bce_ticks` has no
            "deleted" in it. Rows where the counter genuinely has another name
            are ALIASed one at a time, with the reason.
  PAIRING   every rule the inprocessing slot doses (an `EFFORT_SPECS` row) has
            BOTH ids, and the effort id returns exactly the counter that rule's
            own `meter_*` function reads — i.e. the currency the schedule is
            actually denominated in, not merely some plausible work counter.
            A newly dosed rule therefore fails here until its pair is exposed.

    tools/docs/check-stat-ids.py            report; exit 1 on any failure
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
HEADER_REL = "include/tactix.h"
API_REL = "src/api/api.cpp"
POLICY_REL = "src/policies/default_policy.cpp"

# Macro tokens that no expression can be expected to contain, mapped to the
# counter the id is REQUIRED to return instead. One row per deviation, each
# with its reason — the point is that the deviation is declared, not inferred.
ALIAS = {
    "TACTIX_STAT_SWITCHES": ("k.stats.switched",
                             "the counter is past-tense; the id is a count"),
    "TACTIX_STAT_LEVEL": ("k.level", "kernel state, not a Stats counter"),
    "TACTIX_STAT_UNASSIGNED": ("k.unassigned", "kernel state"),
    "TACTIX_STAT_LAST_LEARNED_GLUE": ("k.pending.glue",
                                      "the pending clause outlives Backjump; "
                                      "that IS the last learned glue"),
    "TACTIX_STAT_STABLE": ("k.h && k.h->stable ? 1 : 0",
                           "H's mode, and 0 for a null-H solver"),
    "TACTIX_STAT_BINARY_CLAUSES": ("k.binary_ids.size()",
                                   "the id SET is the counter"),
}

# The (yield, effort) pair per rule family. The KEY set is checked against
# `EFFORT_SPECS` — a dosed rule missing from here fails, which is the whole
# point: exposing the pair is part of landing a dosed rule, not a follow-up.
PAIRS = {
    "BVE":              ("TACTIX_STAT_BVE_ELIMINATED", "TACTIX_STAT_BVE_RESOLUTIONS"),
    "Probe":            ("TACTIX_STAT_PROBE_FAILED", "TACTIX_STAT_PROBE_TICKS"),
    "Backbone":         ("TACTIX_STAT_BACKBONE_FAILED", "TACTIX_STAT_BACKBONE_TICKS"),
    "TransitiveReduce": ("TACTIX_STAT_TRANSITIVE_DELETED", "TACTIX_STAT_TRANSITIVE_TICKS"),
    "Vivify":           ("TACTIX_STAT_VIVIFIED", "TACTIX_STAT_VIVIFY_TICKS"),
    "Strengthen":       ("TACTIX_STAT_STRENGTHENED", "TACTIX_STAT_STRENGTHEN_VISITS"),
    "SubsumeForward":   ("TACTIX_STAT_FORWARD_SUBSUMED", "TACTIX_STAT_FORWARD_VISITS"),
    "Factor":           ("TACTIX_STAT_FACTORINGS", "TACTIX_STAT_FACTOR_TICKS"),
    "HyperTernaryRes":  ("TACTIX_STAT_TERNARY_RESOLVENTS", "TACTIX_STAT_TERNARY_TICKS"),
    "HyperBinaryRes":   ("TACTIX_STAT_HBR_BINARIES", "TACTIX_STAT_HBR_TICKS"),
    "BCE":              ("TACTIX_STAT_BCE_DELETED", "TACTIX_STAT_BCE_TICKS"),
    "CCE":              ("TACTIX_STAT_CCE_DELETED", "TACTIX_STAT_CCE_TICKS"),
    "Instantiate":      ("TACTIX_STAT_INSTANTIATED", "TACTIX_STAT_INSTANTIATE_TICKS"),
    "Sweep":            ("TACTIX_STAT_SWEEP_EQUIVALENCES", "TACTIX_STAT_SWEEP_TICKS"),
    "Congruence":       ("TACTIX_STAT_CONGRUENCE_MERGES", "TACTIX_STAT_CONGRUENCE_TICKS"),
}
# Substitute has no EFFORT_SPECS row (an un-titratable closure — the slot gates
# how OFTEN it runs, per check-effort-specs.py's ALTERNATE-GOVERNOR row), so it
# has no meter to check the effort id against. It is still a rule family a
# learner schedules, so it must carry a pair; the effort id is declared here as
# a PROXY and is exempt from the meter-identity check only for that reason.
PROXY_PAIRS = {
    "Substitute": ("TACTIX_STAT_SUBSTITUTE_VARS",
                   "TACTIX_STAT_SUBSTITUTE_SCC_LITS"),
}


def read(rel):
    return open(os.path.join(ROOT, rel)).read()


def header_ids():
    """macro -> (id, declaration index)."""
    out, order = {}, 0
    for m in re.finditer(r"^#define\s+(TACTIX_STAT_\w+)\s+(\d+)", read(HEADER_REL),
                         re.M):
        out[m.group(1)] = (int(m.group(2)), order)
        order += 1
    return out


def api_cases():
    """id -> (returned expression, trailing comment, declaration index)."""
    s = read(API_REL)
    i = s.index("uint64_t tactix_stat(")
    body = s[i:s.index("\n}\n", i)]
    out, order = {}, 0
    pat = re.compile(r"case (\d+):\s*(?://([^\n]*)\n(?:\s*//[^\n]*\n)*)?"
                     r"\s*return ([^;]+);", re.M)
    for m in pat.finditer(body):
        out[int(m.group(1))] = (re.sub(r"\s+", " ", m.group(3)).strip(),
                                (m.group(2) or "").strip(), order)
        order += 1
    return out, ("default:\n    return UINT64_MAX;" in body)


def brace_body(s, open_at):
    """The text between `s[open_at] == '{'` and its matching brace. A regex
    cannot do this: the meters are a mix of one-liners and bodies containing a
    lambda, and `(.*?)\\n\\}` on a one-liner runs past the function into every
    counter below it — which reads as a PASS for whatever it was asked about."""
    depth = 0
    for i in range(open_at, len(s)):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return s[open_at + 1:i]
    return ""


def effort_meters():
    """rule name -> the Stats counter its EFFORT_SPECS meter reads."""
    s = re.sub(r"//[^\n]*", "", read(POLICY_REL))
    meters = {}
    for m in re.finditer(r"uint64_t (meter_\w+)\(const Kernel &\w*\)\s*\{", s):
        # every k.stats.X the meter can return (meter_transitive has an env arm)
        body = brace_body(s, m.end() - 1)
        meters[m.group(1)] = set(re.findall(r"k\.stats\.(\w+)", body))
    body = s[s.index("EFFORT_SPECS[]"):]
    body = body[:body.index("\n};")]
    out = {}
    for m in re.finditer(r'\{\s*"(\w+)"\s*,\s*\d+\s*,\s*(meter_\w+)', body):
        out[m.group(1)] = meters.get(m.group(2), set())
    return out


def tokens(macro):
    return [t.lower() for t in macro[len("TACTIX_STAT_"):].split("_")]


def main():
    fails = []
    ids = header_ids()
    cases, has_default = api_cases()
    meters = effort_meters()

    if not ids:
        fails.append(f"no TACTIX_STAT_* macros parsed out of {HEADER_REL} — the "
                     f"header's shape changed and this checker is measuring "
                     f"nothing. Fix the parser before trusting a green run.")
    if not cases:
        fails.append(f"no `case N: return ...;` rows parsed out of "
                     f"{API_REL}'s tactix_stat — same problem.")
    if not meters:
        fails.append(f"no EFFORT_SPECS meters parsed out of {POLICY_REL} — same "
                     f"problem.")
    if ids and not has_default:
        fails.append(f"{API_REL}: tactix_stat lost its `default: return "
                     f"UINT64_MAX;` arm. That sentinel is how a caller "
                     f"distinguishes an unknown id from a zero counter.")

    # --- ORDER ------------------------------------------------------------
    by_id = {}
    for macro, (i, order) in sorted(ids.items(), key=lambda kv: kv[1][1]):
        if i in by_id:
            fails.append(f"{HEADER_REL}: id {i} is claimed by both "
                         f"`{by_id[i]}` and `{macro}`.")
        by_id[i] = macro
    if ids:
        want = list(range(len(ids)))
        if sorted(by_id) != want:
            missing = [i for i in want if i not in by_id]
            fails.append(f"{HEADER_REL}: the id space is not dense 0..{len(ids)-1} "
                         f"(missing {missing}). A learner enumerates ids by "
                         f"counting up; a hole reads as an unknown id.")
        for macro, (i, order) in ids.items():
            if i != order:
                fails.append(f"{HEADER_REL}: `{macro}` = {i} but is the "
                             f"{order}-th declared. Declaration order must be "
                             f"numeric order, or the list cannot be read.")
                break

    for i, macro in sorted(by_id.items()):
        if i not in cases:
            fails.append(f"{macro} ({i}) is documented in {HEADER_REL} but has "
                         f"NO case in {API_REL} — it returns the unknown-id "
                         f"sentinel, so the reward channel for it is dead.")
            continue
        expr, comment, order = cases[i]
        if order != i:
            fails.append(f"{API_REL}: `case {i}:` is the {order}-th case. Case "
                         f"order must match id order — a reader (and the next "
                         f"editor) relies on the sequence to spot a swap.")
        if macro not in comment:
            fails.append(f"{API_REL}: `case {i}:` does not name `{macro}` in its "
                         f"comment (found: '{comment}'). The name at the case is "
                         f"the only local evidence the case is the right one.")
        alias = ALIAS.get(macro)
        if alias:
            if expr != alias[0]:
                fails.append(f"{API_REL}: `case {i}:` ({macro}) returns `{expr}`, "
                             f"but its ALIAS row in "
                             f"{os.path.relpath(__file__, ROOT)} requires "
                             f"`{alias[0]}` ({alias[1]}). Either the wiring "
                             f"changed or the row is stale — decide which.")
        else:
            missing = [t for t in tokens(macro) if t not in expr.lower()]
            if missing:
                fails.append(f"{API_REL}: `case {i}:` claims `{macro}` but returns "
                             f"`{expr}`, which does not mention {missing}. This is "
                             f"the mis-wired-reward class: the id reads as one "
                             f"quantity and returns another. Add an ALIAS row with "
                             f"a reason if the counter genuinely has another name.")

    for i in sorted(cases):
        if i not in by_id:
            fails.append(f"{API_REL}: `case {i}:` returns a counter no "
                         f"TACTIX_STAT_* macro documents, so no caller can ask "
                         f"for it. Name it in {HEADER_REL}.")

    # --- PAIRING ----------------------------------------------------------
    for rule in sorted(meters):
        if rule not in PAIRS:
            fails.append(f"DOSED rule `{rule}` has an EFFORT_SPECS row but no "
                         f"(yield, effort) pair in "
                         f"{os.path.relpath(__file__, ROOT)}. The slot doses it, "
                         f"so a learner schedules it — and with no ids it cannot "
                         f"read what the dose bought. Expose both and add the row.")
    for rule, (y, e) in sorted(list(PAIRS.items()) + list(PROXY_PAIRS.items())):
        proxy = rule in PROXY_PAIRS
        if not proxy and rule not in meters:
            fails.append(f"PAIRS lists `{rule}` but it has no EFFORT_SPECS row — "
                         f"stale row, or the rule lost its meter.")
        for macro in (y, e):
            if macro not in ids:
                fails.append(f"`{rule}`'s pair names `{macro}`, which "
                             f"{HEADER_REL} does not define.")
        if proxy or rule not in meters or e not in ids:
            continue
        expr = cases.get(ids[e][0], ("", "", -1))[0]
        counters = meters[rule]
        if counters and not any(f"k.stats.{c}" == expr for c in counters):
            fails.append(f"`{rule}`'s EFFORT id `{e}` returns `{expr}`, "
                         f"but the rule is DOSED on {sorted(counters)} "
                         f"({POLICY_REL}). Yield/effort is only a real "
                         f"cost-per-unit-output if the effort id is the currency "
                         f"the schedule spends.")

    for f in fails:
        print(f"FAIL: {f}\n")
    n = len(ids)
    print(f"check-stat-ids: {n} ids, {len(cases)} cases, "
          f"{len(PAIRS) + len(PROXY_PAIRS)} rule pairs "
          f"({len(PROXY_PAIRS)} proxy) — {len(fails)} failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
