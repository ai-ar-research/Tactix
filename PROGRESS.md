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
  SLURM job `1638010` is unrelated. Post-repair fuzz jobs `1641365`--`1641368`
  are accepted and clean. Full Tier-D array `1641379` and dependent aggregate
  `1641380` are active. Paired Rank-27 c3069 source-handoff observer job
  `1641810` completed and released its four reserved CPUs.
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

- Current-HEAD soundness campaign
  `20260823-1700-current-head-fuzz` is blocked on two native-LRAT rejections in
  the existing Substitute stress scenario: seeds 20261465 and 20265342 in job
  `1641326`, after 894,743 directly counted target applications. The default
  DRAT+converted-LRAT leg `1641325` passes 10,000 cases with 9,631 verdicts.
  Per `DEVELOPMENT_PLAN.md` §8.2, the bounded Claude repair protocol is
  mandatory before the prepared Tier-D campaign may be submitted.
- Claude localized the native-LRAT defect to `kernel_reason_walk`: it re-walked
  a retained clause reason for a root-fixed literal after Vivify could delete
  that root-satisfied clause, emitting a deleted LRAT hint. The general kernel
  repair collapses every root-level reason to its already-emitted unit id, as
  Analyze already does; no strategy code changes. Codex source review and
  independent validation pass 13/13 relevant ASan/UBSan tests, 27/27 corpus
  entries, both Substitute witnesses, and three additional random-policy LRAT
  witnesses. Full post-repair jobs `1641365`--`1641368` accept the repair and
  unblock Tier-D preparation.
- All four post-repair fuzz legs are terminal and clean: random native LRAT job
  `1641365` has 10,000 cases, 0 failures, and 8,772 verdicts with Sweep in
  2,535 and Substitute in 4,280 cases; Substitute-stress native LRAT job
  `1641366` has 10,000 cases, 0 failures, 8,768 verdicts, and 895,214 directly
  counted Substitute applications; default DRAT-to-LRAT job `1641368` has
  10,000 cases, 0 failures, and 9,631 verdicts with Sweep in 2,401 and
  Substitute in 2,312 cases. Forced-Sweep DRAT job `1641367` completes all
  10,000 cases with 0 failures, 7,006 checked verdicts, and 1,438,080 target
  applications. Its predeclared corpus-026 nontermination ran no oracle and is
  not counted as a soundness pass.
