# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — faithful reference dosing, optimization, and scheduling.
- **Exit:** T3 + N4 parity ledger discharged + one full Tier D campaign.
- **Non-negotiables:** N1 API universality ✓ · N2 **restored** (frozen-pivot
  fix `7fbcb11`; 40k-case battery green with real backward-BVE reach; retake
  pair N2-clean) · N3 Tier-D parity not yet measured · N4 scheduling parity
  not yet discharged.
- **Last updated:** 2026-08-08.

## Resume here

The codebase revisit and `SP-COMPACT-1` implementation are complete.
`DefaultPolicy` owns CDCL/search strategy and composes
`DefaultInprocessScheduler`; formula rules remain policy-independent and the
`rule_policy_boundary` check enforces that separation. The N2 frozen-pivot fix
and its clean retakes remain recorded in
`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`.

Variable identities are now permanent at every external boundary while search
uses dense internal indices. The kernel provides a formula-neutral compaction
primitive; `DefaultPolicy` invokes it after Reduce when retired variables exceed
kissat's 10% threshold. Focused tests cover mapping, proof/model/reconstruction,
live reasons, extension variables, `H`, and stable scheduler cursors. Final pin
`e1d35b8` is green in the full Release suite and focused assertion/sanitizer
suite. Closing benchmark windows are `20260808-221957-A-compact2`,
`20260808-222138-B-compact2`, and `20260808-222726-C-compact2`.
Final LRAT fuzz job 1594313 passed 10,000 cases with zero failures; source probe
1594316 observed 3 compactions removing 2,796 internal variables.

**Next actions:**

1. Resume the Kissat-default parity campaign through the extracted scheduler;
   take one remaining N4 row at a time, starting with `SP-SWEEP-7` unless a
   different row is explicitly selected.
2. Then run the Tier D shakedown, Tier D campaign, and T3 gate in that order.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
are priced on Tier C because Tier B is dominated by four unstable instances.
Feature acceptance is cumulative: an accepted change remains enabled while the
next change is evaluated against the preceding accepted pin. Single-feature off
arms are diagnostic; they do not replace the cumulative comparison. Incremental
campaigns run only those two Tactix pins. Kissat and CaDiCaL are rerun at gates,
or when the manifest or allocation shape changes, rather than on every step.

## Gates

- [x] **T1:** Tier A and MiniSat bar (`benchmarks/reports/2026-07-14-t1-gate.md`).
- [x] **T2:** Glucose/API bar (`benchmarks/reports/2026-07-14-t2-gate.md`).
- [ ] **T3:** ≤1.10× CaDiCaL on Tier C and parity with CaDiCaL/Kissat on the
  rule-neutral subset; retake required after the N2 fix.
- [ ] **T4:** DefaultPolicy engineering exceeds Kissat on the SC25 Tier-D view.
- [ ] **T5:** beat AE-Kissat-MAB or ship best effort.

## Phases 0–3 — complete

- [x] Setup, kernel, proof pipeline, competition I/O, API freeze, and full
  reference rule-inventory disposition.
- [x] Phase 3 rule-completeness gate, including reach evidence or explicit
  no-implement disposition (`benchmarks/reports/2026-08-07-phase-3-gate.md`).

## Phase 4 — dosing, optimization, scheduling

The baseline is the faithful five-layer reference scheme: phase intervals,
per-technique effort fractions in native currencies, structural candidate caps,
success-driven delay, and fixpoint watermarks. Reference ports must demonstrate
their effect with source counters before performance interpretation.

- [x] Port phase-anchored effort, reference phase structure/repeats, density-gate
  removal, shared forward allowance, tick units, delay/yield ladder, Factor/BVE
  coupling, backward BVE, and analyze-side trail reuse.
- [x] Fix backward BVE strengthened-to-unit handling and add independent SAT
  model checks (`6033378`).
- [x] Fix frozen-at-0 retired variables voiding model reconstruction
  (`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`).
- [ ] Complete and report the fixed-binary Tier C four-arm attribution.
- [ ] Close or accept the remaining N4 rows below.
- [ ] Run a Tier D shakedown, then the 793-instance campaign.
- [ ] Evaluate and report T3 last.

<!-- track-sched:begin -->
### Remaining N4 parity ledger

Each item below is a resume handle, not its specification. Search the exact id
in `docs/implementation-notes.md` §13.2–§13.4 for the reference mechanism,
current implementation, evidence, and closer; no Git-history excavation is
required.

- [x] `SP-SWEEP-2` is implemented and closed.
- [x] `SP-BVE-BWD-1`, `SP-CHRONO-1`: closed — repairs landed, accepted
  deviations recorded, closing pair banked N2-clean
  (`20260808-141938-B-n2fix` / `20260808-141941-C-n2fix`, commit `7fbcb11`).
- [x] `SP-FACTOR-1`: closed — the N2-clean closing pair is banked and
  `test_slot_pose` proves the level and Factor snapshot walk an injected
  irregular `BveBoundPolicy` ladder through its terminal phase.
- [x] `SP-COMPACT-1`: closed at `e1d35b8` — permanent external identities,
  dense internal renumbering after Reduce above kissat's 10% inactive threshold,
  and formula-neutral kernel/policy separation.
