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

    POLICY   the scheduler selects it; direct or named typed wiring is required.
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
# Scoped typed invocations are policy writers even though their save/install/
# restore mechanics live in the strategy layer. Keep these exceptions named so
# broad strategy code cannot accidentally satisfy a POLICY row.
POLICY_ADAPTERS = {
    "bve_fastel_traversal": ["src/strategy/bve_invocation.hpp"],
    **{field: ["src/strategy/work_execution.hpp"] for field in (
        "work_budget", "work_budget_offset", "work_budget_bounded",
        "midround_budget", "touch_filter", "touch_redundant")},
}

# A shared setter alone must not certify scheduling. Bind each moved wire to
# the actual policy request and its installation, even if a direct reset would
# otherwise satisfy writers(). Keep these paths and expressions explicit.
WORK_EXECUTION = "src/strategy/work_execution.hpp"
VIVIFY_EXECUTION = "src/strategy/vivify_execution.hpp"
VIVIFY_BAND = (r"execution::VivifyBandRequest\{[^{}]*"
               r"e\.meter\(k\) - dose_start")
MATERIAL_REQUEST = (r"execution::MaterialSettingsRequest\{\s*"
                    r"config\(\)\.touch_filter,\s*"
                    r"config\(\)\.likely_kept_redundant")
POLICY_BINDINGS = {
    "work_budget": [
        (POLICY[0], r"execution::WorkSettingsRequest\{\s*handed,"),
        (WORK_EXECUTION, r"k\.work_budget\s*=\s*\*request\.budget"),
    ],
    "work_budget_offset": [
        (POLICY[0], VIVIFY_BAND),
        (VIVIFY_EXECUTION, r"WorkSettingsRequest\{request\.cumulative_budget,\s*"
                          r"request\.spent_offset"),
        (WORK_EXECUTION, r"k\.work_budget_offset\s*=\s*\*request\.offset"),
    ],
    "work_budget_bounded": [
        (POLICY[0], r"execution::VivifyDoseScope dose\(k\)"),
        (VIVIFY_EXECUTION, r"work_\(k, \{k\.work_budget, k\.work_budget_offset, true,"),
        (WORK_EXECUTION, r"k\.work_budget_bounded\s*=\s*\*request\.bounded"),
    ],
    "midround_budget": [
        (POLICY[0], r"execution::WorkSettingsRequest\{\s*handed,\s*"
                    r"std::nullopt,\s*std::nullopt,\s*"
                    r"config\(\)\.forward_midround_budget"),
        (WORK_EXECUTION, r"k\.midround_budget\s*=\s*\*request\.midround"),
    ],
    "touch_filter": [
        (POLICY[0], MATERIAL_REQUEST),
        (WORK_EXECUTION, r"k\.touch_filter\s*=\s*\*request\.touch_filter"),
    ],
    "touch_redundant": [
        (POLICY[0], MATERIAL_REQUEST),
        (WORK_EXECUTION, r"k\.touch_redundant\s*=\s*\*request\.likely_kept_redundant"),
    ],
}


def adapter_binding_errors(sources):
    failures = []
    for field, bindings in POLICY_BINDINGS.items():
        for path, pattern in bindings:
            # Comments cannot provide request/installation evidence.
            code = re.sub(r"//[^\n]*|/\*.*?\*/", "", sources[path], flags=re.S)
            if not re.search(pattern, code, re.S):
                failures.append(f"UNWIRED: `{field}` lacks its named typed "
                                f"request/installation binding in {path}")
    return failures


def binding_sources():
    return {path: open(os.path.join(ROOT, path)).read()
            for bindings in POLICY_BINDINGS.values() for path, _ in bindings}


def binding_self_test():
    sources = binding_sources()
    assert not adapter_binding_errors(sources)
    count = 0
    for field, bindings in POLICY_BINDINGS.items():
        for path, pattern in bindings:
            changed = dict(sources)
            # Remove all copies so a second band cannot mask a missing wire.
            changed[path], replacements = re.subn(pattern, "", sources[path], flags=re.S)
            assert replacements
            assert any(f"`{field}`" in failure
                       for failure in adapter_binding_errors(changed))
            count += 1
    print(f"check-config-wiring self-test: {count}/{count} missing-request/"
          "installation controls rejected")