- The reusable Tier-D comparison runner's historical rejected/UNKNOWN path
  contradicted the current §8.1 zero-retention authority by moving partial
  certificates into `retained/`. It now requires the manifest's `retention:
  none` contract, hashes models and partial proofs, records diagnostic tails,
  deletes each consumed checker input stage-by-stage, clears every remaining
  certificate/derivative before its sole result publication, and counts any
  legacy retained file as aggregate residue. A new integration regression
  covers UNKNOWN partial proof, rejected UNSAT plus partial LRAT derivative,
  and rejected SAT model paths; all three pass with empty scratch/retention.
  The successor Tactix campaign inherits the stable non-`v1` baseline's
  certificate policy and custom 1800 s/16 GiB resource contract.
- Two debug crashes exposed by focused unit tests were repaired through the
  required bounded Claude protocol and source-reviewed: the accidental
  `clause_actual_words` self-recursion, and lazy-stream initialization after a
  touched large clause had already been removed. Claude's ASan/UBSan focused
  suites passed; no performance repair was delegated.
- The post-compaction Release rebuild and complete suite pass 74/74, including
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
- Rank-27 campaign chain `20260823-1530` through `20260823-1720` first traced
  the visible c6786 equal-score stable-heap split backward to c5492's matched
  no-learn backtrack. The heap algorithms, scores, eligibility, phase payload,
  learned transactions, and ordered bump identities are exact; the reinsertion
  order differs because Kissat had propagated external 475857 earlier from a
  large clause while Tactix later uses binary `-475808`. Frozen manifests own
  every intermediate checkpoint and the retracted "full common semantic state"
  interpretation.
- Reason checkpoints `1641297`, `1641329`, and `1641349` narrow the causal
  reassignment to `(c5471 analysis_end,c5472 conflict entry]`: 475810 has the
  common binary reason, but 475857 has Kissat large-clause ref 1179665 versus
  Tactix binary `-475808`. All paired endpoints are untruncated and residue-free.
- Paired implication job `1641369` completes from separate read-only pins with
  both c5472 endpoints, zero truncation/residue, and trace bundle SHA-256
  `1f38163e64772ab44fe87cad7b2b1af85379fea56628fc70a7abebb22aee6e28`.
  After c5471 all 89 decisions and the first 3,617 implications are exact. At
  implication index 3,617 Kissat immediately derives `-475857` at level 3486
  from large clause `(-475857,-208918,-121233)`, while Tactix next derives
  `-121237` from the common `477074` binary frontier. Tactix derives `-475857`
  only 19 implication positions later from binary `(-475857,-475808)`. This is
  the first portable worse-side propagation divergence; filter the watch pose
  for the `121233` scan next. Paired filtered watch-pose job `1641371` completes
  without truncation/residue; trace bundle SHA-256 is
  `84e7b7540c696b603c4a76f0e0cda3eaceff4c8f0666fc5f87ae2109c23f5a84`.
  Kissat has the focal large clause in pose
  `(-121233,-475857,-208918)` on that list and assigns `-475857`; Tactix has no
  selected entry. The scan differs by exactly one large-watch pair: Tactix is
  58→30 words, 17 dereferences, 3 large assignments, and 60 ticks versus
  Kissat 60→32, 18, 4, and 62. Binary assignments (23) and relocations (14)
  match. Clause-set snapshot job `1641373` then shows this is not a displaced
  watch inside a shared clause: Tactix has no live focal clause at c5471, while
  Kissat retains redundant glue-2 clause `(-121233,-475857,-208918)` with
  `searched=2`, `used=29`, and that watched pair. Trace bundle SHA-256 is
  `acfef3fbc27c11cad1ee202ad4e6d45f11bcdfe2ce3c08e40e5e2a72c8a17030`.
  Localize the earlier clause creation/strengthening/deletion lineage before
  changing propagation. Sparse lifetime observer job `1641376` is
  inconclusive: the formula snapshot hook emitted only its detailed c5471
  endpoint, not the ten earlier requested checkpoints. It repeats the c5471
  result without truncation or residue and supports no lifetime claim. Use one
  bounded endpoint per task next. Corrected job `1641492` proves common absence
  through c3000 and Kissat-only presence by c4000 (persistent through c5000),
  then stops the later redundant tasks. Paired job `1641531` further narrows
  the split to `(3000,3200]`: Tactix is absent at c3200 while Kissat carries
  the focal glue-2 clause at `used=31`; later tasks were cancelled. Correction:
  Analyze also resets used clauses to 31, so this does not alone prove fresh
  creation. Paired trajectory-only job `1641547` records c3001--c3200 while
  omitting millions of matched decisions. All 175 ordinary learned transactions
  match exactly at raw, sorted, shrunk, and final stages, and none is the focal
  clause. The formula split therefore lies in a side transaction such as OTFS
  strengthening/representation rather than worse ordinary clause learning.
  Attempted paired job `1641563` computed formula hashes but omitted
  `KEEP_SNAPSHOTS`, so its intermediate rows were suppressed. It was cancelled
  at 7m38s and supports no presence claim. Corrected job `1641589` retains
  reachable checkpoints c3100/c3150/c3175/c3200 with the same frozen observers.
  It localizes the owner to the final late Substitute application inside the
  shared c3069 inprocessing phase: both are absent at the preceding Sweep exit;
  Kissat Substitute emits/retains the glue-2 focal ternary while Tactix remains
  absent, and that split persists through c3200. Compare representative maps
  and the rewritten source-clause transaction next; Analyze/OTFS is excluded.
  Six-task paired job `1641694` traces c3069 maps for all three focal literals.
  Each remains an active identity representative in every observed round in
  both solvers. Thus Tactix does not map a focal literal away; invert the general
  diagnostic filter to find source literals mapped into those representatives.
  Disabled-by-default output-representative job `1641720` finds exactly one
  discriminating inverse map: final-round Kissat maps `121304 -> 121233` while
  Tactix never does. Late Substitute is only the consumer; the semantic owner
  is the preceding final Sweep equivalence. Paired job `1641738` then shows all
  343 canonical equivalences exact. Target seed 121232 has the same 256 vars,
  325 clauses, 10,704 ticks, 24 solves, and consequence in both. The worse owner
  is the representation handoff: Kissat late Substitute consumes
  `121304 -> 121233`, while Tactix inline Sweep substitution does not
  materialize the focal ternary. Trace unit tests pass; Tier-D is untouched.
  Endpoint job `1641748` observes the expected source ternary identically in
  both at c3069 entry. Kissat is untruncated; Tactix's endpoint row precedes a
  signal-drain trace cap, but `HEAP_POP_ONLY` retry `1641761` is a one-row,
  untruncated exact repeat. A missing input clause is excluded. Tactix's dense
  Sweep session explicitly skips redundant large clauses while priming rewrite
  occurrences. Paired job `1641772` proves that this is not a divergence: each
  solver rewrites the same ordered 16 connected occurrences of `-121304`
  (eight binary, then eight large), and neither visits the common redundant
  source `(-121304,-475857,-208918)`. Both traces are untruncated and residue
  free; SHAs are Tactix `1576f65…d0e1`, Kissat `ba7142d…5c`. Retract the
  suspected Sweep occurrence-ledger repair. The open owner is persistence or
  consumption of the proved equivalence between Sweep and late Substitute.
  Source-handoff job `1641810` closes that question. At the productive final
  Sweep exit, both still hold the common redundant preimage with identical
  metadata. Kissat's Sweep retires zero variables and its following Substitute
  retires 345, removes the preimage, and produces the focal image. Tactix's
  Sweep prematurely retires 343 variables; its following Substitute sees only
  two, leaves the preimage unchanged, and never produces the image. Both traces
  are untruncated and residue-free (Tactix `67fa544…8fed`, Kissat
  `2829de4…bf2a`). The repair now preserves Sweep's proved equivalence binaries
  and active classes through its exact connected irredundant rewrite. Same-batch
  bridge endpoints are suffix-composed as sequential Kissat merges them, so an
  intermediate class winner cannot lose its only link. Canonical Substitute
  now owns residual learned cleanup, reconstruction, and retirement. Independent
  review confirms the rule/strategy boundary and LRAT identities; focused
  ASan/UBSan, 74/74 release tests, and all 44 external proof checks pass. No
  benchmark witness has yet been run on this repair.
  These observers do not mutate the candidate.

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

1. Do not wait for full Tier-D campaign
   `20260823-1745-tierd-current-head-16g-1800s`: array `1641379` runs
   `0-1165%89` at 1800 s/16 GiB and aggregate `1641380` is dependent. Its
   commit `0997356…78c8` binary is an independent read-only inode, SHA-256
   `cb6fdaa9…6c7a4`; compute-node SAT/model and UNSAT/proof validation passed
   with zero residue. Inspect the aggregate when terminal.
2. Freeze a production repair binary and run the mandatory full B28+fixed-C5
   16-GiB/1200-s guard; validate every SAT model and UNSAT proof with zero
   residue before any Rank-27 performance witness.
3. If all guard solves remain, run the checked Rank-27 witness and compare the
   repaired c3069 Sweep→Substitute handoff directly with current Kissat. Keep
   cadence/admission/effort outside the proof rules.
4. At the next coherent successful increment, update this resume surface and
   the parity ledger, generate the required Andrew TeX/PDF report, commit, push,
   and leave `main` clean and synchronized.

## Recent changes

- 2026-08-23 — Repaired Sweep→Substitute ownership with suffix-composed
  equivalence bridges; sanitizer, full-suite, and proof checks pass locally.
- 2026-08-23 — Localized the first worse-side propagation divergence: Tactix
  derives 475857 19 implications later from a weaker binary reason.
- 2026-08-23 — Traced the visible equal-score heap split back through c5492's
  reinsertion order to that earlier reason-class/propagation-order divergence.
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
