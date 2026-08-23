# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and the debug contract;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4
evidence. This file is the current resume surface. Frozen manifests and reports
own experimental detail and history.

## Resume anchor

- Worktree `/home/hwu/SAT-solving`, branch `main`. This convergence checkpoint
  descends from `f41cf483da211b7550ff97a12a211d4d45138f20`, which was exactly
  synchronized with `origin/main` before the line began. Do not discard it.
- `.claude/worktrees/tierd-next10` is clean. The historical
  `.claude/worktrees/vivify-unit-crash` is dirty and must remain untouched.
  SLURM job `1638010` is unrelated. **NOTHING IN FLIGHT** for this project.
  Final c5492 assignment-pose job `1641293` is complete, untruncated, and
  residue-free.
- Phase-4 N1/N2/N4 and P4-CVG are complete. T3 passed checked Tier B/C at
  1.036x/1.067x Kissat PAR-2. N3, T4-K, T4-VBS, and T5 remain open under
  `DEVELOPMENT_PLAN.md` §8.3.
- Tier-D selection is frozen by
  `benchmarks/runs/20260818-0142-tierd-ranks21-40-predeclared/manifest.json`.
  Ranks 21, 23, 24, 25, 28, 29, 34, 38, and 39 are closed. Rank 27 is active;
  rank 22 is parked. Do not duplicate their frozen investigations.
- Rank 27 is immutable input
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`,
  SHA-256 `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
  Frozen Kissat 4.0.4 proves UNSAT in 278.187 s. The current target is checked
  Tactix UNSAT within 300 s/16 GiB through a general divergence repair.

## Immutable guard and comparator

- Never rerun frozen scoring references. Current instrumented Kissat may be
  used only as a source observer; its result is not scoring evidence.
