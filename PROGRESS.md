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
- **Last updated:** 2026-08-11.

## Resume here

The two random-policy fuzz defects found on `07dccc2` are fixed, and the
regression corpus is clean in both proof formats.

1. **Sweep traversed a stale arena snapshot** (crash, both formats). Its
   round-local occurrence index outlived the collection the inline Substitute
   bridge performs. `Kernel::arena_generation` is bumped by every
   `kernel_collect_garbage`; the snapshot carries it and `sweep_env_build`
   rebuilds when it moved. `Kernel::dense` is rebuilt by the collector, so
   DefaultPolicy never reached this.
2. **A root literal was fixed without its LRAT unit line** (proof rejection,
   LRAT only). Only `propagate_service` named root units. Sweep asserts each
   environment's units un-propagated and then replays, so the replay's
   speculative propagation drained them at level 0, un-named; 0 is LRAT's
   terminator, so the next citation truncated the line.
   `kernel_propagate_probing` now emits through `kernel_emit_root_units`.
   The earlier session's Substitute workaround is reverted — it made the
   installed clause depend on the proof format.

"Root-fixed implies it has a unit id" and the arena-generation rule are
standing invariants in `docs/implementation-notes.md` §3, asserted by
`kernel_check_invariants` and `Proof::add_rup`/`add_rat`. Diagnoses live in
corpus entries `016-sweep-stale-occurrence-index` and
`017-probing-drain-root-unit-id`.

3. **A clause added behind the propagation cursor stayed invisible** (crash,
   both formats; the fourth escaped-unit member after corpus 004/005/006).
   Propagation reaches a clause only through a literal it is currently scanning
   off the trail, so one added over an already-scanned false literal is never
   revisited and the next drain still reports a fixpoint. Sweep reaches it
   because its emission keeps units unpropagated while the RUP replay fixes
   literals underneath the pair it is about to add, leaving the rule's own
   value guard stale. Both kernel adders now rewind `propagate_head`
   (`kernel_rearm_propagation`), gated to a root with no conflict in flight
   because ConflictAnalysis adds an all-false clause above the root and
   Backjump already resets the cursor there.

Evidence on pin `pin-rearm-v2`: corpus 18/18 clean on both legs, where
`07dccc2` failed 3 (DRAT) and 4 (LRAT) including the long-deferred 004 and 005;
10,000-case legs `1611179`/`1611180` are **0 failures** in both proof formats.
Same-window Tier A (`20260811-161629-A-A-cand` versus `-A-A-base`) and Tier B
(`20260811-161642-B-B-cand` versus `20260811-161645-B-B-base`) against
`07dccc2` are instance-identical over all 228 — 0 verdict, proof-byte or
reach-map differences, 200/200 and 28/28, every proof checked and all 14 Tier B
models validated. The re-arm counter reads 0 on twelve Tier A instances under
DefaultPolicy (`1611384`), so the trajectory is untouched and all three repairs
are confined to free schedules.

The user-directed Tier B performance block is complete. Factor now uses sparse
literal buckets for last-factor construction, and Sweep owns its
representation-bridge effort charge instead of relying on Substitute to mutate
another rule's counter. Full same-window jobs `1609714`/`1609715` solve all
**28/28** Tier B fixtures (14 SAT, 14 UNSAT), improving the previous accepted
pin's 25/28 result and 114.19s PAR-2 to 28/28 and 28.69s. Every scored model and
DRAT certificate passed independent validation. The causal record is
`benchmarks/reports/2026-08-11-tier-b-factor-sweep-performance.md`.

The subsequent rule/strategy boundary audit removed a dead Factor admission
switch, moved Sweep resumption into caller-owned `SweepLimits`, and eliminated
the remaining Substitute→Sweep governor mutation. Full Tier A and Tier B
before/after evaluations have identical verdicts, reached maps, proof sizes,
and model-check outcomes; Tier A runtime changes by +0.56% and Tier B by +0.73%.
The completed windows are `20260811-113528-A-rule-policy-after` and
`20260811-113628-B-rule-policy-after`.
An additional `FmlaEquivChain` replay produced byte-identical DRAT and
normalized logs. Job `1610381` passes the full Release suite 69/69. The audit
record is `benchmarks/reports/2026-08-11-rule-strategy-boundary-audit.md`.

