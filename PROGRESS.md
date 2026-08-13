# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — Tier B/C causal convergence with original Kissat.
- **Exit:** identical Kissat/Tactix solved sets on Tier B and Tier C, ≤1.10×
  Kissat PAR-2 on both at the matched 32 GB shape, plus P4-CVG/N4 closure.
- **Non-negotiables:** N1 verification **complete** (2026-08-12: the three
  reopened audit findings are closed — Factor now enumerates seed literals,
  Congruence's round is specified as the indivisible instance, and Decide
  enumerates every unassigned literal including retired variables; each has an
  adversarial reachability test) · N2 verification **pending**: the first legs to run the
  solver's own `--lrat` rather than the DRAT conversion rejected certificates of
  runs answering UNSATISFIABLE, under DefaultPolicy as well as random schedules.
  Three causes are fixed (V3 chain order, unit identity installed before the
  assert, a reason naming a deleted clause) and all 57 preserved rejections now
  verify; the standing evidence is the 200k native-LRAT pair in flight · N3 Tier-D parity not yet measured · N4 verification
  **pending** while the current causal frontier is classified.
- **Last updated:** 2026-08-12.

## Resume here

Phase 4 has one goal: Tactix must solve every Tier B/C instance solved by the
clean original Kissat pin, and its Tier B and Tier C PAR-2 must each be no more
than 1.10× Kissat's under the matched 32 GB shape. A divergence is work to
localize and repair unless the Tactix behavior is proved stronger or an explicit
source-backed exception is accepted. Fixed P4A/P4B fixtures are cumulative
regression guards inside this loop, not a separate workstream.

The current Tier B witness is
`2013113162201nw.shuffled-as.sat03-1668.cnf`. Stored clean-original Kissat is
6.052 s. The first Sweep mismatch was localized to failed-assumption core
commitment: Kissat committed every unit lemma exposed by the core immediately,
while Tactix retained only the final target and deferred it to the seed boundary.
Tactix now commits the ordered core units during detection. On seed 2816 both
solvers produce `-4737,-1793` after query 3 and then ask the same 38 embedded
queries with matching candidates, outcomes, and per-query ticks. Proof-mode
Tactix solves the complete witness in 1.02 s with a valid model and zero Sweep
fallback replays. The remaining local ordering difference is the intentional
environment-boundary batch of already-proved equivalence substitutions, which
avoids repeated whole-arena rebuilds without changing later embedded queries.

Tier C still owns two cumulative gates. The goldcrest crash was localized to
stale jumped-reason metadata across variable compaction; the two exact
regressions and focused sanitizers are green, but a fresh immutable Release
rerun remains required. `128_125` is a real solve-set gap: under 32 GB clean
Kissat solved SAT in 87.419 s with a verified model, while the preceding Tactix
pin timed out at 1200 s after using about 25.7 GiB. It is not a memory exemption.

The trace-divergence workflow is being replaced by a structured rule journal in
both diagnostic solvers. Every proof rule in Kissat and Tactix must use the same
transaction contract, including applied, declined, no-op, budget, and refuted
outcomes; exact guard facts must come from the evaluator which controls the
operation. Sweep → consequence commitment → Congruence/Substitute is the
pilot because it owns the active witness. Coverage then extends to every proof
rule, while production builds compile out the journal implementation, symbols,
environment markers, and hot-path branches. Record→replay must reproduce the
ordered semantic deltas/digests and stop at the exact changed field.

**Next actions — repeat without opening a separate Phase-4 workstream:**

1. Pin the core-unit repair with a focused regression, checker gates, and the
   selected witness; finish the Sweep/Congruence/Substitute journal pilot in
   both diagnostic solvers and demonstrate the newly closed first divergence.
2. Complete uniform journal coverage for all proof rules in both solvers, with
   replay/mutated-field tests and production trace-elision checks.
3. Rerun goldcrest and current Tactix Tier B/C at 32 GB. Compare only with the
   stored clean-original Kissat results; never use an instrumented Kissat for
   performance. State solved-set differences before aggregate timing.