# The config surface, classified. Everything from `bve_bound` down in
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
    "bve_occurrence_limit":    ("POLICY", "named DefaultStrategyConfig cap installed "
                                          "at each inprocessing phase"),
    "bve_clause_size_limit":   ("POLICY", "named DefaultStrategyConfig antecedent "
                                          "cap installed at each inprocessing phase"),
    "bve_xor_arity_limit":     ("POLICY", "typed BVE XOR recognition arity installed "
                                          "at each inprocessing phase"),
    "bve_resolvent_size_limit": ("POLICY", "typed BVE resolvent-size admission limit"),
    "bve_resolvent_count_limit": ("POLICY", "typed BVE absolute resolvent-count admission limit"),
    "bve_fastel_traversal":      ("POLICY", "typed BVE physical traversal selected by the owning fast-elimination schedule"),
    "bve_backward_occurrence_limit":
                                ("POLICY", "typed backward elimination occurrence limit"),
    "bve_definition_session":  ("POLICY", "embedded Kitten create/release lifetime; "
                                           "advanced once per BVE slot"),
    "factor_composite":        ("POLICY", "S2b composite grids, per-occasion"),
    "work_budget":             ("POLICY", "per-round budget handed to budget-aware rules"),
    "work_budget_offset":      ("POLICY", "work already charged against a cumulative "
                                          "Vivify tier ceiling"),
    "work_budget_bounded":     ("POLICY", "distinguishes a zero cumulative Vivify "
                                          "ceiling from an unbounded rule call"),
    "vivify_scope":            ("POLICY", "which clause tier Vivify may touch; SCHED-G3 "
                                          "splits the redundant side into kissat's "
                                          "tier1/tier2/tier3 (C10 hand-off H3)"),
    "vivify_irredundant_sort_ratio":
                                ("POLICY", "typed irredundant Vivify population ratio"),
    "bump_reason_limit":       ("POLICY", "reason-side bumping rate"),
    "bump_reason_depth":       ("POLICY", "reason traversal depth; the shipped config is a "
                                          "constant 1 in BOTH modes, reset after analysis"),
    "inprocessing":            ("POLICY", "phase bracket; routes a tick away from "
                                          "stats.search_ticks (kissat gets the same "
                                          "split from the call site)"),
    "jump_reasons_bigbig":     ("POLICY", "Kissat formula classification refreshed "
                                          "at search/reduce/probe/eliminate boundaries"),
    "conflict_reuse_chrono_levels":
                                ("POLICY", "configured chronological-gap limit "
                                           "for the conflict-reuse shortcut"),
    # --- a rule computes these -------------------------------------------
    "tier1":                   ("RULE", "reduce.cpp, from the live glue distribution"),
    "tier2":                   ("RULE", "reduce.cpp; 2 consumers (reduce, vivify) — SP-TIER-2 "
                                        "audited cadical's 8 down to 2 live mechanisms"),
    "kept_glue":               ("RULE", "reduce.cpp survivor envelope (cadical lim.keptglue)"),
    "kept_size":               ("RULE", "reduce.cpp survivor envelope (cadical lim.keptsize)"),
    "phase_saving_suppressed": ("RULE", "Lucky probe bracket; mirrors kissat "
                                         "solver->probing for assign-time phases"),
    "factor_literals_marked":  ("RULE", "cumulative Kissat factor-bit zero-to-one "
                                        "counter maintained by clause-producing rules"),
    "factor_completed":        ("RULE", "Factor reports whether its current schedule "
                                        "drained before policy advances the watermark"),
    # --- A/B or diagnostic knobs; shipped value is the kernel.hpp default --
    "bve_gate_mask":           ("ENV", "--bve-gates"),
    "bve_backward":            ("POLICY", "typed BVE backward-tail setting"),
    "bce_occ_limit":           ("POLICY", "typed BCE occurrence limit"),
    "midround_budget":         ("POLICY", "typed default forward scheduling and "
                                           "per-invocation bounded rule-work pose"),
    "factor_size":             ("POLICY", "typed Factor clause-size limit"),
    "transitive_early_exit":   ("POLICY", "typed TransitiveReduce stop setting"),
    "transitive_live_meter":   ("POLICY", "typed TransitiveReduce meter setting"),
    "transitive_budget_checked": ("POLICY", "typed TransitiveReduce budget currency"),
    # R10: the per-attempt sub-solver ceiling for definition extraction. NOT
    # POLICY — kissat treats `definitionticks` as a fixed option and moves
    # nothing per phase, so a writer in default_policy.* would be the fake
    # wiring this check exists to expose. Pricing it is phase-4 dosing.
    "bve_definition_budget":   ("POLICY", "typed BVE first-core Kitten budget"),
    "bve_definition_cores":    ("POLICY", "typed BVE refined-core count"),
    "bve_definition_core_budget_multiplier":
                                ("POLICY", "typed BVE refinement allowance"),
    "ternary_occ_limit":       ("POLICY", "typed HyperTernaryRes occurrence limit"),
    "ternary_max_add_percent": ("POLICY", "typed HyperTernaryRes growth limit"),
    # Default-on with an `=0` revert hatch: the touched-epoch filters are
    # rule-round policy, not scheduler policy.
    "touch_filter":            ("POLICY", "typed canonical material filter; "
                                           "TACTIX_TOUCH_MARKS is a frontend adapter"),
    # CaDiCaL's likely-kept redundant tier band ships on; =0 is the narrow arm.
    "touch_redundant":         ("POLICY", "typed likely-kept redundant band; "
                                          "TACTIX_TOUCH_REDUNDANT is a frontend adapter"),
    # Same shape as touch_filter. The probe/candidate order and the trail
    # retention are one mechanism inside the rule's round.
    "vivify_reuse":            ("POLICY", "typed Vivify trail-reuse setting"),
    "vivify_flush":            ("POLICY", "typed Vivify schedule-flush setting"),
    "sweep_tail_next":         ("RULE", "Sweep canonical schedule state"),
    "sweep_tail_indexed": ("RULE", "Sweep canonical schedule state"),
    "sweep_live": ("RULE", "Sweep canonical schedule state"),
    "sweep_passes_completed":  ("RULE", "Sweep completed-pass limit growth"),
    # Ships on via the kernel.hpp default, matching both references (kissat
    # unconditionally, cadical `factorunbump`=1), with an `=0` revert hatch.
    "factor_demote_fresh":     ("POLICY", "typed Factor fresh-variable ranking advice"),
    "factor_cand_rounds":      ("POLICY", "typed Factor candidate-pruning rounds"),
    "no_ext_reason_bump":      ("POLICY", "typed Analyze behavior; the inverse "
                                           "bump_extension_reasons setting"),
    "subst_demote":            ("POLICY", "typed Substitute ranking advice"),
    # Same shape as retire_enabled / factor_demote_fresh: ships on at kissat's
    # own default (`eagersubsume`=4), `=0` reverts. Both references treat it as
    # a fixed option, so no reference schedule moves it per phase.
    "eager_subsume":           ("POLICY", "typed Analyze eager register size; "
                                          "TACTIX_EAGER_SUBSUME is an adapter"),
    "stop_fuzz":               ("ENV", "TACTIX_STOP_FUZZ, fuzz-only abort knob"),
    "conflict_reuse":          ("POLICY", "typed Analyze conflict reuse; "
                                          "TACTIX_CONFLICT_REUSE is an adapter"),
    "jump_reasons":            ("POLICY", "typed Analyze reason jumping; "
                                          "TACTIX_JUMP_REASONS is an adapter"),
    # Both ship ON at the reference default, `=0` reverts — the transliterated
    # behaviour is the baseline and the OFF arm is what gets priced.
    "shrink_glue_promote":     ("POLICY", "typed Analyze glue promotion; "
                                          "TACTIX_SHRINK_GLUE is an adapter"),
    "tumble":                  ("ENV", "TACTIX_TUMBLE; kissat `tumble`, ships ON "
                                       "(SP-DECIDE-2), =0 reverts to ascending "
                                       "variable order"),
    # --- external-policy surface (N1) ------------------------------------
    "factor_bound":            ("POLICY", "typed Factor base bound combined with the "
                                          "DefaultPolicy BVE-bound coupling; public calls "
                                          "can scope or set it explicitly"),
    "walk_even_break_base_milli":
                                ("POLICY", "typed Walk ProbSAT break-score base"),
    "reorder_maximum_clause_size":
                                ("POLICY", "typed Reorder scan cutoff; online calls "
                                           "can scope it explicitly"),
    "reorder_clause_decay_milli":
                                ("POLICY", "typed Reorder clause-size decay"),
    "reorder_binary_weight_milli":
                                ("POLICY", "typed Reorder binary-clause weight"),
    "reorder_balanced_polarity_milli":
                                ("POLICY", "typed Reorder both-polarity multiplier"),
    "parity_model_maximum_variables":
                                ("POLICY", "typed Lucky structural admission bound"),
    "parity_model_maximum_groups":
                                ("POLICY", "typed Lucky structural memory bound"),
    "parity_model_maximum_constraints":
                                ("POLICY", "typed Lucky formula-work bound"),
    "parity_model_minimum_group_clauses":
                                ("POLICY", "typed Lucky equation admission"),
    "parity_model_minimum_equations":
                                ("POLICY", "typed Lucky equation admission"),
    "parity_model_maximum_free_variables":
                                ("POLICY", "typed Lucky affine search bound"),
    "parity_model_candidate_budget":
                                ("POLICY", "typed Lucky candidate budget"),
    "parity_refutation_maximum_variables":
                                ("POLICY", "typed Lucky XOR-refutation variable bound"),
    "parity_refutation_maximum_groups":
                                ("POLICY", "typed Lucky XOR-refutation memory bound"),
    "parity_refutation_maximum_constraints":
                                ("POLICY", "typed Lucky XOR-refutation scan bound"),
    "parity_refutation_maximum_arity":
                                ("POLICY", "typed Lucky XOR-refutation arity bound"),
    "parity_refutation_probe_constraints":
                                ("POLICY", "typed Lucky XOR feedback checkpoint"),
    "parity_refutation_minimum_probe_repeats":
                                ("POLICY", "typed Lucky XOR feedback threshold"),
    "parity_refutation_maximum_proof_additions":
                                ("POLICY", "typed Lucky XOR certificate bound"),
    "onehot_model_maximum_variables":
                                ("POLICY", "typed Lucky one-hot admission bound"),
    "onehot_model_maximum_domains":
                                ("POLICY", "typed Lucky one-hot memory bound"),
    "onehot_model_maximum_constraints":
                                ("POLICY", "typed Lucky one-hot formula-work bound"),
    "onehot_model_maximum_equation_arity":
                                ("POLICY", "typed Lucky GF(3) recognition bound"),
    "onehot_model_minimum_domains":
                                ("POLICY", "typed Lucky one-hot admission"),
    "onehot_model_minimum_equations":
                                ("POLICY", "typed Lucky GF(3) admission"),
    "onehot_model_maximum_free_domains":
                                ("POLICY", "typed Lucky GF(3) search bound"),
    "onehot_model_candidate_budget":
                                ("POLICY", "typed Lucky GF(3) candidate budget"),
    "exact_one_refutation_maximum_variables":
                                ("POLICY", "typed Lucky exact-one variable bound"),
    "exact_one_refutation_maximum_groups":
                                ("POLICY", "typed Lucky exact-one memory bound"),
    "exact_one_refutation_maximum_constraints":
                                ("POLICY", "typed Lucky exact-one scan bound"),
    "exact_one_refutation_maximum_arity":
                                ("POLICY", "typed Lucky exact-one arity bound"),
    "exact_one_refutation_maximum_proof_additions":
                                ("POLICY", "typed Lucky exact-one certificate bound"),
    "reduce_victim_numerator": ("POLICY", "typed canonical Reduce victim fraction"),
    "reduce_victim_denominator":
                                ("POLICY", "typed canonical Reduce victim fraction"),
    "reduce_protect_tiers":    ("POLICY", "typed canonical Reduce tier protection"),
    "reduce_compact_collection":
                                ("POLICY", "typed explicit Reduce collection scope"),
    "reduce_bookkeeping":       ("POLICY", "typed Reduce aging and reduction clock control"),
}

