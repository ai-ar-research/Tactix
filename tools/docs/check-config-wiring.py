#!/usr/bin/env python3
"""SCHED-A2: every Kernel scheduling-config field must have its intended writer.

WHY THIS EXISTS. The N4 scheduling-parity audit found that the single most
common high-severity shape is not a missing mechanism but an UNWIRED one: we
own the field, it is named after the reference's mechanism, and nothing ever
writes it. That is worse than a gap, because `grep` finds the mechanism and
reports parity. `Kernel::factor_bound` is the canonical case — it IS kissat's
`reduction > bound` acceptance test, documented as such, and it sat at a static
0 for weeks because the ramp that should drive it (`bve_level_`) lives four
lines away in the policy and was never connected.

A reader cannot see that. A test can:

    POLICY   the scheduler MUST write it. No writer in default_policy.* = FAIL.
    RULE     a rule computes it; the policy may read it.
    ENV      A/B or diagnostic knob, main.cpp only. The shipped default is the
             value in kernel.hpp, so "nothing writes it" is CORRECT here.
    API      public setter for an external policy (N1), no internal writer.

The second half matters as much as the first: a NEW config field that is not in
the table fails too, so classification happens when the field is added rather
than when someone notices years later.

    tools/docs/check-config-wiring.py          report; exit 1 on any failure
"""
import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True, check=True).stdout.strip()
KERNEL_H = os.path.join(ROOT, "src", "kernel", "kernel.hpp")
POLICY = ["src/policies/default_policy.cpp", "src/policies/default_policy.hpp",
          "src/policies/subpolicy.hpp", "src/policies/subpolicy.cpp"]

# The config surface, classified. Everything from `bve_track_min_bound` down in
# kernel.hpp — the fields that steer scheduling rather than record state.
#
# A note on the ENV rows, because it is the easy thing to get wrong: these are
# NOT unwired mechanisms. Their shipped value is the default in kernel.hpp and
# the env var exists only to move it for an A/B. Tagging one POLICY would be
# claiming the scheduler ought to drive it, which for most of these is exactly
# the open question the SCHED track is measuring, not a settled requirement.
TABLE = {
    # --- the policy is supposed to drive these ---------------------------
    "bve_bound":               ("POLICY", "growth-slack ramp (BveBoundPolicy)"),
    "bve_track_min_bound":     ("POLICY", "ramp bookkeeping"),
    "factor_composite":        ("POLICY", "S2b composite grids, per-occasion"),
    "factor_admit":            ("POLICY", "SCHED-N1FIX advice; default = no restriction"),
    "work_budget":             ("POLICY", "per-round budget handed to budget-aware rules"),
    "vivify_scope":            ("POLICY", "which clause tier Vivify may touch"),
    "bump_reason_limit":       ("POLICY", "reason-side bumping rate"),
    # --- a rule computes these -------------------------------------------
    "tier1":                   ("RULE", "reduce.cpp, from the live glue distribution"),
    "tier2":                   ("RULE", "reduce.cpp; SP-TIER-2: 1 consumer here, 8 in cadical"),
    # --- A/B or diagnostic knobs; shipped value is the kernel.hpp default --
    "bve_gate_mask":           ("ENV", "--bve-gates"),
    "bce_occ_limit":           ("ENV", "TACTIX_BCE_OCC_LIMIT"),
    "midround_budget":         ("ENV", "TACTIX_MIDROUND_BUDGET (measured negative)"),
    "factor_size":             ("ENV", "TACTIX_FACTOR_SIZE"),
    "factor_scope_percent":    ("ENV", "TACTIX_FACTOR_SCOPE_PCT (broad-admission arm)"),
    "transitive_early_exit":   ("ENV", "TACTIX_V4_EARLY_EXIT"),
    "bve_cost_limit":          ("ENV", "TACTIX_BVE_COST_LIMIT"),
    "ternary_occ_limit":       ("ENV", "TACTIX_TERNARY_OCC_LIMIT"),
    "ternary_max_add_percent": ("ENV", "TACTIX_TERNARY_MAXADD"),
    "factor_demote_fresh":     ("ENV", "TACTIX_FACTOR_DEMOTE — but SP-FACTOR-2 says BOTH "
                                       "references demote unconditionally; ENV is the "
                                       "status quo, not the target"),
    "factor_cand_rounds":      ("ENV", "TACTIX_FACTOR_CAND_ROUNDS; kissat ships 2, we ship 0"),
    "no_ext_decisions":        ("ENV", "TACTIX_NO_EXT_DECISIONS"),
    "no_ext_reason_bump":      ("ENV", "TACTIX_NO_EXT_REASON_BUMP"),
    "subst_hazard_round":      ("ENV", "TACTIX_SUBST_HAZARD_ROUND"),
    "subst_demote":            ("ENV", "TACTIX_SUBST_DEMOTE"),
    "retire_enabled":          ("ENV", "TACTIX_RETIRE; ships ON via the kernel.hpp default"),
    "stop_fuzz":               ("ENV", "TACTIX_STOP_FUZZ, fuzz-only abort knob"),
    "conflict_reuse":          ("ENV", "TACTIX_CONFLICT_REUSE; ships OFF (TB4b(B))"),
    "conflict_reuse_no_repair": ("ENV", "TACTIX_CONFLICT_REUSE_NOREPAIR, diagnostic"),
    # --- external-policy surface (N1) ------------------------------------
    "factor_bound":            ("API", "kissat's `reduction > bound`. NOT policy-wired, and "
                                       "that is the N4 exhibit: the ramp exists as bve_level_ "
                                       "and is never connected. Wiring it is measured negative "
                                       "3x, so this stays API until a NEW mechanism exists — "
                                       "an accepted gap, not an oversight."),
}