P4-CVG remains closed by its canonical fixed-order jobs `1607665`--`1607672`.
Tier C was deliberately cancelled rather than awaited and has no complete
result in this block. The sparse-GC candidate remains deferred; corpus cases
004 and 005 are no longer deferred — both now pass on every corpus leg. No
Tactix benchmark job is in flight; the live queue contains only unrelated
workloads outside this repository.

**Next actions:**

1. Complete N4 verification against the reconciled scheduling ledger and the
   canonical eight-fixture closure manifest. Do not reopen P4-CVG without a
   new proof/model failure or a localized causal-frontier regression.
2. Restore N1 evidence for Factor, Congruence, and retired Decide
   instances. Do not call N1 green until adversarial public-API enumeration
   tests cover every legal instance or the calculus explicitly defines a round
   as the indivisible instance.
3. Run the Tier D shakedown/campaign, then evaluate T3, after N4 completes.
4. Keep the sparse-GC candidate deferred unless the user separately authorizes
   returning to that security-warning path.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
eventually require Tier C pricing because Tier B is dominated by boundary
instances. P4-CVG's fixed P4A/P4B sequence is closed; current user-directed work
uses full Tier B, while Tier C remains paused.
Feature acceptance is cumulative: an accepted change remains enabled while the
next change is evaluated against the preceding accepted pin. Single-feature off
arms are diagnostic; they do not replace the cumulative comparison. Incremental
campaigns run only those two Tactix pins. Kissat and CaDiCaL are rerun at gates,
or when the manifest or allocation shape changes, rather than on every step.

Phase 4 also requires a small matched feature-off matrix.  For each selected
mechanism, map the semantic gate in Kissat and Tactix (not merely similarly
named command-line flags), disable that mechanism in both solvers, and replay
the same fixed fixtures one configuration at a time.  The resulting trajectory
must either remain aligned or have a localized, source-backed, justified
divergence.  These projections are both robustness tests and diagnostic cuts
through otherwise coupled behavior; they supplement, but never replace, the
shipped-default trajectory and production-performance gates.
One-sided feature-off runs are also encouraged when they isolate which solver
or subsystem introduces a divergence.  Record the hypothesis and trace anchor,
change one semantic gate at a time, and remove the diagnostic variant after the
attribution question is answered; it is not a candidate shipped strategy arm.

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
- [x] Close P4-CVG on the fixed three-instance P4A and five-instance P4B sets
  (`benchmarks/reports/2026-08-11-p4-cvg-closure.md`).
- [ ] Complete N4 verification against the reconciled scheduling ledger.
- [ ] Pass the matched feature-off differential matrix: selected shared
  mechanisms disabled symmetrically in Kissat and Tactix, one configuration at
  a time, with every trajectory divergence localized and justified.
- [ ] Run a Tier D shakedown, then the 793-instance campaign.
- [ ] Evaluate and report T3 last.

### P4-CVG fixture order

Each fixture closes only on a terminal untruncated match or a fully closed
causal-frontier record. After every accepted repair, rerun all earlier checked
fixtures as cumulative guards.

- [x] `tr-n300-s055` — verified UNSAT, 27.83s (job `1607665`).
- [x] `tr-n260-s002` — verified UNSAT, 4.90s (job `1607666`).
- [x] `tr-n280-s052` — verified UNSAT, 16.42s (job `1607667`).
- [x] `mod4block_2vars_10gates_u2_autoenc-sc2009` — verified SAT/model,
  163.96s (job `1607668`).
- [x] `mp1-klieber2017s-0500-023-t12` — verified SAT/model, 195.08s
  (job `1607669`).
- [x] `or_randxor_k3_n540_m540` — verified UNSAT, 14.41s (job `1607670`).
- [x] `unsat-set-b-fclqcolor-10-07-09` — verified UNSAT, 39.38s
  (job `1607671`).
