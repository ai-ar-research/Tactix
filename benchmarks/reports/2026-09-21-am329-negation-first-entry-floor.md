# AM326--AM329: owner negation fixing, generic-entry floor, and an unsound intermediate pin

Checkpoint 21 September 2026 UTC. Base commit `abc527fa`. First Phase 3 (WV2-89)
increment. Authorities are the campaign directories named below; all solver work ran
on `cpu-dense-hwulab-q` with 12 CPUs, 32 GiB, a 1,200 s solve limit and a 57,600 s
checker window, certificates in anonymous node-local scratch. Frozen checkers:
dsr-trim `3b5c7737`, Trestle `19bf79f4`, reference dsr-compress `a44102ce`.

## Pins

| Pin | Release SHA-256 | Debug SHA-256 | Build job | Status |
|---|---|---|---|---|
| AM327 | `7da2b238...` | `77bead72...` | `1711473` | **unsound, withdrawn** |
| AM329 | `74eeb53b...` (two independent builds byte-identical) | `9a7165ba...` | `1711778` | candidate |

## Changes (AM329 relative to `abc527fa`)

1. **Negation fixing before fix-and-cut.** Satsuma's `negation_fixing_schreier`
   fixes every variable whose stabilizer orbit holds its own complement before its
   fix-and-cut pass. The FixAndCut planner now does the same (typed
   `GraphSymmetryParameters::negation_fixing_first`, backend
   `GroupSettings::negation_fixing_first`, public `symmetry_negation_fixing_first_mode`,
   random-policy schema `sr29`). The base point of such a level is the variable's
   negative literal, as in the owner, and materializes as a kind-3 negation-orbit unit.
2. **Owner cut point, typed and declined.** `find_non_trivial_orbit` (largest
   stabilizer orbit, highest variable, strictly better common-clause score) is
   selectable as `SymmetryCutPoint::LargestOrbitLast` and carried by the transliterated
   reference profile. DefaultPolicy declines it and the unconditional handoff on the
   evidence in `benchmarks/runs/20260921-wv2-satsuma-order-ablation` (cut point loses
   `ntil-90d-37/40` and doubles `noL-11-0/8`; handoff loses `clqcl_30_11_10` and both
   `ntil` rows).
3. **Generic-entry floor.** Under progress scheduling the generic graph producer's
   entry opportunity admits at least one generator whenever the hard allowance
   affords it. On `xor_op_n40_d3` one validation quantum (6.2M ticks) exceeded the
   16 Mi window, so nothing was admitted and the owner's negation units were never
   found.
4. **Debug cross-check of producer certificates.** Debug builds assert that every
   chain- or orbitopal-certified X7 instance passes full `validate_semantics` on the
   live formula (the unique-literal certificate is excluded: its implicit witness is
   never parsed). Release is unaffected.

## The AM327 unsoundness

