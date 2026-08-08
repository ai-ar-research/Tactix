# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Plan revision:** 97 (documentation ownership and budgets; no gate change).
- **Current phase:** 4 — faithful reference dosing, optimization, and scheduling.
- **Exit:** T3 + N4 parity ledger discharged + one full Tier D campaign.
- **Non-negotiables:** N1 API universality ✓ · N2 proofs/models ✓ after the
  backward-BVE unit fix · N3 Tier-D parity not yet measured · N4 scheduling
  parity not yet discharged.
- **Last updated:** 2026-08-08.

## Resume here

The wrong-SAT defect in `bve_backward` is fixed and covered by a kernel
regression test; model checking is now part of tier scoring. Evidence and the
full diagnosis are in commit `6033378` and its preceding fix/instrument commits.
Every elimination-era performance number from the unsound binary remains void.

The fixed-binary Tier B retake `20260808-094512-B-p3fix` (manifest SHA
`eece089`) is complete: **p3fix 28/28, PAR-2 49.94; Kissat 28/28, 10.07;
CaDiCaL 27/28, 40.76**. Tier C ablation window `20260808-094434-C-ablfix` is
still RUNNING its last seven tasks (array `1591547` indices
126–129/168–169/171, the long tail; aggregate `1591640` pending on the
dependency) — do NOT resubmit anything; it completes on its own.

**Next actions:**

1. **Fidelity first (user directive 2026-08-08): audit each of the three ports
   against its reference source and repair every implementation gap or missing
   coupled mechanism on its own evidence — tier results are measurement, never
   the selector of what gets repaired.** Primary goal: match the reference
   policy; secondary: performance. Three port audits (BVE-backward/elim vs
   cadical, Factor coupling vs kissat, chrono reuse vs cadical) are in flight;
   one gap is already CONFIRMED from the wrong-SAT investigation: both
   references flush/garbage redundant clauses that hold an eliminated
   variable (cadical `mark_redundant_clauses_with_eliminated_variables_as_
   garbage`); ours has no analogue, so a retired variable stays propagatable.
2. When `20260808-094434-C-ablfix` completes, read it as measurement of the
   fixed state (`all` vs `no-bwd` vs `no-couple` vs `no-chrono` + baselines);
   report and update the N4 ledger.
3. Continue the unresolved Phase 4 parity rows, then run the Tier D shakedown,
   Tier D campaign, and T3 gate in that order.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
are priced on Tier C because Tier B is dominated by four unstable instances.

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

- [x] `SP-BVE-BWD-1`, `SP-FACTOR-1`, `SP-CHRONO-1`, and `SP-SWEEP-2` are
  implemented; the fixed-binary attribution is still required for the first
  three.
- [ ] `SP-COMPACT-1`: port reference variable renumbering when inactive
  variables exceed 10%; current GC compacts watches but leaves retired variables
  paying kernel cost.
- [ ] `SP-VIVIFY-3`: port CaDiCaL's occurrence-sorted schedule and prefix dedup
  before spending propagations on duplicate/subsumed candidates.
- [ ] `SP-SWEEP-3`: persist the ranked untried tail across calls and grow
  environment caps only after a completed pass; a bare wrapping cursor measured
  worse.
- [ ] `SP-SWEEP-7`: replace seed-only comparisons with a partition over all
  environment literals plus the model-flip prefilter; this is rule work.
- [ ] `SCHED-G6`: charge the binary-only scan for entries actually touched;
  evaluate by A/B because changing the meter changes search.
- [ ] `SCHED-G7`: replace the global binary-first flag with per-list state while
  preserving it through probing snapshot/restore.
- [ ] `SP-CONG-BUDGET-1`: resolve Congruence's deliberately stricter local
  budget against its measured cost/yield gap.
- [ ] `SP-SUBST-2`: add Kissat's two-round 10‰ hand-rolled budget without our
  generic 1e7 floor, which would make the port inert.
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

## Recent changes

- 2026-08-08 — fixed backward-BVE unit loss, added regression/reach checks and
  tier SAT-model validation; fixed-binary retakes started (`6033378`).
- 2026-08-07 — completed the phase-3 rule inventory and opened reference-faithful
  Phase 4 (`benchmarks/reports/2026-08-07-phase-3-gate.md`).
- 2026-08-07 — replaced local knob pricing with the five-layer reference dosing
  port (`docs/design/2026-08-07-reference-dosing.md`).