# Where the config surface begins in kernel.hpp. Everything below this marker
# that looks like a scalar with a default is config and must be classified.
CONFIG_START = "cold_state_deferred"
# Fields below the marker that are solver state, not config: the touched-epoch
# substrate's own bookkeeping (`touch_epoch`, `touch_fixed_head`) and the
# per-rule resumption watermarks (`*_touch_mark`) — the `transitive_cursor`
# category, which a policy may reset but never "configures".
NOT_CONFIG = {"cold_state_deferred", "lvl", "transitive_cursor",
              "warming",
              "strategy_epoch", "subsume_material_count",
              "fixed_external_count",
              # Trace-only accounting lives at Kernel's final ABI boundary so
              # a diagnostic test can link against the non-trace library. It
              # is compiled out of production and cannot steer scheduling.
              "diagnostic_search_propagation_ticks",
              # lifecycle state: root assignments increment it and reference
              # flush boundaries clear it; no caller configures it.
              "unflushed_root",
              # the arena's own relocation counter: whoever holds a Ref across
              # a possible collection compares it. Nothing configures it.
              "arena_generation",
              # Sweep's resumable-pass latch is written and cleared by the
              # rule's completion state machine, never configured by policy.
              "sweep_incomplete",
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
            w = writers(f, POLICY + POLICY_ADAPTERS.get(f, []))
            if not w:
                fails.append(f"UNWIRED: `{f}` is tagged POLICY ({why}) but NOTHING "
                             f"in default_policy.* or its named typed adapter writes it. This is the N4 "
                             f"failure shape — we own the mechanism and never "
                             f"connect it.")
            else:
                notes.append(f"POLICY {f:26} <- {', '.join(w[:3])}")

    fails.extend(adapter_binding_errors(binding_sources()))

    for n in notes:
        print(f"  ok    {n}")
    for f in fails:
        print(f"  FAIL  {f}")
    n_pol = sum(1 for k, _ in TABLE.values() if k == "POLICY")
    print(f"check-config-wiring: {len(declared)} config fields, {n_pol} POLICY-tagged, "
          f"{len(fails)} failure(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    if "--self-test" in sys.argv:
        binding_self_test()
    sys.exit(main())