4. Take every missing solve, then every runtime gap threatening 1.10×, in
   descending gate impact. Journal the first differing control event, guard
   fact, or transaction outcome; fix it and rerun all earlier cumulative guards.
5. Repeat actions 3–4 until solved sets match and both PAR-2 ratios are ≤1.10.
   That same closure completes P4-CVG and N4.

## Current measurement contract

`benchmarks/runs/<window>/results.json` owns per-instance results. A claim names
the manifest SHA and allocation shape, states #solved before PAR-2, names moved
instances, and compares only same-window compatible arms. Every arm writes a
proof; every scored Tactix SAT answer is model-checked. Broad scheduling changes
eventually require Tier C pricing because Tier B is dominated by boundary
instances. P4-CVG's fixed P4A/P4B sequence is closed; Tier C performance work
is paused on the early-exit incident above.
Feature acceptance is cumulative: an accepted change remains enabled while the
next change is evaluated against the preceding accepted pin. Single-feature off
arms are diagnostic; they do not replace the cumulative comparison. Incremental
campaigns run only those two Tactix pins. Kissat and CaDiCaL are rerun at gates,
or when the manifest or allocation shape changes, rather than on every step.

Matched or one-sided feature-off runs are diagnostic cuts inside the convergence
loop, not separate deliverables. Map executable semantics rather than flag
names, change one semantic gate at a time, and remove diagnostic variants after
attribution; no ablation replaces shipped-default full-tier evidence.

Every Tier A/B/C/D solver process receives a 32 GB address-space limit. Kissat
performance numbers come only from the clean original pin; diagnostic/event or
rule-journal Kissat binaries are never timed as baselines. Reuse stored Kissat
and CaDiCaL results when the instance and allocation contract are unchanged.

## Gates

- [x] **T1:** Tier A and MiniSat bar (`benchmarks/reports/2026-07-14-t1-gate.md`).
- [x] **T2:** Glucose/API bar (`benchmarks/reports/2026-07-14-t2-gate.md`).
- [ ] **T3:** identical solved sets with original Kissat on Tier B/C and
  ≤1.10× Kissat PAR-2 on both at the matched 32 GB shape.
- [ ] **T4:** DefaultPolicy engineering exceeds Kissat on the SC25 Tier-D view.
- [ ] **T5:** beat AE-Kissat-MAB or ship best effort.

## Phases 0–3 — complete

- [x] Setup, kernel, proof pipeline, competition I/O, API freeze, and full
  reference rule-inventory disposition.
- [x] Phase 3 rule-completeness gate, including reach evidence or explicit
  no-implement disposition (`benchmarks/reports/2026-08-07-phase-3-gate.md`).

## Phase 4 — Tier B/C causal convergence

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
- [x] Close P4-CVG on the fixed three-instance P4A and five-instance P4B sets
  (`benchmarks/reports/2026-08-11-p4-cvg-closure.md`).
- [ ] Match original Kissat's solved set on full Tier B and full Tier C.
- [ ] Reach ≤1.10× original-Kissat PAR-2 on both Tier B and Tier C.
- [ ] Close every decision-relevant first divergence or record an explicit,
  source-backed stronger/accepted behavior; use the resulting corpus plus the
  reconciled scheduling ledger to complete N4 verification.

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

The Tier C early-exit incident is the immediate owner. After it closes,
reconcile the final scheduler snapshot and these accepted deviations against
the complete ledger, then mark N4 only on an explicit verification record.
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

- The Tier C early-exit incident is blocking performance interpretation; the
  second GDB capture exposed stale jumped metadata on an unassigned active
  variable, and the causal repair passes its regression and 69/69 tests but
  still needs the real-instance proof-checked rerun. `128_125` is independently
  confirmed as a 32 GB Tactix timeout versus a verified Kissat SAT. N4 resumes
  through the unified Tier B/C divergence-closure loop after these blockers
  close.
- The sparse-GC candidate remains deferred and outside the closure pin; do not
  exercise it without separate user direction. Corpus 004/005 are closed: the
  Sweep stale-occurrence fix repaired both, and they pass on every corpus leg.
