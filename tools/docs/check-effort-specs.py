#!/usr/bin/env python3
"""Every scheduled inprocessing rule must have a governor.

In the S4 arrangement, `DefaultPolicy` builds the inprocessing slot by walking
the `RuleTier::Inprocess` registry. A rule with no canonical producer may be
explicit-only and omitted from the default append path, but that exception
must prove both the omission and its public API reachability. The effort ledger
is a separate table, `EFFORT_SPECS`, keyed by rule name, and a slot entry absent from
it gets `meter = nullptr, permille = 0` — one full canonical round on every
inprocessing phase, with no meter and no budget.

The gap is structural (two tables, one registry line, no link between them) and
no other check covers it: `check-config-wiring.py` catches the mirror image (an
owned config field nothing writes), the unit tests call a rule's `apply`
directly and never traverse the slot, and fuzz stays green because an unmetered
rule is a performance regression, not a soundness one.

    METERED             has an `EFFORT_SPECS` row. The default expectation, and
                        what a new inprocessing rule should almost always be.
    ALTERNATE-GOVERNOR  deliberately unmetered; a reference-authored mechanism
                        bounds its call frequency, named here, and that mechanism
                        must still be present in the source (`evidence`). A single
                        round remains structurally bounded by the finite formula.
    EXPLICIT-ONLY       no enabled default slot or CLI producer; a typed public
                        action owns the tuple. An authored disabled row may pin
                        ordering before admission. The canonical registry apply
                        is NoOp, and dedicated API tests replace an impossible
                        CLI REACH_SPECS entry. This does not accept N4 scheduling.

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
EXPLICIT = "EXPLICIT-ONLY"

# The named delay path is independent of the optional generic closure watermark.
# Deleting a registered reference governor must fail here, but this checker must
# not require an invented admission predicate merely because a rule is unmetered.
EV_DELAY = r"closure_delay_ && \(e\.closure \|\| delay_spec"
# Congruence's named ladder (kissat congruence.c, `delay.congruence`): the
# DELAY_SPECS row keyed on its merge yield is what `delay_spec(r.name)` finds.
EV_CONGRUENCE_LADDER = (r'\{"Congruence", \[\]\(const Kernel &k\) '
                        r'\{ return k\.stats\.congruence_merges; \}')
EV_SR_LEGACY_SKIP = (r'std::strcmp\(rules\[i\]\.name, '
                     r'"SubstitutionRedundancy"\)')
# Completion is an optional third argument; the same named root owner must
# still receive the native kernel and stop publisher.
EV_SR_ROOT_OWNER = r"run_structured_root\(k, stop(?:, completion_?)?\)"
EV_SR_RUN_ONCE = r"structured_root_consumed_ \|\| k\.stats\.conflicts"
EV_SR_SCOPED_FIXPOINT = (r"result\.repeat = \{strategy::RepeatMode::"
                         r"UntilSizeScaledFixpoint, 128\}")
EV_CONDITION_SKIP = r'std::strcmp\(rules\[i\]\.name, "Condition"\)'
EV_CONDITION_NOOP = (r'ApplyResult condition_apply\(Kernel &\) \{ '
                     r'return ApplyResult::NoOp; \}')

# Every registered Inprocess rule, classified. The justification is the point
# of the row — for ALTERNATE-GOVERNOR it must NAME the mechanism.
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
    # completion). Its named productivity delay ladder governs call frequency;
    # it has no binary/root new-material admission predicate. An EFFORT_SPECS
    # row here is a non-reference ceiling and fails below, not a governor to
    # restore.
    "Congruence":       (ALT, "the exact Kissat productivity delay ladder",
                         [EV_DELAY, EV_CONGRUENCE_LADDER]),
    "Substitute":       (METERED, "hand-rolled 10 permille, substitute_ticks "
                                   "currency, no minimum-effort floor; ALSO "
                                   "closure-gated"),
    "SubstitutionRedundancy":
                        (ALT, "the root-only structured program's run-once "
                              "ownership, typed producer limits and semantic "
                              "sequence fixpoint",
                         [EV_SR_LEGACY_SKIP, EV_SR_ROOT_OWNER, EV_SR_RUN_ONCE,
                          EV_SR_SCOPED_FIXPOINT]),
    "ParityConsequence": (EXPLICIT, "typed bounded parity action; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "ParityConsequence"\)'],
                         [("src/rules/parity_consequence/parity_consequence.cpp",
                           r'parity_live_apply\('),
                          ("src/strategy/online_inprocess.cpp",
                           r'ParityConsequenceParameters'),
                          ("tests/test_parity_consequence.cpp", r'public_c\(')]),
    "AffineConsequence": (EXPLICIT, "typed certified affine consequence; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "AffineConsequence"\)'],
                         [("src/rules/semantic_extension/affine_consequence_action.cpp",
                           r'affine_consequence_action_apply\('),
                          ("tests/test_affine_consequence_action.cpp",
                           r'propagation_is_deferred\(')]),
    "ExactCoverExtension": (EXPLICIT, "typed conservative exact-cover extension; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "ExactCoverExtension"\)'],
                         [("src/rules/semantic_extension/exact_cover_action.cpp",
                           r'exact_cover_extension_action_apply\('),
                          ("tests/test_exact_cover_action.cpp",
                           r'success_stages_candidate_and_commits_identity\(')]),
    "IndependentSetCover": (EXPLICIT, "typed structured independent-set consequence; no canonical producer",
                         [r'std::strcmp\(rules\[i\]\.name, "IndependentSetCover"\)'],
                         [("src/rules/semantic_extension/independent_set_cover_action.cpp",
                           r'independent_set_cover_action_apply\('),
                          ("tests/test_independent_set_cover_action.cpp",
                           r'void exercise\(')]),
    "Gf3AffineConsequence": (EXPLICIT, "typed caller-selected GF(3) equation-pair consequence; no canonical producer",
                         [r'std::strcmp\(rules\[i\]\.name, "Gf3AffineConsequence"\)'],
                         [("src/rules/semantic_extension/gf3_affine_action.cpp",
                           r'gf3_affine_action_apply\('),
                          ("tests/test_theory_action_v5_runtime.cpp",
                           r'gf3_step\('),
                          ("tests/test_api_instances.c",
                           r'TACTIX_ONLINE_PARAMETERS_GF3_AFFINE_CONSEQUENCE')]),
    "DirectCspSupport": (EXPLICIT, "typed caller-selected Direct CSP support row; no canonical producer",
                         [r'std::strcmp\(rules\[i\]\.name, "DirectCspSupport"\)'],
                         [("src/rules/semantic_extension/direct_csp_support_action.cpp",
                           r'direct_csp_support_action_apply\('),
                          ("tests/test_theory_action_v5_runtime.cpp",
                           r'direct_step\('),
                          ("tests/test_api_instances.c",
                           r'TACTIX_ONLINE_PARAMETERS_DIRECT_CSP_SUPPORT')]),
    "DirectPhpRefutation": (EXPLICIT, "typed terminal direct-PHP action; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "DirectPhpRefutation"\)'],
                         [("src/rules/direct_php/direct_php_action.cpp",
                           r'direct_php_action_apply\('),
                          ("tests/test_direct_php_public.cpp",
                           r'random_generator_reaches_the_same_action\('),
                          ("tests/test_direct_php_public.cpp",
                           r'c_api_executes_terminal_action\(')]),
    "Gf3GaussianConsequence":
                        (EXPLICIT, "typed authenticated complete-batch GF(3) action; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "Gf3GaussianConsequence"\)'],
                         [("src/rules/semantic_extension/gf3_gaussian_action.cpp",
                           r'gf3_gaussian_action_apply\('),
                          ("tests/test_gf3_gaussian_public.cpp",
                           r'singular_batch_and_atomic_publication\(')]),
    "TseitinCycleBreaking":
                        (EXPLICIT, "typed authenticated selected XOR-component action; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "TseitinCycleBreaking"\)'],
                         [("src/rules/lucky/tseitin_cycle_action.cpp",
                           r'tseitin_cycle_breaking_apply\('),
                          ("tests/test_tseitin_cycle_action.cpp",
                           r'typed_replay_random_reach_and_separate_propagation\('),
                          ("tests/test_tseitin_cycle_action.cpp",
                           r'c_v7_is_backward_compatible_and_deep_copies_identity\(')]),
    "NaryCspPrefix":
                        (EXPLICIT, "typed authenticated n-ary CSP prefix action; default schedule pending",
                         [r'std::strcmp\(rules\[i\]\.name, "NaryCspPrefix"\)'],
                         [("src/rules/semantic_extension/nary_csp_prefix_action.cpp",
                           r'nary_csp_prefix_action_apply\('),
                          ("tests/test_nary_csp_prefix_public.cpp",
                           r'native_random_offer_is_exact_and_applies_with_raw_feedback\('),
                          ("tests/test_nary_csp_prefix_public.cpp",
                           r'c_v8_deep_copies_and_validates_the_exact_selection\(')]),
    "Condition":        (EXPLICIT, "typed (clause,cube) API; no default producer",
                         [EV_CONDITION_SKIP],
                         [("src/rules/condition/condition.cpp",
                           EV_CONDITION_NOOP),
                          ("src/api/api.cpp", r'int tactix_apply_condition\('),
                          ("tests/test_api_c.c", r'test_condition_cube_api\('),
                          ("tests/condition_certificate_driver.c",
                           r'tactix_apply_condition\(')]),
}

# The slot's registry admission predicate. Explicit-only exclusions are checked
# separately; if this predicate disappears, re-derive the scheduled set.
SLOT_PREDICATE = r"rules\[i\]\.tier == RuleTier::Inprocess"
PROGRAM_REL = "src/strategy/inprocess_program.hpp"


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

    if re.search(r"e\.closure\s*=\s*true\s*;", src):
        fails.append("the slot builder again defaults an unmetered entry to the "
                     "generic closure/new-material gate; production gates must "
                     "be reference-authored per rule, not inferred from the "
                     "absence of an EFFORT_SPECS row")

    if not re.search(SLOT_PREDICATE, src):
        fails.append(f"the slot's admission predicate `{SLOT_PREDICATE}` is gone "
                     f"from {POLICY_REL}. This checker classifies exactly the "
                     f"Inprocess-tier rules BECAUSE the append path uses that tier; "
                     f"if that changed, re-derive the scheduled set here before "
                     f"trusting a green run.")

    inprocess_rules = sorted(n for n, (tier, _) in rules.items()
                             if tier == "Inprocess")

    # Every CLI-reachable rule needs a CLI reach counter. An explicit-only
    # rule cannot fire from that harness; its typed public surface and fixed
    # API driver are checked by the EXPLICIT row instead. Broad API fuzz remains
    # an independent acceptance gate, never inferred from this static check.
    reach_src = open(os.path.join(ROOT, "tools/fuzz/fuzz.py")).read()
    reach = set(re.findall(r'^\s*\("([A-Za-z]+)",\s*r"', reach_src, re.M))
    for n in sorted(rules):
        if n not in reach and not (n in TABLE and TABLE[n][0] == EXPLICIT):
            fails.append(
                f"registered rule `{n}` ({rules[n][1]}) has no REACH_SPECS entry "
                f"in tools/fuzz/fuzz.py, so no fuzz leg can be REQUIRED to reach "
                f"it and a green leg says nothing about it. Add the counter that "
                f"proves it fired — and key it on a counter the DOSED path moves, "
                f"not a round counter apply_instance never touches.")

    for n in inprocess_rules:
        if n not in TABLE:
            fails.append(f"UNCLASSIFIED Inprocess rule `{n}` ({rules[n][1]}) — "
                         f"give it a METERED, ALTERNATE-GOVERNOR or evidenced "
                         f"EXPLICIT-ONLY row in {os.path.relpath(__file__, ROOT)}.")

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
        elif kind == ALT:
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
        elif kind == EXPLICIT:  # No enabled slot or canonical work to meter.
            if n in specs:
                fails.append(f"explicit-only `{n}` unexpectedly has an effort "
                             "row; it may now be scheduled")
            missing = [e for e in row[2] if not re.search(e, src)]
            for rel, pattern in row[3]:
                content = squash(strip_comments(
                    open(os.path.join(ROOT, rel)).read()))
                if not re.search(pattern, content):
                    missing.append(f"{rel}: {pattern}")
            program = squash(strip_comments(
                open(os.path.join(ROOT, PROGRAM_REL)).read()))
            body = re.search(
                r"reference_inprocess_program\(\)\s*\{(.*?)return result;",
                program)
            if not body:
                missing.append(f"{PROGRAM_REL}: default program cannot be inspected")
            else:
                named = re.search(r'"' + re.escape(n) + r'"', body.group(1))
                disabled = re.search(
                    r'\{\s*\d+\s*,\s*"' + re.escape(n) +
                    r'"\s*,\s*\w+\s*,\s*false\s*\}', body.group(1))
                if named and not disabled:
                    missing.append(f"{PROGRAM_REL}: {n} is not explicitly disabled")
            if missing:
                fails.append(f"explicit-only `{n}` lost omission/NoOp/API "
                             f"evidence: {missing}")
            else:
                notes.append(f"EXPLICIT {n:16} <- {why}")
        else:
            fails.append(f"`{n}` has unknown classification `{kind}`")

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
    n_explicit = sum(1 for r in TABLE.values() if r[0] == EXPLICIT)
    print(f"check-effort-specs: {len(inprocess_rules)} registered Inprocess rules "
          f"({len(inprocess_rules) - n_explicit} default-scheduled, "
          f"{n_explicit} explicit-only), "
          f"{len(specs)} EFFORT_SPECS rows, {n_alt} alternate-governor, "
          f"{len(fails)} failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