# Where the config surface begins in kernel.hpp. Everything below this marker
# that looks like a scalar with a default is config and must be classified.
CONFIG_START = "bve_retry_fixed_head"
# Fields below the marker that are solver STATE, not config.
NOT_CONFIG = {"bve_retry_fixed_head", "lvl"}


def declared_fields():
    s = open(KERNEL_H).read()
    i = s.index(CONFIG_START)
    out = []
    # Handles multi-declarator lines: `uint32_t tier1 = 2, tier2 = 6;` declares
    # TWO config fields, and missing the second is exactly the silent gap this
    # script exists to prevent.
    for m in re.finditer(r"^\s{2}(bool|uint8_t|uint32_t|uint64_t)\s+([^;]+);",
                         s[i:], re.M):
        for decl in m.group(2).split(","):
            nm = re.match(r"\s*(\w+)\s*=", decl)
            if nm and nm.group(1) not in NOT_CONFIG:
                out.append(nm.group(1))
    return out


def writers(field, paths):
    hits = []
    for rel in paths:
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            continue
        for n, line in enumerate(open(p), 1):
            # assignment, not comparison; skip comments
            if re.search(r"\b" + field + r"\s*=(?!=)", line) and \
               not line.lstrip().startswith("//"):
                hits.append(f"{rel}:{n}")
    return hits


def main():
    fails, notes = [], []
    declared = declared_fields()

    for f in declared:
        if f not in TABLE:
            fails.append(f"UNCLASSIFIED config field `{f}` — add it to TABLE in "
                         f"{os.path.relpath(__file__, ROOT)} with its intended "
                         f"writer (POLICY/RULE/ENV/API). A new field must be "
                         f"classified when it is ADDED, which is the whole point.")
    for f, (kind, why) in TABLE.items():
        if f not in declared:
            fails.append(f"TABLE lists `{f}` but kernel.hpp no longer declares it "
                         f"— stale row, delete it.")
            continue
        if kind == "POLICY":
            w = writers(f, POLICY)
            if not w:
                fails.append(f"UNWIRED: `{f}` is tagged POLICY ({why}) but NOTHING "
                             f"in default_policy.* writes it. This is the N4 "
                             f"failure shape — we own the mechanism and never "
                             f"connect it.")
            else:
                notes.append(f"POLICY {f:26} <- {', '.join(w[:3])}")

    for n in notes:
        print(f"  ok    {n}")
    for f in fails:
        print(f"  FAIL  {f}")
    n_pol = sum(1 for k, _ in TABLE.values() if k == "POLICY")
    print(f"check-config-wiring: {len(declared)} config fields, {n_pol} POLICY-tagged, "
          f"{len(fails)} failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