- The inherited comparator is `tactix-shipped`, source
  `7b527fbc711d664898af8e9c5a9655c3bdf4204b`, binary SHA-256
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`.
  Accepted full B28+fixed-C5 guard job `1639948` is 66/66 checked,
  candidate/comparator PAR-2 0.967, with mp1 SAT in 15.048 s and T105 UNSAT in
  1089.328 s. This pin is the regression comparator, not the divergence target.
- The latest pre-lift mandatory guard is
  `benchmarks/runs/20260823-1040-BC-congruence-unmetered-guard/`, job `1640987`:
  65/66 valid and residue-free. Current candidate mp1 is recovered at 22.712 s,
  but T105 times out at 1200.004 s while the comparator proves UNSAT in
  785.626 s. Candidate/comparator PAR-2 is 2.030, so promotion is blocked.
- The newest canonical completed Tier-C `results.json` window remains
  `20260813-111513-C-cleanup-b2b8362`; later targeted and B/C guard manifests
  above supersede it only for the current regression/guard question, not T3.
- Before Rank 27, the current candidate must pass the complete frozen 28-instance
  Tier B plus the inherited five mechanism-relevant Tier C set under the
  pre-Phase-4.5 16 GiB/1200 s contract. Submit full-width candidate/comparator
  tasks from immutable pins, validate every SAT model and UNSAT proof, and leave
  no decompressed input, certificate, or checker residue.

## Current convergence checkpoint

The checkpoint is one coherent current-Tactix/current-Kissat convergence
line. It contains general source-backed scheduling, semantic, observer, and
representation repairs only: no benchmark names, hashes, ranks, per-instance
arms, deadlines, or tuned constants enter the solver. Campaign manifests before
`20260823-1420` retain the full closure history, including exact BVE/Vivify/
Congruence/Substitute scheduling and semantic owners; do not re-derive them
without contradictory evidence.

### Sweep semantic and throughput closure

- Current/current traces share the exact conflict-4,353 entry state. The final
  mixed-occurrence repair reproduces current Kissat's environment ordering and
  yield for all 9,379 aligned seeds. Accepted semantic run
  `benchmarks/runs/20260823-1420-C-t105-sweep-inplace-semantic/` (job `1641098`)
  has trace SHA-256
  `a70a89df76c9885217680cb08f8c2c5f3607d68634024402dd67321ead617de1`,
  byte-identical to the pre-representation repair trace from job `1641093`.
- Kissat's source mechanism was lifted: surviving large-clause images shrink at
  a stable arena `Ref` and allocation span, with a one-time `INVALID_LIT` tail
  marker; ordinary garbage collection traverses allocation span and packs only
  semantic words. Sweep no longer forces an arena collection.
- The connected transaction now materializes its binary/large mixed occurrence
  stream only for literals actually reached by an environment or rewrite,
  retaining storage across calls. `prime_large` snapshots every touched live
  large clause before proof/kernel mutation. Semantic run
  `benchmarks/runs/20260823-1440-C-t105-sweep-lazy-semantic/` (job `1641100`)
  reproduces the same full trace hash, is untruncated and residue-free, and
  lowers trace-build Sweep time 10.869→8.939 s and RSS
  2,070,736→1,767,476 KiB.
- Observer-free production run
  `benchmarks/runs/20260823-1450-C-t105-sweep-lazy-production/` (job `1641101`)
  preserves the stopped state and semantic counters while reducing Sweep
  6.029→2.039 s. At this common semantic state it is 0.645x frozen current
  Kissat's 3.16 s. Eager ledger construction falls 1.233→0.031 s and final
  collection remains zero. Stage totals are dense entry 0.446 s, scheduling
  0.389 s, seed processing 0.631 s, and post-seed closure 0.513 s.
- Diagnostic-only `sweep_ticks` is 42 lower because a stream first materialized
  after an unrelated clause became garbage omits that stale host occurrence.
  Scheduled `sweep_ticks_sub`, policy effort, trace, formula state, and
  trajectory are unchanged. This is a source-backed better-side host-scan
  difference, not an effort or semantic divergence.

### Proof-rule/strategy separation

- Kernel owns a general, policy-free stable-span representation primitive.
  Substitute alone owns proof deletion/addition, reconstruction witnesses,
  connected occurrence maintenance, and use of that primitive. Sweep owns the
  V5 proof-rule discovery/commit semantics. `DefaultPolicy` owns admission,
  cadence, effort allowance, and invocation order.
- No rule or kernel primitive reads a benchmark identity, deadline, formula
  classifier, or policy budget to choose the repair. Public V3/V5 guards and
  instance surfaces are unchanged.
- `tools/docs/check-rule-policy-boundary.py` statically permits direct
  `kernel_replace_large_clause_in_place` calls only in Kernel and Substitute.
  Focused `rule_policy_boundary`, Substitute, Sweep, Scheduling, RuleJournal,
  EventTrace, Kernel, and proof tests cover the division.

### Validation state

- Two debug crashes exposed by focused unit tests were repaired through the
  required bounded Claude protocol and source-reviewed: the accidental
  `clause_actual_words` self-recursion, and lazy-stream initialization after a
  touched large clause had already been removed. Claude's ASan/UBSan focused
  suites passed; no performance repair was delegated.
- The post-compaction Release rebuild and complete suite pass 72/72, including
  proof/model, Sweep proofs, documentation, trace-zero-overhead, and static
  rule/policy boundary coverage. `check-progress` and `check-doc-size` have zero
  errors; only reserve warnings remain for the two long-lived semantic docs.
- Mandatory full-width B28+fixed-C5 guard
  `20260823-1510-BC-sweep-representation-guard`, job `1641102`, is complete:
  66/66 valid, 17 model-verified SAT plus 16 proof-verified UNSAT per arm, and
  zero residue. Candidate T105 recovers to 1165.385 s and mp1 remains 22.017 s.
  The no-coverage-loss iteration criterion passes, permitting Rank 27, but this
  is not a performance promotion: candidate/comparator PAR-2 is 1.460, with
  open costs on mod4block (622.404/95.317 s), T105 (1165.385/804.843 s), and
  sted5 (256.125/35.991 s).
- Immutable proof-producing Rank-27 campaign
  `20260823-1520-tierd-rank27-sweep-representation-witness`, job `1641168`,
  times out UNKNOWN at 1200.004 s, so it misses the 300-second target. The
  106,954,752-byte partial proof is hashed and deleted; no proof verdict was
  claimed and residue is zero. Resume latest-current Tactix versus current
  instrumented Kissat from the retained common Rank-27 boundary; Sweep's exact
  semantic/throughput closure and the proof-rule/strategy boundary stay closed.
- Current-Tactix-only macro campaign
  `20260823-1530-tierd-rank27-current-c10000-macro`, job `1641169`, completes
  cleanly. Macro schedule, decisions, and search state remain common through the
  exact conflict-5,580 Retier; at conflict 7,150 Tactix is first visibly three
  decisions and one level ahead. Phase-only campaign
  `20260823-1540-tierd-rank27-current-phase-5580-7150`, job `1641170`, compares
  1,118 normalized target/best records exactly with retained current Kissat.
  Headers remain exact through c6734; at c6787 search is one decision/level
  apart while the complete phase payload remains exact. Phase saving,
  target/best watermarks, and the mp1 compaction repair are excluded as owner;
  paired current/current trajectory job `1641171` now localizes the first split:
  every portable transaction is exact through logical decision 5,374,229, then
  at c6786 stable-heap decision 5,374,230 Tactix chooses external `74855` while
  Kissat chooses `388954`, both with positive target/saved phase. The following
  learned hashes remain exact, but Tactix reaches c6787 one decision/level
  higher. Analysis, learning, phase, and random-decision admission are excluded;
  distinguish score arithmetic from equal-score heap pose next.
- Aligned heap-pop job `1641173` closes that classification: both decision
  candidates have exact score `1.2323120049784528e+101`, eligibility and phase
  agree, and the first pop starts at equal heap size 379,722. Tactix places
  external `74855` above `388954` at path positions 19/39; Kissat has the same
  pair reversed. The first owner is latent stable-heap representation pose, not
  arithmetic or policy. Localize the first post-analysis heap-hash mismatch
  after the exact c5580 macro boundary before changing heuristics.
- Full-heap checkpoint job `1641175` keeps the external-variable stack hash
  exact through c5149 (`3db93bfc10e8210e`) and first observes an unequal hash
  at c5580 (`d20e83b8d02564fe` vs `d9e9e31d8ede9780`) while decisions, level,
  and the complete top-32 score payload remain exact. Tactix and Kissat's heap
  bubble algorithms are source-identical, so narrow the ordered mutation input
  (analysis bump or lazy pop/reinsert) in `(5149,5580]`.
- Narrowing job `1641207` keeps the full heap exact through c5450
  (`b93f6708fdea450b`) and finds it unequal at c5550
  (`393dbe9166313da5` vs `3dca4f1f96a9e831`) under identical decisions, level,
  and mode. Requested counters 5500 and 5579 have no ordinary analysis endpoint
  and support no claim. Continue on reachable conflicts in `(5450,5550]`.
- Reachable-checkpoint job `1641241` keeps the full heap exact at c5490
  (`23740694b4412e4a`) and finds it unequal by c5499
  (`717b8c82c6c4b1f0` vs `f25710113d00d122`) with identical search headers.
  The exact ordered-mutation owner lies in `(5490,5499]`.
- Dense job `1641261` keeps c5491 exact (`f8e1f974e15233a5`) and first samples
  an unequal hash at c5493 (`429ea61fb1df9f7f` vs `037d1a46181fc919`), but its
  predeclared list accidentally omitted c5492. The later complete trajectory
  proves c5492 has a matched ordinary `analysis_end`; the earlier no-endpoint
  interpretation is retracted. c5493 is only the first *sampled* unequal end.
- c5493 bump/heap job `1641277` excludes that analysis as owner. Before its
  first bump the full heap is already unequal (`962618624955c3d9` vs
  `837b5fa45aacabb3`) while top-32 scores are exact; conflict, every learned
  stage, and all nine ordered bump identities are exact. The owner lies after
  exact c5491 and before c5493 analysis, in c5492's special transaction or a
  decision-time lazy pop.
- Complete trajectory job `1641279` compares all 1,059 portable records exactly:
  521 decisions, 521 phase picks, three conflict/analysis transactions, and two
  learned transactions. c5492 is a matched no-learn analysis/backtrack from
  level 3970 to 3452, followed by the exact same 521 decisions through c5493.
  Sample the c5492 full-heap endpoint directly; if exact, binary-search the
  following decision-time lazy pops. Both traces are complete and residue-free.
- Direct endpoint job `1641281` makes c5492 decisive: the formerly exact heap
  becomes Tactix `205428df3fbd4cf5` versus Kissat `765a211140867827`, while
  both heaps have size 406,567 and the full portable top-32 score payload is
  exact. The first owner is ordered reinsertion during c5492's matched no-learn
  backtrack, not later lazy decision pops. Trace the ordered inserted variables
  and their final poses without changing Analyze/Backjump proof semantics.
- Ordered reinsertion job `1641283` compares 1,006 insertions per solver. The
  first 121 are exact; at index 121 Tactix reinserts external 475810 while
  Kissat reinserts 475857 from the same heap size and to the same pose. Locally
  Tactix orders `475808,475810,475814,475857,475918,475863`, versus Kissat
  `475808,475857,475810,475814,475918,475863`. The source-identical heap
  algorithm is excluded; inspect the two assignments' trail positions and
  reasons at the common c5492 conflict.
- Final provenance job `1641293` identifies the inherited owner. External
  475810 is level 3490 with binary reason `-475808` in both solvers (Tactix's
  trail pose carries the known retained-identity offset). External 475857 is
  also level 3490, but Kissat assigns it earlier from a large clause while
  Tactix assigns it later from binary `-475808`. This reason/propagation-order
  split explains the swapped reinsertion order. Localize the Kissat large-watch
  transaction; do not change the heap algorithm or Analyze/Backjump calculus.

## Exact convergence contract

- Compare latest Tactix directly with current instrumented Kissat from the
  latest common portable semantic state. Prioritize the earliest divergence
  where Tactix performs equal normalized work more slowly or produces weaker
  work at comparable cost; record stronger/neutral differences without tuning
  them away unless they block localization.
- Every inprocessing rule must expose compile-time-only matched invocation,
  candidate, logical-cost, terminal-budget, semantic-delta, pose/lineage,
  elapsed-time, and normalized-yield tracing as specified in
  `DEVELOPMENT_PLAN.md` §8.3. Exact cost and candidate-frontier agreement is
  required while semantic states remain common unless a source-backed stronger
  effect or accepted N4 deviation is documented.
- Wall-clock instrumentation is diagnostic only. Strategy remains outside the
  proof rule and kernel, including when a representation repair is motivated by
  one scheduled rule.

## Next actions

1. Trace external 475857's earlier Kissat large-clause implication and Tactix's
   corresponding watch state; localize the first portable propagation owner.
2. Repair only a general source-backed semantic, scheduling, or representation
   owner while keeping proof/reconstruction in rules and cadence/admission/
   effort in strategy. Repeat the mandatory guard before the next witness.
3. At the next coherent successful increment, update this resume surface and
   the parity ledger, generate the required Andrew TeX/PDF report, commit, push,
   and leave `main` clean and synchronized.

## Recent changes

- 2026-08-23 — Localized the first Rank-27 physical heap mismatch to c5492's
  matched no-learn backtrack/reinsertion transaction.
- 2026-08-23 — Traced that pose mismatch to an earlier reason-class/order split:
  Kissat large-clause-propagates 475857 before Tactix's binary implication.
- 2026-08-23 — Proved the split is equal-score heap pose: candidate score and
  eligibility agree, but external variables `74855`/`388954` are reversed.
- 2026-08-23 — Closed the 9,379-seed Sweep mixed-occurrence semantic stream at
  the shared T105 state; accepted trace hash is `a70a89d…17de1`.
- 2026-08-23 — Lifted Kissat's stable-allocation in-place large-clause shrink
  into a general Kernel primitive used only through Substitute.

<!-- track-sched:begin -->
N4 verification remains complete at the accepted parity pin. Living mechanism
dispositions and reopening conditions are in `docs/scheduling-parity-ledger.md`.
The current Sweep semantic and throughput owner is closed at the shared T105
state, but promotion remains blocked until the candidate passes the mandatory
full B28+fixed-C5 guard and then the checked Rank-27 witness.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