- `HyperBinaryRes` remains an accepted standalone deviation with a policy-side
  structural cap.
- Random-policy nontermination: ~12% of cases reach no verdict at the 5 s case
  timeout, so they run no semantic oracle. The ranked-picker totality defect is
  fixed (both pickers fall back to index order when `H`'s structures cannot
  answer), but the population itself is undiagnosed.
- `VariableCompaction` and `VivifyFlush` read as never-fired on every random
  and stress leg for a structural reason, not a coverage one: compaction is
  called only from DefaultPolicy (`default_policy.cpp`), and `vivify_flush`
  defaults off and is only set by its env knob. Their coverage has to come from
  a DefaultPolicy leg and from an explicit flush arm; the `Lucky` and
  `SweepResume` rows read 0 for a real reason (broken specs) and are repaired.
- Factor's next-factor counting and matching passes can disagree (the count
  reserves each large occurrence across the whole quotient, the re-scan only
  against the chosen factor). Grid-choice fidelity only; the repair is the
  memoized `qlauses` lifecycle in `docs/implementation-notes.md` §5.
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

- 2026-08-12 — N1 audit and soundness fuzzing. Fixed a heap-buffer-overflow in
  Factor's per-candidate scratch (the admitted fresh literal indexed one past
  `cofactor`/`armed`; Kissat's `resize_factoring` grows unconditionally), the
  missing binary branch of Vivify's instantiation strengthening, and a debug
  watch invariant stricter than the sparse-collect contract it checked. All 167
  preserved crash bundles are clean, corpus is 21/21 with three new entries, and
  both suites are 72/72 in Release and under ASan/UBSan with assertions (they
  were 62/72 before). Closed the three reopened N1 findings with dispositions
  and adversarial tests, and made the fuzz harness vary the policy seed per case
  so the dense-index half of the schedule is reached (4407 of 8803 cases).
  Four further defects came out of the first legs on the fixed pins. The eager
  register asserted a redundancy neither consumer requires (C3 promotes a
  registered clause). The other three are N2: V3's rewrite chain cited a
  bridging lemma before the unit that makes it unit and deduplicated by sorting;
  `PendingUnits` installed a variable's LRAT unit identity when a unit was
  recorded rather than asserted, so the opposite literal could become the root
  value first and poison every chain citing it (two assert loops also refuted
  without emitting the pair's empty clause); and S6 Walk and V5 Sweep deleted
  root-satisfied clauses that were still trail reasons, so a later reason walk
  cited a deleted id. Corpus is 24/24 through the native-LRAT oracle and
  micro_proofs.sh carries the chain witness.
- 2026-08-11 — made 32 GB Tier B/C original-Kissat convergence the Phase-4
  exit and retired may-not-gate exemptions. `128_125` is a 1200.546s Tactix
  timeout versus Kissat's verified 87.419s SAT. Two optimized goldcrest
  backtraces exposed retired and stale-unassigned jumped-reason compaction
  hazards; both exact regressions, focused sanitizers, and 69/69 Release tests
  pass, with the second real-instance closure rerun pending.
- 2026-08-11 — started a current-pin Tier C retake, stopped it under plan §8.1
  after two early nonterminal exits, and added explicit Tier C causal-diagnostic
  support without changing the fixed P4-CVG gate. The partial 22/30 window is
  diagnostic only; focused exit classification remains open.
- 2026-08-11 — fixed three random-policy fuzz defects: Sweep's occurrence
  snapshot outliving an arena collection, a root literal fixed by a speculative
  propagation without its LRAT unit line, and a clause added behind the
  propagation cursor. Both 10,000-case legs are 0 failures and corpus is 18/18;
  Tier A stays instance-identical.
- 2026-08-11 — re-audited rule/strategy ownership: removed dead Factor policy
  state, made Sweep resumption an explicit limit, and stopped Substitute from
  charging Sweep; full A/B behavior is identical with +0.56%/+0.73% runtime.