- [x] `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition` — verified UNSAT,
  101.62s (job `1607672`).

<!-- track-sched:begin -->
### Open scheduling-parity findings

P4-CVG is closed. `SP-VIVIFY-DEDUCE-1`, compact-universe randec admission, the
fresh Sweep→Substitute equivalence bridge, and the solve-latched large-formula
Factor gate are implemented and cumulatively verified. `SP-FACTOR-OCCURRENCE-1`
remains semantically closed at startup. The sparse-GC candidate is excluded
from the closure pin and stays deferred.

N4 verification is now the immediate owner: reconcile the final scheduler
snapshot and these accepted deviations against the complete ledger, then mark
N4 only on an explicit verification record.
<!-- track-sched:end -->

## Phase 4.5 — engineering ingenuity beyond reference parity

- [ ] Add debug proof-event correlation: record the emitting rule/application
  and proof line or byte range so checker failures localize without inference.
- [ ] Add extended-resolution support: expose extension-variable introduction
  as a universal public proof-rule operation, emit independently checkable
  DRAT/LRAT evidence, preserve SAT-model reconstruction, and evaluate a
  DefaultPolicy admission strategy only after the rule/API boundary is green.
- [ ] Evaluate a zero-copy embedded-SubSolver clause view that reuses immutable main-formula clause storage while retaining local watches, trail, compact variables, and proof mapping.
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

- N4 verification against the reconciled scheduling ledger is the immediate
  phase-order action now that P4-CVG is closed.
- The sparse-GC candidate remains deferred and outside the closure pin; do not
  exercise it without separate user direction. Corpus 004/005 are closed: the
  Sweep stale-occurrence fix repaired both, and they pass on every corpus leg.
- `N1-FACTOR-INSTANCES`, `N1-CONG-INSTANCES`, and `N1-RETIRED-DECIDE` are
  reopened by the documentation audit. Each needs an explicit semantic
  disposition plus an adversarial public-API reachability test.
- `HyperBinaryRes` remains an accepted standalone deviation with a policy-side
  structural cap.
- Diagnose random-policy nontermination; it is an N1 robustness issue but does
  not gate the already-closed phase 3.
- The random-policy fuzz legs never connect the dense occurrence index:
  `main.cpp` defaults the policy seed to the even constant 20260710, so
  `random_solve`'s `(seed & 1) != 0` branch never fires and every bare
  `--random-policy` run prints `c stress-dense 0`. The dense-gated tails read
  `BveBackward=0!`, `BveBackwardUnit=0!` and `VariableCompaction=0!` in every
  leg's reach line — that half of the coverage comment is vacuous until the
  harness varies the policy seed.
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

- 2026-08-11 — fixed three random-policy fuzz defects: Sweep's occurrence
  snapshot outliving an arena collection, a root literal fixed by a speculative
  propagation without its LRAT unit line, and a clause added behind the
  propagation cursor. Both 10,000-case legs are 0 failures and corpus is 18/18;
  Tier A stays instance-identical.
- 2026-08-11 — re-audited rule/strategy ownership: removed dead Factor policy
  state, made Sweep resumption an explicit limit, and stopped Substitute from
  charging Sweep; full A/B behavior is identical with +0.56%/+0.73% runtime.
- 2026-08-11 — replaced Factor's dense scan with sparse literal buckets and
  repaired Sweep representation accounting; full Tier B improves 25/28 →
  28/28 solved and 114.19s → 28.69s PAR-2, correctness clean.
- 2026-08-11 — made retirement deactivation idempotent after GDB/ASan and
  one-sided cuts localized duplicate-event VMTF corruption; full Tier B improves
  21/28 → 25/28 and 187.33s → 116.58s PAR-2, correctness clean.
- 2026-08-11 — closed all eight P4-CVG fixtures in fixed order on release pin
  `c3ecdf38…28c14`; jobs `1607665`--`1607672` all pass proof/model validation.
