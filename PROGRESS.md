# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — faithful reference dosing, optimization, and scheduling.
- **Exit:** P4-CVG + N4 verification complete + T3 + one full Tier D campaign.
- **Non-negotiables:** N1 verification **pending after the documentation audit**
  (Factor and Congruence expose only canonical rounds; retired Decide instances
  need an adversarial reachability test) · N2 verification **complete** (frozen-pivot fix
  `7fbcb11`; 40k-case battery green with real backward-BVE reach; retake
  pair N2-clean) · N3 Tier-D parity not yet measured · N4 verification
  **pending** while the current causal frontier is classified.
- **Last updated:** 2026-08-09.

## Resume here

The application-order and Analyze repairs are correctness-green. Portable
Tactix/Kissat traces are identical for 286,987 records through conflict 1,000
on `tr-n300-s055`. Exact OTFS replacement reuse and Kissat's reduction
cadence/ramp now ship; all 66 current Release checks and the traced DRAT proofs
pass.
The first remaining mismatch is `REDUCE-CANDIDATE-1`: from the same 957-clause
pre-Reduce population, Tactix retains 905 redundant clauses and Kissat 633.
This is an open strength question, not an unexplained downstream watch-order
bug. The canonical evidence and cumulative trajectory are in
`benchmarks/reports/2026-08-09-tier-a-kissat-divergence-convergence.md`;
the full lifecycle matrix remains in
`benchmarks/reports/2026-08-09-default-strategy-application-order-audit.md`.
Production builds omit all trace implementation, symbols, and event strings.
The newest broad window, `20260809-144538-B-parity-old`, belongs to the
superseded pre-repair collapse and is cited for provenance, not banked.

**Next actions:**

1. Instrument the first Reduce candidate sequence with glue, used age, live
   mode-tier thresholds, reason lock, rank, and keep/delete disposition. Compare
   the first differing candidate at conflict 1,000 with Kissat's executable
   `reduce.c` path.
2. Decide whether Tactix's larger retained set is strictly better. If not, fix
   it to Kissat; if the candidate trace leaves the strength relation uncertain,
   ask for a policy decision. Rerun `tr-n260-s002` only after an actual increment.
3. Independently restore N1 evidence for Factor, Congruence, and retired Decide
   instances. Do not call N1 green until adversarial public-API enumeration
   tests cover every legal instance or the calculus explicitly defines a round
   as the indivisible instance.
4. Close the three P4A fixtures in fixed order, then the five P4B fixtures in
   fixed order. Broad Tier B/C pricing remains deferred during this loop. After
   all eight close and N4 verification is complete, run the Tier D
   shakedown/campaign and evaluate T3 in that order.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
eventually require Tier C pricing because Tier B is dominated by four unstable
instances. During P4-CVG, only the fixed P4A/P4B subsets are run; broad Tier
B/C campaigns remain deferred until the eight causal fixtures close.
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
- [ ] Close P4-CVG on the fixed three-instance P4A and five-instance P4B sets.
- [ ] Complete N4 verification against the reconciled scheduling ledger.
- [ ] Run a Tier D shakedown, then the 793-instance campaign.
- [ ] Evaluate and report T3 last.

### P4-CVG fixture order

Each fixture closes only on a terminal untruncated match or a fully closed
causal-frontier record. After every accepted repair, rerun all earlier checked
fixtures as cumulative guards.

- [ ] `tr-n300-s055` — current frontier: `REDUCE-CANDIDATE-1` at the common
  conflict-1,000 Reduce.
- [ ] `tr-n260-s002`.
- [ ] `tr-n280-s052`.
- [ ] `mod4block_2vars_10gates_u2_autoenc-sc2009`.
- [ ] `mp1-klieber2017s-0500-023-t12`.
- [ ] `or_randxor_k3_n540_m540`.
- [ ] `unsat-set-b-fclqcolor-10-07-09`.
- [ ] `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition`.

<!-- track-sched:begin -->
### Open scheduling-parity findings

None currently classified. The stale resume rows `ORDER-TUMBLE-1`,
`ORDER-ELIM-INIT-1`, `ORDER-BUMP-RATE-1`, `ORDER-LUCKY-1`, and
`ORDER-FOLLOWUP-1` are implemented and dispositioned; their evidence remains in
`docs/implementation-notes.md` §13. `REDUCE-CANDIDATE-1` stays in the P4-CVG
classification queue until source and candidate tracing determines whether it
is a scheduling deficiency, a stronger/deliberate choice, or downstream.

N4 verification remains pending while that classification is unresolved. A
P4-CVG result adds one deduplicated finding here only when it identifies missing
or cruder default scheduling.
<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity beyond reference parity

- [ ] Add debug proof-event correlation: record the emitting rule/application
  and proof line or byte range so checker failures localize without inference.
- [ ] Add extended-resolution support: expose extension-variable introduction
  as a universal public proof-rule operation, emit independently checkable
  DRAT/LRAT evidence, preserve SAT-model reconstruction, and evaluate a
  DefaultPolicy admission strategy only after the rule/API boundary is green.
- [ ] Disposition the seven independent-lineage candidates before compacting
  their survey: binary-resolution minimization and blocking restarts (phase
  4.5), adaptive Reduce intervals and usage-count ranking (Reduce policy), and
  branch/restart/polarity rotation, `best_inv`, and schedule strings (phase 7
  scheduler/preset design). Each is a candidate, not a promised implementation.
- [ ] Concretize O15 through `revise-plan` when phase 4 exits.
- [ ] Substitute (equivalent-literal substitution) is a potential proof rule
  whose implementation can be optimized.
- [ ] SAT sweeping currently harvests only equivalences and backbone units;
  harvesting richer consequences from the sweep environment is room for
  improvement (related: designing new proof rules).

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

- `REDUCE-CANDIDATE-1` is the current one-at-a-time frontier: explain the first
  different survivor in the common conflict-1,000 Reduce.
- `N1-FACTOR-INSTANCES`, `N1-CONG-INSTANCES`, and `N1-RETIRED-DECIDE` are
  reopened by the documentation audit. Each needs an explicit semantic
  disposition plus an adversarial public-API reachability test.
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

- 2026-08-09 — matched Kissat's reduction cadence/ramp; the next mismatch is
  `REDUCE-CANDIDATE-1` at the common conflict-1,000 Reduce (`d22593e`).
- 2026-08-09 — reused exact OTFS replacements while preserving strict
  post-minimization subclauses (`ca8b341`).
- 2026-08-09 — matched Kissat minimization depth and learned-block order
  (`7a52c53`, `3d297e5`).
- 2026-08-09 — repaired OTFS reason lifetime and conflict-clock accounting
  (`4a08214`, `bb0f17f`).
- 2026-08-09 — added P4-CVG manifests, conflict-window tracing, zero-overhead
  production trace checks, and the causal-frontier divergence-v2 gate.
