#!/usr/bin/env python3
"""Every Kernel scheduling-config field must have its intended writer.

The commonest high-severity shape the N4 scheduling-parity audit found is not a
missing mechanism but an unwired one: the field exists, it is named after the
reference's mechanism, and nothing ever writes it — worse than a gap, because
`grep` then finds the mechanism and reports parity. `Kernel::factor_bound` is
the canonical case: it is kissat's `reduction > bound` acceptance test,
documented as such, and it sat at a static 0 because the ramp that should drive
it (`bve_level_`) lives four lines away in the policy, unconnected.

A reader cannot see that. A test can:

    POLICY   the scheduler must write it. No writer in default_policy.* fails.
    RULE     a rule computes it; the policy may read it.
    ENV      A/B or diagnostic knob, main.cpp only. The shipped default is the
             value in kernel.hpp, so "nothing writes it" is correct here.
    API      public setter for an external policy (N1), no internal writer.

The second half matters as much as the first: a new config field missing from
the table fails too, so classification happens when the field is added.

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
# An ENV row is not an unwired mechanism: its shipped value is the kernel.hpp
# default and the env var exists only to move it for an A/B. Tagging one POLICY
# would claim the scheduler ought to drive it, and an assignment added to
# default_policy.* purely to satisfy this test is the fake wiring the check
# exists to expose. Whether a given knob should become policy-driven is what
# the scheduling-parity track measures, not a settled requirement.
TABLE = {
    # --- the policy is supposed to drive these ---------------------------
    "bve_bound":               ("POLICY", "growth-slack ramp (BveBoundPolicy)"),
    "bve_track_min_bound":     ("POLICY", "ramp bookkeeping"),
    "factor_composite":        ("POLICY", "S2b composite grids, per-occasion"),
    "factor_admit":            ("POLICY", "SCHED-N1FIX advice; default = no restriction"),
    "work_budget":             ("POLICY", "per-round budget handed to budget-aware rules"),
    "vivify_scope":            ("POLICY", "which clause tier Vivify may touch; SCHED-G3 "
                                          "splits the redundant side into kissat's "
                                          "tier1/tier2/tier3 (C10 hand-off H3)"),
    "bump_reason_limit":       ("POLICY", "reason-side bumping rate"),
    "bump_reason_depth":       ("POLICY", "stable/focused reason traversal depth"),
    "inprocessing":            ("POLICY", "phase bracket; routes a tick away from "
                                          "stats.search_ticks (kissat gets the same "
                                          "split from the call site)"),
    # --- a rule computes these -------------------------------------------
    "tier1":                   ("RULE", "reduce.cpp, from the live glue distribution"),
    "tier2":                   ("RULE", "reduce.cpp; 2 consumers (reduce, vivify) — SP-TIER-2 "
                                        "audited cadical's 8 down to 2 live mechanisms"),
    "kept_glue":               ("RULE", "reduce.cpp survivor envelope (cadical lim.keptglue)"),
    "kept_size":               ("RULE", "reduce.cpp survivor envelope (cadical lim.keptsize)"),
    "phase_saving_suppressed": ("RULE", "Lucky probe bracket; mirrors kissat "
                                         "solver->probing for assign-time phases"),
    # --- A/B or diagnostic knobs; shipped value is the kernel.hpp default --
    "bve_gate_mask":           ("ENV", "--bve-gates"),
    "bve_backward":            ("ENV", "TACTIX_BVE_BACKWARD (cadical elimbackward; ships ON)"),
    "bce_occ_limit":           ("ENV", "TACTIX_BCE_OCC_LIMIT"),
    "midround_budget":         ("ENV", "TACTIX_MIDROUND_BUDGET (measured negative)"),
    "factor_size":             ("ENV", "TACTIX_FACTOR_SIZE"),
    "transitive_early_exit":   ("ENV", "TACTIX_V4_EARLY_EXIT"),
    "transitive_live_meter":   ("ENV", "TACTIX_TRANSITIVE_LIVE_METER"),
    # R10: the per-attempt sub-solver ceiling for definition extraction. NOT
    # POLICY — kissat treats `definitionticks` as a fixed option and moves
    # nothing per phase, so a writer in default_policy.* would be the fake
    # wiring this check exists to expose. Pricing it is phase-4 dosing.
    "bve_definition_budget":   ("ENV", "TACTIX_BVE_DEF_TICKS; ships 1e4 (kissat "
                                       "`definitionticks` is 1e6 in another "
                                       "currency)"),
    "ternary_occ_limit":       ("ENV", "TACTIX_TERNARY_OCC_LIMIT"),
    "ternary_max_add_percent": ("ENV", "TACTIX_TERNARY_MAXADD"),
    # Default-on with an `=0` revert hatch: the touched-epoch filters are
    # rule-round policy, not scheduler policy.
    "touch_filter":            ("ENV", "TACTIX_TOUCH_MARKS (stage-F off arm)"),
    "subsume_holds_elim":      ("ENV", "TACTIX_SUBSUME_HOLD (SCHED-F2 off arm)"),
    # CaDiCaL's likely-kept redundant tier band ships on; =0 is the narrow arm.
    "touch_redundant":         ("ENV", "TACTIX_TOUCH_REDUNDANT; ships ON "
                                       "(SCHED-F6 parity), =0 reverts"),
    # Same shape as touch_filter. The probe/candidate order and the trail
    # retention are one mechanism inside the rule's round.
    "vivify_reuse":            ("ENV", "TACTIX_VIVIFY_REUSE; ships ON (SCHED-G3), "
                                       "=0 reverts to the per-candidate root return"),
    "vivify_flush":            ("ENV", "TACTIX_VIVIFY_FLUSH; CaDiCaL vivifyflush, "
                                       "ships OFF (SP-VIVIFY-3), =1 enables"),
    "sweep_resume":            ("ENV", "TACTIX_SWEEP_RESUME; SP-SWEEP-3 on arm"),
    "sweep_tail_next":         ("RULE", "Sweep canonical schedule state"),
    "sweep_passes_completed":  ("RULE", "Sweep completed-pass limit growth"),
    # Pure diagnostic: it makes C1 report instead of act, so it can never be a
    # shipped default and nothing in the policy may write it.
    "subsume_shadow":          ("ENV", "TACTIX_SUBSUME_SHADOW (SCHED-G2 shadow arm)"),
    # Ships on via the kernel.hpp default, matching both references (kissat
    # unconditionally, cadical `factorunbump`=1), with an `=0` revert hatch.
    "factor_demote_fresh":     ("ENV", "TACTIX_FACTOR_DEMOTE; ships ON via the kernel.hpp "
                                       "default (SP-FACTOR-2 parity), =0 reverts"),
    "factor_cand_rounds":      ("ENV", "TACTIX_FACTOR_CAND_ROUNDS; both references "
                                       "ship 2, =0 restores the unfiltered set"),
    "no_ext_decisions":        ("ENV", "TACTIX_NO_EXT_DECISIONS"),
    "no_ext_reason_bump":      ("ENV", "TACTIX_NO_EXT_REASON_BUMP"),
    "subst_hazard_round":      ("ENV", "TACTIX_SUBST_HAZARD_ROUND"),
    "subst_demote":            ("ENV", "TACTIX_SUBST_DEMOTE"),
    "retire_enabled":          ("ENV", "TACTIX_RETIRE; ships ON via the kernel.hpp default"),
    # Same shape as retire_enabled / factor_demote_fresh: ships on at kissat's
    # own default (`eagersubsume`=4), `=0` reverts. Both references treat it as
    # a fixed option, so no reference schedule moves it per phase.
    "eager_subsume":           ("ENV", "TACTIX_EAGER_SUBSUME; ships ON at kissat's "
                                       "default 4 (SP-EAGERSUB-1), =0 reverts"),
    "stop_fuzz":               ("ENV", "TACTIX_STOP_FUZZ, fuzz-only abort knob"),
    "conflict_reuse":          ("ENV", "TACTIX_CONFLICT_REUSE; ships OFF (TB4b(B))"),
    "conflict_reuse_no_repair": ("ENV", "TACTIX_CONFLICT_REUSE_NOREPAIR, diagnostic"),
    "jump_reasons":            ("ENV", "TACTIX_JUMP_REASONS; kissat `jumpreasons` "
                                       "ships ON, =0 reverts (SP-JUMP-1)"),
    # Both ship ON at the reference default, `=0` reverts — the transliterated
    # behaviour is the baseline and the OFF arm is what gets priced.
    "shrink_glue_promote":     ("ENV", "TACTIX_SHRINK_GLUE; kissat `promote`, "
                                       "ships ON (SP-SHRINK-GLUE-1), =0 reverts "
                                       "to min(glue, new_size)"),
    "tumble":                  ("ENV", "TACTIX_TUMBLE; kissat `tumble`, ships ON "
                                       "(SP-DECIDE-2), =0 reverts to ascending "
                                       "variable order"),
    # --- external-policy surface (N1) ------------------------------------
    "factor_bound":            ("API", "kissat's `reduction > bound`. NOT policy-wired, and "
                                       "that is the N4 exhibit: the ramp exists as bve_level_ "
                                       "and is never connected. Wiring it is measured negative "
                                       "3x, so this stays API until a NEW mechanism exists — "
                                       "an accepted gap, not an oversight. Ledger row + "
                                       "removal path: implementation-notes.md 13.3, "
                                       "SP-FACTOR-1."),
}

# Where the config surface begins in kernel.hpp. Everything below this marker
# that looks like a scalar with a default is config and must be classified.
CONFIG_START = "bve_retry_fixed_head"
# Fields below the marker that are solver state, not config: the touched-epoch
# substrate's own bookkeeping (`touch_epoch`, `touch_fixed_head`) and the
# per-rule resumption watermarks (`*_touch_mark`) — the `transitive_cursor`
# category, which a policy may reset but never "configures".
NOT_CONFIG = {"bve_retry_fixed_head", "lvl", "transitive_cursor",
              "warming",
              "touch_epoch", "touch_fixed_head",
              "subsume_touch_mark", "strengthen_touch_mark",
              "bce_touch_mark", "ternary_touch_mark",
              "instantiate_touch_mark",
              "cce_touch_mark",
              # rule outputs, read by the policy, never configured
              "subsume_complete", "strengthen_complete"}


def declared_fields():
    s = open(KERNEL_H).read()
    i = s.index(CONFIG_START)
    # The config surface is the tail of `struct Kernel`, so the scan stops at
    # that struct's closing brace. Without the bound, a local scalar with an
    # initializer inside a later inline kernel helper (indented by the same two
    # spaces) reads as an unclassified config field.
    j = s.index("\n};", i)
    out = []
    # Multi-declarator lines declare several fields: `uint32_t tier1 = 2,
    # tier2 = 6;` is two, and missing the second is the silent gap this script
    # exists to prevent.
    for m in re.finditer(r"^\s{2}(bool|uint8_t|uint32_t|uint64_t)\s+([^;]+);",
                         s[i:j], re.M):
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
