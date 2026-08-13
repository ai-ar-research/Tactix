# Progress

**Tactix:** a proof-rule-driven SAT solver. Stable scope and architecture are in
`DEVELOPMENT_PLAN.md`; rule semantics in `docs/calculus.md`; guards and
deviations in `docs/implementation-notes.md`. This file contains current state
only.

- **Current phase:** 4 — T3 met; N4 verification is the remaining exit.
- **Exit:** identical Kissat/Tactix solved sets on Tier B and Tier C, ≤1.10×
  Kissat PAR-2 on both at the matched 32 GB shape, plus P4-CVG/N4 closure.
- **Non-negotiables:** N1 verification **complete** (2026-08-12: the three
  reopened audit findings are closed — Factor now enumerates seed literals,
  Congruence's round is specified as the indivisible instance, and Decide
  enumerates every unassigned literal including retired variables; each has an
  adversarial reachability test) · N2 verification **complete** (2026-08-13: four proof
  defects found by first running the solver's own `--lrat` under the random
  policy, then closed; the 200k native-LRAT and 200k DRAT random-policy legs on
  `6f44f23` are 0 failures with corpus 25/25) · N3 Tier-D parity not yet measured · N4 verification
  **pending** while the current causal frontier is classified.
- **Last updated:** 2026-08-13.

## Resume here

T3 is met on both tiers in a single-array, three-arm window
(`benchmarks/reports/2026-08-12-tier-bc-t3-same-window.md`): Tier B 28/28 at
1.036× original Kissat, Tier C 30/30 at 1.067×, correctness clean, every arm
writing proofs. `128_125` and the Tier C early-exit incident closed with it.
**N4 verification is the remaining phase-4 exit**, and it is a documentation
and reconciliation task, not a measurement one: reconcile the final scheduler
snapshot and the accepted deviations against the complete scheduling-parity
ledger, then mark N4 only on an explicit verification record.

The aggregate ratio hides live divergences rather than retiring them. The
sharpest is `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition` at 7.0× on
Tier B; `minxor128` (2.08×) and `goldcrest-and-9` (2.01×) lead Tier C. The
Sweep witness `2013113162201nw.shuffled-as.sat03-1668` is down to 2.81× after
the core-unit repair. Any of these is a legitimate next divergence to localize
with the rule journal; none of them gates T3 any more.

The structured rule journal remains the divergence-localization tool. Every
proof rule in Kissat and Tactix should use the same transaction contract —
applied, declined, no-op, budget, refuted — with exact guard facts from the
evaluator that owns the operation. Sweep → consequence commitment →
Congruence/Substitute is the landed pilot; coverage still has to extend to
every proof rule, with production builds compiling out the journal
implementation, symbols, environment markers, and hot-path branches.

**Next actions:**

1. Reconcile the scheduler snapshot and the accepted deviations against the
   scheduling-parity ledger and record an explicit N4 verification result;
   that closes phase 4.
2. Extend the rule journal to every proof rule in both solvers, with
   replay/mutated-field tests and production trace-elision checks.
3. Take the largest remaining paired divergences in descending gate impact
   (`veer_axi…p23_transition`, `minxor128`, `goldcrest-and-9`), journal the
   first differing control event or guard fact, and rerun the cumulative
   guards after each repair.
4. Widen the oracles that have never run at scale: 10 of 25 rules still have
   no targeted stress leg, `src/frontend/parse.cpp` has no direct test, and
   cake_lpr is not wired into the harness. Every oracle first run at depth
   this session found a defect.

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
- [x] **T3:** identical solved sets with original Kissat on Tier B/C and
  ≤1.10× Kissat PAR-2 on both at the matched 32 GB shape
  (`benchmarks/reports/2026-08-12-tier-bc-t3-same-window.md`).
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
- [x] Match original Kissat's solved set on full Tier B and full Tier C
  (28/28 and 30/30, one array per tier, 2026-08-12).
- [x] Reach ≤1.10× original-Kissat PAR-2 on both Tier B and Tier C
  (1.036× and 1.067×).
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

- The Tier C early-exit incident and the `128_125` solve gap both closed on the
  2026-08-12 32 GB reruns: full Tier C is 30/30 with every proof checked and no
  nonterminal exit, `goldcrest-and-9` is a verified UNSAT in 168.27 s, and
  `128_125` is a verified SAT in 131.53 s against Kissat's 88.84 s. N4 resumes
  through the unified Tier B/C divergence-closure loop.
- The sparse-GC candidate remains deferred and outside the closure pin; do not
  exercise it without separate user direction. Corpus 004/005 are closed: the
  Sweep stale-occurrence fix repaired both, and they pass on every corpus leg.
- `HyperBinaryRes` remains an accepted standalone deviation with a policy-side
  structural cap.
- Random-policy nontermination: 24,052 of 200,000 cases (12.0%) reach no
  verdict at the 5 s case timeout, so they run no semantic oracle and the legs
  below are 176k cases, not 200k. The ranked-picker totality defect is fixed
  (both pickers fall back to index order when `H`'s structures cannot answer);
  the population itself is undiagnosed.
- A `Decide,Vivify` stress schedule leaves corpus 017 and 025 without a
  verdict at the 60 s corpus timeout, so that leg pre-registers
  `--allow-nonterminating 2`. Both are proof-oracle entries whose verdict needs
  the normal-step propagation this schedule starves.
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

- 2026-08-13 — T3 met and N2 re-verified. Tier B 28/28 at 1.036× original
  Kissat and Tier C 30/30 at 1.067×, one array per tier with Kissat and CaDiCaL
  re-measured beside it, correctness clean
  (`benchmarks/reports/2026-08-12-tier-bc-t3-same-window.md`). The intensive
  legs on the same binary are 0 failures: 200k random-policy native-LRAT, 200k
  random-policy DRAT, 10k random-policy under ASan/UBSan with assertions, and
  10k stress-Factor, with corpus 25/25 and 87,946 dense-connected cases.
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
- 2026-08-11 — fixed three random-policy fuzz defects: Sweep's occurrence
  snapshot outliving an arena collection, a root literal fixed by a speculative
  propagation without its LRAT unit line, and a clause added behind the
  propagation cursor. Both 10,000-case legs are 0 failures and corpus is 18/18;
  Tier A stays instance-identical.
- 2026-08-11 — re-audited rule/strategy ownership: removed dead Factor policy
  state, made Sweep resumption an explicit limit, and stopped Substitute from
  charging Sweep; full A/B behavior is identical with +0.56%/+0.73% runtime.