- [x] `SP-VIVIFY-3`: CaDiCaL's band-local prefix flush is implemented with a
  pure per-clause surface and defaults on by project policy despite its recorded
  robust Tier C regression
  (`20260808-234549-A-vivflush`, `20260808-234549-B-vivflush`,
  `20260808-234836-C-vivflush`;
  `benchmarks/reports/2026-08-09-sp-vivify-3.md`).
- [x] `SP-SWEEP-3`: Kissat's eligible ranked tail and completed-pass cap growth
  are implemented and default on by project policy despite losing solves on
  both Tier B and Tier C
  (`20260809-010103-A-sweepresume-on`, `20260809-010105-B-sweepresume-on`,
  `20260809-010110-C-sweepresume-on`;
  `benchmarks/reports/2026-08-09-sp-sweep-3.md`).
- [x] `SP-SWEEP-7`: the default Sweep detector partitions all environment
  literals and applies the model-flip prefilter; `TACTIX_SWEEP_PARTITION=0`
  retains the seed-only attribution arm.
- [x] `SCHED-G6`: binary-only propagation charges the cache lines of the
  actually scanned prefix, matching Kissat's `touched` meter. The focused
  binary-first-tail test and full suite are green; cumulative A/B/C pricing is
  the next-pin comparison.
- [x] `SCHED-G7`: binary-first state is per watch list. Binary insertion
  invalidates only affected lists; rebuilds reconstruct it, and probing
  snapshot/restore includes the metadata.
- [x] `SP-CONG-BUDGET-1`: accepted measured deviation. Congruence keeps its
  stricter local budget because its current round is about 20x the reference
  gate volume; the ledger records the cost/yield evidence and removal test.
- [x] `SP-SUBST-2`: Substitute uses its own binary-watch tick meter and
  Kissat's per-round 10‰ allowance without the generic 1e7 floor; an
  over-budget first round suppresses the phase's second and final round.
- [ ] `SP-ARENA-1`: make budget-cut candidate order search-coupled instead of
  clause-creation order.
- [ ] `SP-PREPROC-2`: replace three full-list preprocessing rounds and the 1e7
  floor with the references' reduced, bounded pipeline.
- [ ] `SP-BUMP-2`: evaluate CaDiCaL's stable-mode reason-bump depth of two versus
  our unconditional depth one.
- [ ] `SP-SUBSUME-3`: split the connection cap for long (1e2) and binary (1e4)
  populations instead of using 1000 for both.

Each row closes with an implementation or an accepted deviation in
`docs/implementation-notes.md` carrying measurement and a reopening path.
<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity beyond reference parity

- [ ] Add debug proof-event correlation: record the emitting rule/application
  and proof line or byte range so checker failures localize without inference.
- [ ] Concretize O15 through `revise-plan` when phase 4 exits.

## Phase 5 — tuning, bindings, extensibility

- [ ] Decide O16; add policy presets/MAB, Python bindings, external-branching
  demos, ≤5% hardcoded-twin check, and CCE extensibility gate.
- [ ] Exit on T4.

## Phase 6 — competition readiness

- [ ] Package, audit licenses/attribution, rehearse the proof pipeline, and
  decide O7/O8. Exit on T5 or best effort.

## Phase 7 — learned scheduling

- [ ] Replay/reward validation; RL1 windowed UCB; random-policy fuzz inclusion;
  L-safe; contextual RL2; composed-batch RL3; anytime test; L-win.

## Open items

- `Reorder` remains off although Kissat enables it; changing the shipped default
  requires the user's decision.
- `HyperBinaryRes` remains an accepted standalone deviation with a policy-side
  structural cap.
- Diagnose random-policy nontermination; it is an N1 robustness issue but does
  not gate the already-closed phase 3.
- **Sweep ships — user directive, non-negotiable:** neither disabling Sweep nor
  shrinking its budget until inert counts as a fix; after any governor change it
  must still fire and find equivalences.
- Per-call O(nvars) scratch in hot rules (`RV-S19`): five fill-init sites in
  BVE/probe/backbone; fix is persistent scratch + touched restore, gated on
  identical rule counters and proof SHA.
- `main.cpp` split (`RV-S29`); while there, fix `c strengthen` printed twice
  under two different schemas.
- Stress-policy coverage: 10 of 25 rules have no targeted stress leg, including
  `Reorder`.
- No direct test for `src/frontend/parse.cpp`, and cake_lpr is not wired into
  the fuzz harness (`RV-S24` remainder).

## Recent changes

- 2026-08-08 — extracted `DefaultInprocessScheduler`, removed the last macro
  rule→policy leaks through rule-owned selection/events, and compacted verbose
  source commentary (`fd5f0f5`; identity/fuzz/Tier B gates green).
- 2026-08-08 — closed `SP-FACTOR-1` with the injected posed-phase ladder ctest.
- 2026-08-08 — fixed the N2 frozen-at-0 reconstruction defect behind the three
  invalid closing-pair models; made backward BVE fuzz-reachable
  (`benchmarks/reports/2026-08-08-n2-frozen-pivot.md`).
- 2026-08-08 — rewrote all 17 Claude skills, retired manual plan revisions,
  added generic JSON stress scenarios, CMake discovery, SLURM waiting, and the
  2026 competition wrapper/compliance checks.
- 2026-08-08 — made the proof-rule guide concrete for M3/C3, V3/V5/V6,
  E1, and X4/X6: indexed constructions, ordered certificates, model witnesses,
  and realizable worked chains now mirror `docs/calculus.md` and the emitters.
