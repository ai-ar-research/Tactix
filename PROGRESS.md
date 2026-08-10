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
- **Last updated:** 2026-08-10.

## Resume here

The three P4A fixtures remain closed, but the first P4B fixture, `mod4block`, is
reopened by its cumulative guard.  The former closing pin `95bcea4` solves it in
**10.96s**; the next accepted `mp1` increment `c5a98e4` takes **261.40s** with
5,028,372 conflicts.  Both models and proofs verify.  The first old/new semantic
difference is preprocessing Sweep seed 171: the corrected embedded solver
invalidates a SAT model after a later UNSAT-under-assumptions solve.  A focused
rollback proves that first difference is performance-neutral by itself.  The
complete boundary is `SP-SWEEP-KITTEN-1`: Tactix deduplicates one equal-depth
binary encounter, and even after the environment is made byte-equivalent its
embedded initial model/query stream differs from Kitten.  Environment-only and
search-only partial ports are inadmissible; the whole per-seed state machine is
the implementation unit.

On `mp1`, the accepted startup/random and sparse-watch repairs make the
normalized Tactix/Kissat trajectory exact through conflict 2504.  The first
mismatch at conflict 2505 is downstream of a learned reason Kissat deletes in
the first search-time Vivify and Tactix retains.  Three locally sound partial
ports were rejected and removed: Kissat's implied-candidate deletion depends on
the complete Vivify deduction state machine, not a probe-position or derived-size
proxy.  The evidence and exact implementation boundary are in
`benchmarks/reports/2026-08-10-mp1-vivify-semantic-unit.md`.
The canonical Tier A evidence is in
`benchmarks/reports/2026-08-09-tier-a-kissat-divergence-convergence.md`;
the full lifecycle matrix remains in
`benchmarks/reports/2026-08-09-default-strategy-application-order-audit.md`.
Production builds omit all trace implementation, symbols, and event strings.
The newest broad window, `20260809-144538-B-parity-old`, belongs to the
superseded pre-repair collapse and is cited for provenance, not banked.

**Next actions:**

1. Re-close `mod4block` by implementing `SP-SWEEP-KITTEN-1` as one complete
   semantic unit: depth-sensitive environment construction, Kitten-compatible
   model/decision/phase and flip behavior, typed query stream, and
   effort/resumption handoff.  Never restore stale-model reuse.
2. Then return to `mp1` and implement `SP-VIVIFY-DEDUCE-1` only as the complete
   deduction terminal-state machine, with per-arm reach counters, proof/model
   tests, the target-clause deletion trace, and all earlier fixtures as
   cumulative guards.
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

- [x] `tr-n300-s055` — accepted stronger singleton minimization.
- [x] `tr-n260-s002` — accepted stronger singleton minimization.
- [x] `tr-n280-s052` — warm-up fixed; measured-better B+W deviation retained.
- [ ] `mod4block_2vars_10gates_u2_autoenc-sc2009` — reopened: corrected
  cumulative pin regresses 10.96s → 261.40s; `SP-SWEEP-KITTEN-1` is localized
  at preprocessing seed 171 and must land as a complete coupled unit.
- [ ] `mp1-klieber2017s-0500-023-t12`.
- [ ] `or_randxor_k3_n540_m540`.
- [ ] `unsat-set-b-fclqcolor-10-07-09`.
- [ ] `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition`.

<!-- track-sched:begin -->
### Open scheduling-parity findings

`ORDER-SPARSE-COLLECT-1`, `FACTOR-PREPROCESS-1`, and the `mod4block` Sweep
currency/candidate-flip findings are implemented and dispositioned. The stale
rows `ORDER-TUMBLE-1`, `ORDER-ELIM-INIT-1`, `ORDER-BUMP-RATE-1`,
`ORDER-LUCKY-1`, and `ORDER-FOLLOWUP-1` remain closed; their evidence is in
`docs/implementation-notes.md` §13. `SP-SWEEP-KITTEN-1` is the current open
`mod4block` handle; environment construction, embedded model/search, query
state, and scheduler handoff are one semantic unit. `SP-VIVIFY-DEDUCE-1` is
open from `mp1`, but follows the earlier cumulative guard.

N4 verification remains pending until the remaining fixed fixtures complete the
lifecycle audit. A P4-CVG result adds one deduplicated finding here only when it
identifies missing or cruder default scheduling.
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
- [ ] It is known that factor (i.e., bounded variable addition) can be implemented
  in a drastically faster manner:
  https://drops.dagstuhl.de/storage/00lipics/lipics-vol377-sat2026/LIPIcs.SAT.2026.29/LIPIcs.SAT.2026.29.pdf

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

- The current one-at-a-time frontier is `SP-SWEEP-KITTEN-1` at `mod4block`
  preprocessing seed 171.  Align the complete environment/model/query and
  effort/resumption lifecycle before returning to `mp1`'s
  `SP-VIVIFY-DEDUCE-1`.
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