AM327 passed all four gates with every certificate verified, and then failed the
focused fuzz legs: 209 cases in `rel-default-focused`, 115 in `dbg-default-focused`,
115 and 123 in the AVX2 lookup legs (families `sc-unsat`, `sc-drop`, `lk-sat`; "dsr-trim did
not verify" and "solver says UNSAT, oracle says SAT"). Smallest reproducer: 5
variables, 21 clauses, now corpus entry
`tools/fuzz/corpus/regressions/sr-negation-unit-planner-value`.

Cause: the new negation level recorded `values[base] = false` in the planner while
the published kind-3 unit sets the base true. A later clausal-fix level then read a
clause satisfied by that unit as an open pair and emitted a fix unit that is not a
consequence. Chain rows carry `symmetry_chain_certificate`, which skips semantic
validation, and clausal-fix units cross the backend boundary without any structural
check, so nothing downstream rejected it. Real benchmarks rarely combine a negation
orbit with a later fix on a clause containing the same variable; tiny symmetric
formulas do. **Green gates are not soundness evidence for a symmetry-planner edit.**

Repair: the planner assigns the negation level's base point true, exactly as
published; backend regression `negation_unit_value_matches_the_published_unit` pins
the sign against the planner's clause view; change 4 makes any future mismatch abort
in the Debug fuzz legs at its source. The remaining AM327 jobs were cancelled by
explicit ID; its campaign keeps `NOTE-unsound-pin.txt`. AM327 gate numbers are void.

## AM329 evidence

Build/verify job `1711778`: 43-entry corpus 0 failures in Release and Debug under
recorded arguments and random policy; all 324 AM327 default-policy focused failures
pass (209 Release, 115 Debug); full CTest 169/169 Release and 168/168 Debug (job `1712082`;
`demo3_scripted_default` excluded in Debug, 52 min) after isolating one scheduling
fixture that adds a clause after structured startup has fixed free variables; proof vs no-proof search identical on 97 runs, 0
different, 7 skipped on timeout; `xor_op_n40_d3` UNSAT in 131.9 s, 59.5 MB proof
verified with the every-lemma audit in 1,521 s; `bphp_p23_h22` UNSAT in 487.9 s.

Fuzz matrix `benchmarks/runs/20260921-am329-negation-unit-sign-claude-fuzz` (jobs
`1711779`--`1711796`): failure-seeds PASS, controls OK, and 0 failures in every
leg: eight nightly legs (400,030 cases), `dbg-default` 50,000,
`dbg-random` 50,000 (44,463 verdicts), `rel-default-focused` 42,000, `rel-random-focused`
27,500, `dbg-default-focused` 21,000, `dbg-random-focused` 14,000, both AVX2 lookup legs
9,000 each. All 18 jobs exited 0 with the 133 pinned files verified before and after.

Gates `benchmarks/runs/20260921-am329-negation-unit-sign-{rank1-50,b28c5,wv100,wv2-89}`
(jobs `1711797`--`1711800`), against the committed binary's campaigns:

| Gate | Solved | Time on rows solved by both | Lost | Conflict-count changes |
|---|---|---|---|---|
| Rank 1--50 | 50/50 | 7,659.6 s vs 7,668.9 s | none | 0 |
| B28+C5 | 33/33, all valid | 873.9 s vs 869.0 s | none | 1 |
| WV100 | 100/100 | 2,321.5 s vs 2,342.5 s | none | 1 (`xor_op_n36_d3` 87 s -> 74 s) |
| WV2-89 | 79 solved, 74 within 600 s (89/89 reported) | 6,535.3 s vs 6,486.1 s | none | 1 |

WV2-89 gains `bphp_p23_h22` (486.7 s) and `xor_op_n40_d3` (130.8 s) over the 72-row
baseline. Fifteen rows still miss 600 s: five are solved late (`sgen1-sat` 969 s,
`FmlaEquivChain` 751 s, `6g_6color` 741 s, `Circuit_multiplier24` 688 s, `snw_16_9` 614 s)
and ten time out. No solved row is invalid; large certificates were still in the checker at
this checkpoint (3 Rank, 4 WV100, WV2-89 tail) and do not gate the solve comparison.

## Open WV2-89 rows and diagnosis so far

`benchmarks/runs/20260921-wv2-satsuma-gap-survey`: on Satsuma's preprocessed formula
Tactix solves `6g_6color` (21 s), `hcp_CP18_18` (127 s), `ramsey_4_4_19` (288 s) and
`mp1-Nb7T42` (182 s); `SC25_Timetable` still times out. `mp1` receives the same single
negation unit from Tactix as from Satsuma, so its gap is search. On `ramsey` both
produce the same negation unit and seven clausal-fix units; they diverge afterwards
because the owner's `orbit_overlaps_clause` anchors on a clause's first stored literal
(a falsified anchor has orbit size one, so the clause is cut rather than fixed) while
Tactix skips falsified literals anywhere. A same-window A/B of that anchor rule
(`benchmarks/runs/20260921-wv2-owner-anchor`, job `1712075`) is negative: both arms time
out at 1,200 s with about 25M conflicts, so the anchor alone does not explain the gap. The large-domain edits held in
`benchmarks/runs/20260921-am328-linear-pass-window` showed no gain and stay unapplied.
LymphoSAT structural families (`sgen1-sat`, `sted5`, `1-ET-512`, `FmlaEquivChain`,
`noL-11-4`, `puzzle32`) and plain CDCL parity rows (`snw_16_9`, `Circuit_multiplier24`,
`ntil-90d-39`, `lockchart...L19`) are untouched.
