# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and the debug contract;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4
evidence. This file is the current resume surface. Frozen manifests and reports
own experimental detail and history.

## Resume anchor

- Worktree `/home/hwu/SAT-solving`, branch `main`. The synchronized accepted
  base before the active increment is batched-ITE wrap-up
  `8001b7f86d08c35e05e6807c415456b295a81b14`. Its accepted comparator is
  frozen as Release binary SHA-256 `9e9f0dab…defa`. The accepted batched ITE
  sign-mask repair is source-diff SHA-256 `4c2cd4a6…591a`, Release binary
  SHA-256 `9e9f0dab…defa`, and ASan/UBSan binary SHA-256 `1e02dd46…fb7c`.
- `.claude/worktrees/tierd-next10` is the sole surviving secondary worktree and
  is clean. No repository commit, push, solver campaign, or benchmark campaign
  occurred after the `e9c161c` wrap-up before this line began; SLURM `1645399`
  is the current interactive shell allocation, not a solver submission.
- On user direction, full Tier-D array/aggregate `1645240`/`1645241` and the
  surviving predecessor `1642950`/`1642951` were canceled at 15:58:01 UTC to
  free capacity for fuzzing and convergence. Current campaign `0343` preserves
  294 classified rows (187 valid, 107 invalid) and predecessor `0010` preserves
  1,165 (689 valid, 476 invalid); both are explicitly non-scorable. Their
  `cancellation.json` records own the exact disposition. Pending, retained,
  scratch, and certificate residue are zero in both campaigns.
- On later user direction, a fresh frozen Kissat 4.0.4 Tier-D reference campaign
  `20260825-0010-tierd-kissat-4.0.4-16g-1800s` was submitted asynchronously as
  array/aggregate `1646252`/`1646295`. Its 1,166 tasks use the accepted Kissat
  binary SHA-256 `3aa8e41f…d49c`, two CPUs, 16 GiB, an exact 1,800-second solver
  timeout, no node constraint, and array throttle `0-1165%40`. Fixed local SAT
  model and UNSAT DRAT→LRAT→Cake validation passed with zero residue before
  submission. The campaign is not yet scorable: dependent aggregate `1646295`
  owns complete validity, solve-loss, independent-check, and cleanup
  classification after every array task terminates.
- Fuzzing ownership is now explicit in `DEVELOPMENT_PLAN.md` §8.2: Claude Code
  prepares, submits, monitors, inspects, and reports every solver fuzz job.
  Codex does not probe fuzz logs or artifacts and accepts Claude's terminal
  pass/failure report; failure returns directly to Claude's bounded repair
  workflow. Benchmark guards, proof witnesses, and performance diagnostics
  retain their separate independent-validation contracts.
- 2026-08-25 active Vivify convergence checkpoint — Compact propagation-only
  watch spans are closed negative and reverted by exact common-state job
  `1646178`: all non-time work is identical, but Rank-27 conflict-9450 total
  time regresses 16.35→17.72 s, Vivify 2.029→2.308 s, and RSS rises. The
  surviving general repair instead exposes Kernel's accepted cached-key stable
  radix Ref ordering to Vivify, removing its residual comparison stable-sort.
  Exact stage `1646181` preserves every non-time counter and lowers Vivify
  2.106→1.865 s; event trace `1646183` is byte-identical for all 64 rows to
  frozen SHA `7a6216bb…7cc1`, fixing rule/candidate order, exact tick/cost,
  scheduling, learning, pruning, and resulting state. Complete frozen B28+C5
  array/aggregate `1646184`/`1646250` passes 66/66 independently checked tasks:
  each arm has 17 valid SAT models and 16 valid UNSAT proofs, all 33 normalized
  non-time pairs match, invalidity/solve-loss/cleanup/residue are zero, Vivify
  falls 198.247→162.431 s (18.07%), and candidate/shipped PAR-2 is 0.978814.
  Summary SHA is `18a60d2f…9e5eb6c`. Dependency-serialized complete Rank-27
  pair `1646251` is exact-state UNSAT on both arms and lowers Vivify
  64.399→58.191 s (9.64%) and inprocessing 192.415→186.927 s. Its total
  515.505→520.897 s sequential delta is invalid for whole-run pricing: the
  unchanged predecessor itself drifted roughly 116 s above the accepted
  approximately 399-second witness, while about 40 tasks from independently
  submitted Tier-D array `1646252` shared the nodes. That concurrent load is a
  plausible environmental cause, not a proved attribution, and the candidate's
  later 5.392-second delta is neither a demonstrated regression nor a win.
  RSS falls 631,500→630,336 KiB and cleanup is zero. Proof-producing Rank-27
  witness `1646346` is valid exact-trajectory UNSAT in 535.225 s internal /
  535.781 s wall under the same external contention. Pinned `drat-trim`
  verifies the byte-identical 1,514,395,111-byte proof SHA
  `0f99f55a…8bb2d` in 131.493 s; cleanup and certificate residue are zero.
  Claude Code's terminal report closes exact-source ASan/UBSan native-LRAT
  fuzz job `1646352` as PASS: corpus 27/27 and random 10,000/10,000 have zero
  failures, with 4,966 checked SAT models, 3,451 checked UNSAT proofs, direct
  reachability in 5,865 Vivify / 4,306 VivifyChanged / 6,127 Propagate cases,
  zero sanitizer/crash/invariant finding, and zero certificate/job residue.
  Codex accepted that report without inspecting fuzz artifacts. Candidate base
  is `a4b11964…37da4276`, source-diff SHA `65e9489b…442824`, Release pin SHA
  `067f7f3a…5070bd`. Proof and fuzz correctness requirements are closed.
  Whole-run promotion remains open because job `1646251` used sequential arms
  with a badly drifting predecessor control. Initial control allocation
  `1647189` failed before either solver launch because its
  nested `srun` steps inherited mutually exclusive SLURM memory variables; the
  corrected wrapper starts both arms directly inside the same reserved
  four-CPU allocation. Corrected paired job `1647195` started both arms in the
  same second, but was canceled on user clarification that concurrent Tactix
  processes invalidate the requested standalone absolute-time observation; it
  has no terminal claim and zero residue. Standalone immutable Vivify-only
  Rank-27 job `1647203` now runs Release without tracing or proof, with two
  CPUs, 16 GiB, and 1,200 s. Require exact effects/state; repeated >500 s
  reopens the implementation under the user criterion, while a return near the
  accepted approximately 399 s rejects the earlier absolute timing as
  non-reproducible.
  Terminal result: valid UNSAT in 583.796 s internal / 584.33 s wall at
  651,808 KiB, so the candidate fails the user-directed >500 s reopening
  threshold. Every normalized non-time line remains byte-identical, including
  Vivify's 300 rounds / 198,109 vivified / 91 deleted / 1,046,683 checked and
  every detailed effect, search, learning, pruning, scheduling, BVE, and
  retained-state counter. Thus no unintended Vivify semantic/effect change is
  present, but promotion is blocked. Run the immutable unchanged predecessor
  standalone under the identical wrapper next: near 399 s rejects/reverts the
  Vivify repair; near 584 s proves the current load reproduces independently
  and requires a clean-load control before decision.
  Immutable unchanged-`a4b11964` standalone causal control `1647229` is now
  submitted with the identical proofless Release wrapper, two CPUs, 16 GiB,
  and 1,200 s. Do not resume BVE until its terminal timing/state separates
  candidate causality from the continuing cluster environment.
  Control result: unchanged predecessor is exact-state UNSAT in 580.077 s
  internal / 580.60 s wall, so the >500 s absolute slowdown reproduces without
  Vivify. Candidate/predecessor is 583.796/580.077 (+0.641%); candidate Vivify
  is 60.910/70.742 (-13.90%) and inprocessing 194.157/204.559 (-10.402 s),
  while outside-rule search/scaffolding varies +14.122 s. All non-time lines
  are byte-identical. SLURM TotalCPU is 581.774/578.196 s over 586/582 s
  elapsed, ruling out CPU descheduling; the node remains saturated by roughly
  40 independent Tier-D tasks, so frequency/cache/memory-bandwidth contention
  remains plausible. Absolute causality is closed against Vivify, but its
  +0.641% total is not promotion-quality: run a clean/exclusive two-arm control
  before acceptance or BVE resumption.
  The partition rejected an exclusive-node request before assigning a job
  because that configuration is unavailable under the frozen two-CPU/16 GiB
  request. Replacement two-node crossover `1647252` preserves the contract and
  uses no node constraint: phase 1 places one arm per node, phase 2 swaps the
  binaries on the same rank-to-node mapping. No two Tactix processes share a
  node; exact per-node and aggregate comparisons will close the residual
  +0.641% timing ambiguity. Exclusive attempt created no allocation/artifact.
  Crossover `1647252` closes the ambiguity with four exact-state UNSAT arms at
  99% CPU and zero residue. Node017 candidate/predecessor is
  556.510/583.044 s (-26.534), while node018 is 472.229/450.091 s (+22.138):
  opposite signs prove node/phase conditions dominate a single whole-run
  comparison. The balanced crossover cancels additive node and phase bias:
  candidate/predecessor is 514.3695/516.5675 s (-2.198, ratio 0.995745),
  Vivify 57.097/64.5305 s (-11.52%), and inprocessing
  183.5095/191.2535 s (-7.744). Every non-time/effect image is byte-identical.
  Therefore Vivify did not cause the 399→580 jump; the historical absolute
  time is currently unreproducible by either binary. Combined with exact
  prefix/profile, complete B+C ratio 0.978814, checked proof, and Claude fuzz
  PASS, the general radix-order representation is promoted. The final
  BVE-free Release rebuild reproduces candidate SHA `067f7f3a…5070bd`; the
  suite is 73/74 with every executable/correctness/boundary/cleanup test green
  and only the recorded opt-in documentation-size debt failing. Required
  three-page Andrew report is
  `report-to-andrew/2026-08-25-0825-rank27-vivify-radix-order/`, TeX SHA
  `13bb9a62…2240`, PDF SHA `331591c1…5385`, with no LaTeX box warning.
- 2026-08-25 residual-BVE resume checkpoint — User directed convergence to
  continue with BVE while Claude closes the independent Vivify fuzz report.
  Accepted full profiles remain the source authority: on the current
  3,813,669-conflict trajectory Tactix spends about 30--39 s in BVE versus
  current optimized Kissat's 16.32 s, while the antecedent-size admission,
  definition tick/session lifecycle, compact Kitten map/core, and optional
  backward-tail investigations are already closed. Source comparison names a
  remaining representation asymmetry before proof or strategy: Kissat keeps
  pivot antecedents and generated resolvents as references/slices in reusable
  raw stacks, whereas Tactix still gives every materialized antecedent and
  every retained resolvent separate `std::vector` literal/proof-chain
  ownership. The accepted callgraph already identified allocation/marshalling
  as the dominant historical BVE class, and the current flat profile still
  carries material allocator/memmove cost. Price a reusable flat-slice arena
  first, preserving antecedent order, pair order, exact ticks/bounds, proof
  chains, candidate heap/schedule, pruning, and resulting state; no admission
  cap or completed proof rule is reopened.
  Per subsequent user direction this remained a narrow staged prototype while
  Vivify's whole-run comparison was unresolved. That blocker is now closed.
  Its source is preserved immutably in
  `benchmarks/runs/20260825-0729-bve-flat-slice-prototype/` and absent from the
  promoted Vivify source. Resume with local Release/sanitizer checks and one
  exact paired BVE-heavy Tier-C T105 mechanism test; do not launch a full B+C
  or Rank-27 BVE campaign until that stage is positive.
- Current/current exact-state jobs `1645876`/`1645878` localize the earliest
  Rank-27 common-state loss inside Congruence: complete Tactix/current-Kissat is
  2346/1644 ms, AND 283/568 and XOR 89/269 ms favor Tactix, but ITE is 630/117
  ms. Tactix performed six exact ternary membership probes for each canonical
  record; Kissat obtains the same sign combinations from literal-local dense
  watches. A compact per-key hash is rejected and reverted by exact-state job
  `1645880` because it worsens ITE 646→938 ms and Congruence 2355→2715 ms.
- The accepted representation repair batches the eight exact ternary sign
  patterns per unsigned three-variable prefix while retaining canonical record
  iteration. Exact stage/profile jobs `1645882`/`1645884` preserve every
  non-time counter, cut the membership lambda 475.813M→130.659M cycles,
  gate extraction 983.981M→714.124M cycles, and total sampled cycles
  63.890B→63.485B. Candidate trace `1645886` is byte-identical for all 64 rows
  to accepted SHA `7a6216bb…7cc1`, including rule/candidate order, exact
  tick/cost, scheduling, learning, pruning, and resulting state.
- Full B28+fixed-C5 guard `1645888`/`1645954` passes 66/66 independently
  checked tasks: each arm has 17 valid SAT models and 16 valid UNSAT proofs,
  all 33 pairwise non-time statistic images match, candidate/shipped PAR-2 is
  1.0073877x, and invalidity, solve loss, cleanup failure, and certificate
  residue are zero. Summary SHA-256 is `4bc3326c…a7488`.
- Exact-source ASan/UBSan native-LRAT fuzz `1645887` passes 10,000 cases with
  zero failures: 2,628 SAT models and 3,490 UNSAT proofs are checked,
  Congruence fires in 3,570 verdict cases for 1,259,092 target applications,
  and sanitizer/model/proof/crash findings plus certificate/job-temporary
  residue are zero. The 3,882 five-second nonterminations make no oracle claim;
  25 retained CNFs are declared diagnostic inputs only.
- Checked two-CPU Rank-27 job `1645956` is valid trajectory-exact UNSAT in
  409.438 s internal / 409.957 s wall. Pinned `drat-trim` verifies the
  byte-identical 1,514,395,111-byte proof SHA `0f99f55a…8bb2d` against the
  original CNF and cleanup is zero. Congruence improves 16.910→16.154 s; the
  +5.583 s total delta is identical-trajectory timing noise outside that owner,
  consistent with exact common-state cycles falling 0.64%. The repair is
  promoted, Rank 27 remains above the 300-second milestone, and convergence
  continues across every justified current-Kissat gap.
- Residual ITE observer `1645957` reproduces the accepted conflict-9450 state
  and localizes 210 of 415 ITE milliseconds to 4,585,536 conditional-pair
  inserts through `unordered_map`; collection alone is 1.79x inherited current
  Kissat's complete 117 ms ITE stage. The portable active candidate replaces
  that map with a dense literal-to-sparse-slot image while retaining vectors
  only for touched literals. Exact paired job `1645958` keeps normalized
  solver SHA `3b87bb36…2c5f`, including 445,868,596 ticks and every candidate,
  schedule, learning, and pruning aggregate; ITE falls 572→184 ms, complete
  Congruence 2383→1953 ms, prefix 16517→16147 ms, and RSS 406028→394776 KiB.
- Paired profile `1645960` is exact and lowers total sampled cycles
  65.038B→62.876B (0.96675x), ITE membership 414.600M→163.410M, and
  `congruence_apply` 1.009B→0.803B; 366.783M hash-node insertion cycles
  disappear with no ownership displacement. Event trace `1645961` is all 64
  rows byte-identical to frozen SHA `7a6216bb…7cc1`. Frozen full-width B28+C5
  array/aggregate `1645964`/`1646030` passes 66/66: each arm has 17 valid SAT
  models and 16 valid UNSAT proofs, every non-time pair is exact, PAR-2 ratio is
  1.0126918x, and invalidity, solve loss, cleanup failure, and certificate
  residue are zero. Exact-source fuzz `1645963` completed 10,000 random cases
  with zero failures before the Claude-owned-fuzzing policy took effect: 2,624
  SAT models and 3,484 UNSAT proofs were checked, Congruence reached 3,564
  verdict cases for 1,253,848 applications, and certificate/job residue is
  zero. Future fuzz jobs and reports are exclusively Claude Code-owned.
- Checked two-CPU Rank-27 witness `1646031` is valid trajectory-exact UNSAT in
  399.152 s internal / 399.668 s wall. Pinned `drat-trim` verifies the exact
  1,514,395,111-byte proof SHA `0f99f55a…8bb2d`; every non-time statistic is
  byte-identical to predecessor SHA `8cd3eda4…2da7`, and cleanup is zero.
  Congruence improves 16.154→14.720 s, total internal time improves 10.286 s,
  and peak RSS falls 643,976→604,272 KiB. The repair is promoted; Rank 27 still
  exceeds the 300-second milestone and convergence continues.
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
  Frozen Kissat 4.0.4 proves UNSAT in 278.187 s. A checked Tactix UNSAT within
  300 s/16 GiB is the current engineering milestone, not a stop condition:
  continue merging every justified portable current-Kissat gap after crossing
  it. Every Rank-27 job uses `-c 2`.

## Immutable guard and comparator

- Never rerun frozen scoring references. Current instrumented Kissat may be
  used only as a source observer; its result is not scoring evidence.
- The accepted regression comparator remains the `tactix-shipped` pin frozen
  by the guard manifests; it is not the current-Kissat divergence target.
  Latest accepted full B28+fixed-C5 authority is ITE-dense-slot campaign
  `20260824-2032-BC-ite-dense-slot-guard`, array/aggregate
  `1645964`/`1646030`: 66/66 checked, zero residue, all 33 non-time images
  exact, and candidate/comparator PAR-2 ratio 1.0126918049. Its summary SHA-256
  is `4675a3f2…ac4f`.
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

- The earlier Substitute native-LRAT reason-lifetime defect was repaired through
  the mandatory bounded Claude protocol, independently reviewed, and accepted
  by four 10k post-repair fuzz legs (`1641365`--`1641368`) with zero failures.
  Frozen campaign manifests own the seeds, coverage, and proof-check details.
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
- Two earlier debug crashes also followed the required Claude repair/review
  path; frozen reports own their focused sanitizer evidence.
- The post-compaction Release rebuild and complete suite pass 74/74, including
  proof/model, Sweep proofs, documentation, trace-zero-overhead, and static
  rule/policy boundary coverage. `check-progress` and `check-doc-size` have zero
  errors; only reserve warnings remain for the two long-lived semantic docs.
- Handoff guard `20260823-2025-BC-sweep-substitute-handoff-guard` first lost
  T105 at 1200 s; the causal extra Sweep environments were repaired by matching
  Kissat's non-representative-endpoint decline. The exact current replacement
  guard is `20260824-0020-BC-bve-one-sided-unit-lrat-guard`, array `1642853`
  plus aggregate `1642857`; it remains the only Rank-27 promotion authority.
  It is 64/66 valid and residue-free; candidate mp1 is 18.848 versus 400.680 s.
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
  benchmark witness has yet been run on this repair. Paired semantic job
  `1642305` closes the exact handoff: final Sweep loses zero active variables
  and ends at 751,798 binaries in both solvers; late Substitute retires 345 and
  ends at 751,108, consuming the common preimage. Tactix-only focal job
  `1642495` directly observes the exact glue-2 image with inherited source order
  and metadata (trace `a1ec171…3489`); both jobs are untruncated and residue-free.
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

1. Do not wait for Tier-D replacement `20260824-0010-tierd-pin099-harness-nfs-16g-1800s`: array `1642950` is `0-1165%85` at 1800 s/16 GiB; aggregate `1642951` depends on it.
   Its commit-`0997356` solver is an independent read-only inode (70873282, mode 0550), SHA `cb6fdaa9…6c7a4`; both repaired runners are separately frozen read-only.
   Compute validation `1642949` passed both verdicts with zero residue. At 575 durable results, eight UNSAT proofs pass `drat-trim` before Cake-LPR exhausts its fixed heap, two trivial proofs print VERIFIED but exit 1, and one 2.15 GB proof exhausts `drat-trim`'s 16 GiB task before conversion; all eleven remain checker-invalid. These are checker/harness failures, not soundness failures; all artifacts are deleted.
   Durable journals/NFS retries precede publication; cleanup cannot rerun work. Dedicated tests pass. Reuse no result from removed array `1641379`.
2. The non-representative Sweep-bridge repair closes all 2,303 c3069
   environments, all 3,701 c5471 implications, and every decision through
   c10000. Guard `1642670` exposed a DRAT-only duplicate-population deletion on
   `square.2.0.i.smt2-cvc4.cnf.xz`; native LRAT verified. Proof-shadow localized
   it to Substitute phase E deleting a transient equivalence lemma identical to
   a live source/bridge binary. DRAT now retains that harmless lemma, LRAT still
   deletes by exact id, and DRAT/LRAT/shadow plus 74/74 tests pass. Superseded
   arrays `1642757` and `1642814` plus their aggregates were canceled after
   preserving completed results; only final-source `1642853` may promote.
3. The untruncated c10000--c20000 control (`1642725`) first exchanges two
   equal-score decisions at c14456 and materially diverges at c15385. Tactix
   reports 553,691 BVE applications / 265,013 eliminations / 509.965 s versus
   Kissat 570,204 attempts / 271,829 eliminations / 4.76 s: comparable work and
   yield, so this is the first strong throughput lead. No-trace control
   `1642748` (independent read-only SHA `7891309e…1ae4`) confirms it: identical
  semantic counters, 516.069 s total and 479.747 s BVE. Flat profile `1642749`
  assigns 85.28% of samples to `SubSolver::add_clause`'s quadratic import scan.
  The general signed-mark import repair passes sanitizers and 74/74; frozen job
  `1642755` preserves every BVE counter and cuts BVE 479.747→42.677 s (11.24x).
  Compact exact job `1642753` keeps candidate order through ordinal 89,153 and
  localizes the first cost/pruning loss much earlier, ordinal 627/ext 396947:
  both enter at tick 685; Kissat's one-sided definition core learns a unit and
  exits before a plan, while old Tactix plans four pairs and spends two ticks.
  Source lift `1642803` confirms ownership at the identical decision/conflict
  endpoint: one unit, exactly two fewer resolutions, one fewer later candidate,
  and unchanged 265,013 eliminations. DRAT retains identity-ambiguous temporary
  lemmas; native LRAT deletes them by exact id after the unit, as Kissat does.
4. All cumulative sanitizer fuzz legs are clean. Exact-source native-LRAT job
   `1642864` completed 10,000 cases with 0 failures and 8,355 checked verdicts;
   Release is 74/74. Final B28+fixed-C5 guard `1642853` completed 66/66 but is
   rejected: candidate T105 timed out at 1200.004 s versus shipped 852.488 s.
   Its partial proof was deleted with zero residue; no Rank-27 witness is legal.
5. `SP-BVE-OCC-1` now caches Kissat's physical dense-stack sizes at the BVE
   strategy/representation boundary instead of rescanning every binary watch
   twice per heap update (22.57% of the repaired profile). Binary mutations and
   large membership edits update the image before score callbacks; lazy large
   garbage remains counted. BVE proof semantics are unaware. Release 74/74 and
   focused ASan/UBSan pass. Checked terminal `1643270` preserves every counter
   and the identical proof SHA while cutting BVE 335.575→253.438 s and total
   1427.564→1362.569 s, still above 1200 (frozen Kissat: 261.089 s). Profile `1643271` assigns 46.21% to clause-order hashing. Radix-only `1643292` and complete no-sort `1643344` time out cleanly; direct ID-order gate `1643383` preserves every counter/proof byte, verifies UNSAT, and cuts total to 712.210 s; repeat `1643407` confirms. Fuzz `1643265` is clean. Current/current `1643493` names equal-yield elimination cost (Tactix BVE+forward 291.4 s vs Kissat 90.2 s). Dense-capacity/radix gate `1643546` is exact checked UNSAT at 639.990 s with the same proof bytes/counters, 72.220 s faster; DWARF `1643504` attributes 20.63% of cycles to `bve_backward`'s per-elimination full scratch reset. Dense lifecycle fuzz `1643568` is clean (8,744 checked verdicts, 1,446,762 target applications; backward not reached). Touched-only persistent marks preserve that proof rule; 74/74 plus focused sanitizers pass, and exact gate `1643660` is checked UNSAT at 507.293 s with BVE 95.065 s versus 247.859 s, identical non-timing statistics/proof SHA, and zero residue (132.697 s wall recovered). Scratch-specific fuzz `1643661` runs. Fused-forward arm `1643685` is proof-clean and moves to 410,779 conflicts with 657,618 subsumptions plus 975,452 strengthenings, but regresses wall 507.293→545.285 s because Strengthen costs 55.971 s versus current Kissat's 6.50 s; admission-only promotion is rejected. Frozen callgraph `1643763` assigns 8.31% of all cycles inclusive (6.62% self) to `find_partner`, versus negligible candidate sorting/allocation. Kissat locally accumulates forward steps before publishing; Tactix did a shared-stat/budget reload for every partner, so the same local-ledger representation now passes 74/74 plus focused sanitizers and checked fused gate `1643790` runs. Direct-order guard `1643451`/`1643456` passed 66/66, zero residue, ratio 1.071, but its prepared Rank-27 witness is superseded and will not run; the latest cumulative pin still needs its own guard.

## Recent changes
- 2026-08-24 — Promoted dense literal-to-sparse-slot ITE list ownership after
  residual accepted-source tracing
  `1645957` identifies conditional-list hash ownership; the dense
  literal-to-sparse-slot candidate passes focused Release and ASan/UBSan,
  exact-state stage `1645958`, cycles profile `1645960`, and exact 64-row trace
  `1645961`. It cuts conflict-9450 ITE 67.8%, complete Congruence 18.0%, and
  total sampled cycles 3.32% without changing exact ticks, candidate/order,
  schedule, learning, pruning, or state. Mandatory B28+C5 guard
  `1645964`/`1646030` passes 66/66 with exact non-time pairs and zero residue,
  and exact-source fuzz `1645963` completed 10k/0 before fuzz ownership moved
  exclusively to Claude Code. Checked Rank-27 `1646031` verifies the identical
  1.514 GB proof with zero residue and improves 409.438→399.152 s internal,
  Congruence 16.154→14.720 s, and RSS 643,976→604,272 KiB. The milestone is
  still missed and the full Tier-D campaign remains canceled. The required
  three-page Andrew handoff is
  `report-to-andrew/2026-08-24-2253-rank27-congruence-ite-dense-slot/`;
  TeX SHA `c4a7f7b1…53cf`, PDF SHA `544a817c…e4827`, with no final-pass LaTeX
  box warning.
- 2026-08-24 — Promoted batched exact ITE ternary sign-pattern membership after
  current/current localization `1645876`/`1645878`, rejecting/reverting the
  negative compact hash `1645880`, and exact stage/profile/trace evidence
  `1645882`/`1645884`/`1645886`. Both prototype unit failures followed the
  mandatory bounded Claude workflow and independent Release/ASan/UBSan review.
  Full B28+C5 guard `1645888`/`1645954` passes 66/66, exact-source native-LRAT
  fuzz `1645887` passes 10k/0, and checked Rank-27 `1645956` verifies the exact
  1.514 GB proof with zero residue. Congruence improves 16.910→16.154 s on the
  exact trajectory; total 409.438 s remains above the 300-second milestone due
  broad timing noise outside the owner. Per user direction, crossing 300 will
  not stop work while portable current-Kissat gaps remain. The four-page Andrew
  handoff is
  `report-to-andrew/2026-08-24-2015-rank27-congruence-ite-sign-mask/`;
  TeX SHA `204bc73f…6e63`, PDF SHA `861a1ed8…6e8`, with no final-pass LaTeX
  box warning.
- 2026-08-24 — Complete-dense forward repair passes random native-LRAT fuzz `1644278` (10k/0, 8,763 oracle verdicts, C1/C2 in 2,430/2,409, zero residue); frozen pin `9574b9…a555` guard `1644281`/`1644284` now passes 66/66 (33/33 each arm, 17 SAT+16 UNSAT, no regression/residue, 0.7702× PAR-2). Same-phase-only `1644279` is rejected/reverted for worse pruning/total. Old-pin guard `1644056`/`1644059` passes 66/66 at 0.701× PAR-2. Exact guarded Rank-27 `1644280` is checked UNSAT in 726.736 s, proof verified/deleted, so misses 300 s. Terminal current-Kissat `1644331` makes BVE the exact same-work/slower owner: 1,605,224 Tactix applications/350,597 eliminations/288.549 s versus 1,621,010 Kissat attempts/364,315 eliminations/26.16 s (11.03×); Tactix has fewer conflicts, 3.25M versus 4.12M. DWARF `1644319` localizes definition marshalling: `bve_find_definition` self 13.65%, ascending var sort 6.14%, malloc 5.69% (4.13% from import), reset 2.25%, import self 2.05%, embedded solve only 0.32%, backward tail 0.19%. Scratch-only checked T105 `1644348` is exact positive: proof/counters unchanged, wall 451.657→423.326 s and BVE 106.744→98.626 s, zero residue. Exact radix/direct-map gate `1644352` preserves every counter and proof byte; BVE is 97.495 s versus 98.626 s but noisy total wall is 436.342 s versus 423.326 s, so T105 throughput is inconclusive while the Rank-27 profile remains its source authority. Checked cumulative backward-off `1644353` verifies UNSAT with zero residue and reproduces the earlier positive trajectory (444,439 conflicts, 3.706M decisions, 2.286M fused pruning actions); internal time is 430.749 s versus simultaneous on-arm 433.305 s. Exact first-core Kitten search `1644355` is checked positive at 435.493 s wall / 97.253 s BVE, 441,522 conflicts and 1.843B saved pairs. Initial two-core `1644356` is proof-clean but negative at 500.451 s / 131.884 s BVE, 474,453 conflicts and 1.221B saved pairs. New current/current mp1 core tracing localizes the cause before the first second core: Tactix started the cleared embedded Kitten at seed 0 rather than Kissat's seed 1 and omitted the learned-empty `new_reference` tick. The general engine/strategy repair restarts the embedded lifetime per BVE slot and charges that tick; all 16,395 common-state definition rows now exactly match candidate, import, core hashes, ticks, budgets and generator. Frozen-source fuzz `1644354` is complete green and older two-core fuzz `1644357` remains green past 7,300/10,000; exact-source fuzz `1644359` and repaired checked T105 `1644358` are running. Exact pin `8048eec…f7e` is frozen into prepared B28+fixed-C5 guard `20260824-0265` and blocked Rank-27 witness `20260824-0270`; neither may run until T105 is non-negative and exact-source fuzz is green.
- 2026-08-24 — Repaired checked T105 `1644358` is positive and proof-clean at
  418.251 s wall / 415.394 s internal / 112.447 s BVE and 351,851 conflicts,
  versus 435.493 s / 441,522 for first-core, 500.451 s / 474,453 for the bad
  lifecycle, and current Kissat's diagnostic 453.92 s / 393,595. The extra
  two-core work is locally dearer but produces a stronger trajectory. Exact-source
  fuzz `1644359` is now the only gate blocker for frozen pin `8048eec…f7e`;
  B28+fixed-C5 guard `20260824-0265` and Rank-27 witness `20260824-0270`
  remain prepared and blocked on that fuzz result.
- 2026-08-24 — Exact-source lifecycle fuzz `1644359` passes 10,000 cases with
  8,758 checked verdicts, 1,455,446 BVE stress applications, zero failures,
  and zero certificate residue. The exact pin `8048eec…f7e` B28+fixed-C5
  guard is now array `1644360` at `%2` with dependent aggregate `1644361`;
  the Rank-27 witness remains blocked until that aggregate passes 66/66.
- 2026-08-24 — Guard task 63 validates exact-candidate `mp1-Nb5T15` SAT but
  reopens its performance closer: 240.787 s, 5,429,547 conflicts and 33.48M
  decisions versus the earlier accepted-source 18.848 s and frozen Kissat
  76.688 s. BVE's aggregate yield counts remain identical (2,925 eliminated,
  124,834 resolutions, 2,925 definitions, 5,850 saved pairs), and exact
  common-state definition records match current Kissat, so this may not be
  recovered by reverting the Kissat effort/lifecycle repair. After Rank-27
  pricing, localize the earliest downstream current/current worse-side event.
- 2026-08-24 — Exact lifecycle guard `1644360`/`1644361` passes 66/66
  (candidate and shipped each 17 SAT + 16 UNSAT), zero residue and no coverage
  loss; candidate PAR-2 1513.879 s versus shipped 2060.188 s (0.73483×).
  Candidate T105 is 393.968 s versus shipped 781.349 s. This authorizes exact
  pin `8048eec…f7e`; proof-producing Rank-27 witness `1644427` is running at
  2 CPUs, 16 GiB and 1200 s against immutable input `5435b1…b4e9`.
- 2026-08-24 — Exact guarded Rank-27 `1644427` is valid checked UNSAT in
  579.772 s with its 1.633-GB proof verified in 104.877 s and deleted with zero
  residue. This improves `1644280` by 146.964 s and BVE 288.549→174.204 s,
  but misses the 300-s objective and frozen Kissat's 278.187 s. Current/current
  BVE remains same-scale/slower: 1.710M Tactix applications in 174.204 s versus
  1.621M Kissat attempts in 26.16 s. On mp1, exact candidates/plans/definitions
  and 2,925 retired variables still lead Tactix to collect only 5,957 redundant
  clauses versus Kissat's 5,985, leaving exactly 28 extra clauses; localize this
  first post-BVE worse-side semantic boundary without reverting accepted work.
- 2026-08-24 — mp1 cleanup dump `1644428` retracts a new-retirement-bug
  hypothesis: only 5,732/7,613 redundant clause hash/size entries are common,
  with 1,881 solver-specific clauses per side; Tactix-only has 1,647 retired +
  234 surviving versus Kissat-only 1,675 + 206, explaining the net 28. This is
  downstream confirmation of frozen authority `20260823-0840`, which localized
  the first formula split to Tactix's stronger redundant Strengthen band, and
  `20260823-0850`, which rejected disabling it (Tactix >300 s, current Kissat
  103.40 s). Do not duplicate/unfix that arm; return to Rank-27 BVE throughput.
- 2026-08-24 — Exact-pin Rank-27 DWARF `1644430` finishes UNSAT in 581.432 s
  under perf with 1.710M BVE applications / 177.221 s and no residue. BVE
  definition remains the comparable-work/slower owner: `SubSolver::reset`
  5.40% self, add 3.43%, shuffle 3.38%, core shrink 2.72%, radix map 2.37%, but
  embedded solve only 0.98%. Line samples split reset between watch clearing
  (46.36%) and an all-clause occurrence index (51.96%) that Kitten mode never
  reads. The general candidate now keeps that safety index only for Fixed-mode
  flips, matching upstream Kitten's watched-clause representation; Release is
  73/74 with only the pre-existing documentation-size gate outstanding, the
  two-mode 100k oracle and focused sanitizer proof tests pass. Exact-source
  fuzz `1644431` and full-width B28+fixed-C5 guard `1644432`/`1644498` now gate
  Rank-27 pricing of frozen binary `19593e…be64`.
- 2026-08-24 — While that frozen pin validates, the next independent
  representation increment removes BVE's measured 2.37% radix renumbering:
  its already-deduplicated variables remain in first clause/literal occurrence
  order, exactly the order Kitten then re-imports, and the same vector reverses
  proof literals to kernel variables. Focused Release and ASan/UBSan proof
  suites pass; bounded Tactix-only mp1 trace `1644499` matches all 16,395
  payloads byte-for-byte to the existing exact common-state trace (normalized
  SHA `10d8f14f…278cd`) without rerunning Kissat, and leaves zero residue.
- 2026-08-24 — Source comparison exposes the larger retained-watch mismatch:
  upstream `kitten_clear`, core shrink, and shuffle visit only the previous or
  current active literal prefix while preserving high-water capacities;
  Tactix visited every historical outer-vector entry. The follow-on candidate
  now uses the same active prefix (empty tail lists consumed no random draws),
  passes focused Release and sanitizer proof tests, and exact trace `1644500`
  matches the unchanged 16,395-row payload image (same normalized/whole-trace
  SHAs as the frozen exact run) with zero residue.
- 2026-08-24 — The frozen occurrence-only guard's candidate T105 task 65 is
  valid checked UNSAT with zero residue and preserves the lifecycle pin's exact
  351,851 conflicts, 4,047,685 decisions, 796,773,998-byte proof, and proof SHA
  `8aeeae6e…dd4`.  The representation specialization cuts internal solve
  415.394→367.591 s and BVE 112.447→99.644 s (checked wall 418.251→370.588
  s).  This is positive source-backed throughput evidence, but the exact guard
  still awaits its final shipped T105 task and aggregate, and frozen-source fuzz
  is still running; neither the occurrence-only nor cumulative Rank-27 witness
  is yet authorized.
- 2026-08-24 — The cumulative occurrence + first-occurrence + active-watch pin
  is now immutable as Release SHA `03d80329…ce0` / debug SHA `5c9220ee…7e5`
  against source-diff SHA `0f1d42c3…446`.  Release remains 73/74 with only the
  documentation-size gate, focused debug proof tests are green, and frozen
  native-LRAT fuzz `1644501` plus the full-width B28+fixed-C5 guard
  `1644502`/`1644568` are running.  Its Rank-27 witness remains blocked until
  both exact-pin gates pass.
- 2026-08-24 — `SP-BVE-DEF-COMPACT-1` removes the next redundant
  representation layer: BVE's proof-reversible variables are already dense in
  first clause/literal occurrence order, so an engine-owned compact-Kitten
  entry point constructs the identical VMTF queue directly and skips a second
  identity import/export map on every clause literal. Sweep retains its real
  external mapping; proof-rule and strategy ownership are unchanged. Focused
  Release/debug proof suites pass, and bounded mp1 `1644569` preserves all
  16,395 exact definition payloads plus normalized/whole trace SHAs
  `10d8f14f…278cd` / `268d35ec…601`, with zero residue. This follow-on is not
  part of the already frozen `03d80329…ce0` guard pin.
- 2026-08-24 — Occurrence-only guard `1644432`/`1644498` passes 66/66:
  both arms are 17 SAT + 16 UNSAT, candidate PAR-2 1659.465 s versus shipped
  2139.493 s (0.77563×), no regressions/coverage loss, summary SHA
  `be59e040…6eb`, and zero residue. Candidate T105 is checked at 370.588 s
  versus shipped 777.863 s. Its separate fuzz is still running, and this pin's
  Rank-27 witness is superseded by the stronger cumulative pin rather than
  duplicated.
- 2026-08-24 — The exact compact-Kitten follow-on is frozen as Release SHA
  `85f74f24…996` / debug SHA `f3abd5cf…2b8` against source-diff SHA
  `d4b918aa…c53`. Native-LRAT fuzz `1644570` and its mandatory full-width
  B28+fixed-C5 guard `1644571`/`1644637` are running. This is now the only
  candidate authorized to advance to a new checked Rank-27 witness after both
  gates pass.
- 2026-08-24 — Occurrence-only sanitizer fuzz `1644431` completes 10,000/0
  with 8,762 checked verdicts (5,188 SAT + 3,574 proof-checked UNSAT), 6,160
  BVE-reaching cases and 1,458,583 target applications. The standard 1,238
  five-second nonterminating cases retain 25 diagnostic CNFs, but no proof,
  LRAT, or staged-input residue remains; log SHA is `53f45eab…fd5`. Together
  with its 66/66 guard this closes that positive increment, now superseded for
  Rank-27 pricing by the compact pin.
- 2026-08-24 — First-occurrence + active-watch guard `1644502`/`1644568`
  passes 66/66 (17 SAT + 16 UNSAT per arm), candidate PAR-2 1663.615 s versus
  shipped 2221.552 s (0.74885×), no regression/coverage loss, summary SHA
  `b11ea947…39a`, and zero residue. Its candidate T105 BVE is 99.625 s versus
  occurrence-only 99.644 s with every semantic counter fixed; the 404.989
  versus 370.588 s total is broad node noise across all rules, not a negative
  BVE effect. Compact-Kitten remains the only live Rank-27 candidate.
- 2026-08-24 — `SP-BVE-DEF-SHRINK-1` lifts Kitten's next measured
  representation detail without entering the frozen compact pin: second-core
  membership bytes persist, and the clause arena compacts in place instead of
  allocating a bitmap plus duplicate arena on every refinement. Tactix keeps
  the parallel byte image only because its proof-visible two-word header has
  no Kitten flag word. Focused Release/debug proof tests pass; mp1 `1644638`
  preserves all 16,395 exact definition payloads and both frozen trace SHAs,
  with zero residue. It remains an independently validated follow-on pending
  pricing after the already-gated compact Rank-27 witness.
- 2026-08-24 — Compact-Kitten guard `1644571`/`1644637` passes 66/66
  (17 SAT + 16 UNSAT per arm), candidate PAR-2 1660.549 s versus shipped
  2122.581 s (0.78233×), no regression/coverage loss, summary SHA
  `c874c55f…fde`, and zero residue. Candidate T105 remains byte-exact at
  351,851 conflicts and proof SHA `8aeeae6e…dd4`; direct identity consumption
  lowers BVE 99.625→94.831 s. Exact-source fuzz `1644570` is now the sole
  blocker for prepared checked Rank-27 campaign `20260824-0306`.
- 2026-08-24 — Corrected current-Kissat performance authority: optimized O3
  metrics observer `1644639` preserves the prior current-source trajectory
  (4,120,691 conflicts, 1,621,010 elimination attempts, 364,315 eliminated)
  but solves diagnostic Rank-27 in 286.30 s process / 287.72 s wall with
  Eliminate at 16.32 s. The earlier event-trace-capable observer's 637.43 s /
  26.16 s was build/instrumentation overhead and remains semantic evidence
  only, not a speed reference. Current optimized Kissat is therefore close to
  frozen checked Kissat 4.0.4 at 278.187 s. It performs 1,137,044 general
  definition checks versus lifecycle Tactix's 1,223,465 (only 7.6% fewer), so
  excess call volume cannot explain Tactix's 174.204 s BVE cost. Observer
  `1644640` now measures the missing exact aggregate definition ticks.
- 2026-08-24 — Exact compact-Kitten sanitizer fuzz `1644570` passes
  10,000/0 with 8,761 checked verdicts (5,187 SAT + 3,574 proof-checked
  UNSAT), 6,158 BVE-reaching cases, 1,457,675 target applications, and zero
  proof/LRAT residue; 25 standard timeout CNFs remain diagnostic only. Log SHA
  is `9323bfff…d65`. Together with guard `1644571`/`1644637`, this authorizes
  immutable proof-producing Rank-27 campaign `20260824-0306` at 2 CPUs,
  16 GiB, and the 1200-second solver contract.
- 2026-08-24 — Current-Kissat tick observer `1644640` preserves the exact
  optimized trajectory and reports 160,758,923 aggregate definition ticks over
  1,137,044 checks. Lifecycle Tactix uses 2,505,837,432 over 1,223,465:
  15.588× total and 14.487× per check. Calls differ by only 7.6%, so the
  residual Rank-27 BVE owner is much harder per-definition logical work after
  semantic divergence, not mainly raw SubSolver throughput. Log SHA is
  `1078e4e…650`, zero residue. Checked compact witness `1644641` is now running
  from the exact guarded pin; next localization must find the earliest portable
  per-definition tick explosion from the last common state.
- 2026-08-24 — First-occurrence + active-watch sanitizer fuzz `1644501`
  completes 10,000/0 with 8,760 checked verdicts (5,187 SAT + 3,573
  proof-checked UNSAT), 6,158 BVE-reaching cases, 1,457,675 target
  applications, and zero proof/LRAT residue; the standard timeout corpus keeps
  25 diagnostic CNFs. Log SHA is `8743664a…46b`. Its 66/66 guard and fuzz now
  fully close that exact positive pin, superseded by compact-Kitten.
- 2026-08-24 — Current/current conflict-15345 definition campaign `1644642`
  yields a complete reusable current-Kissat trace (2,654,950 rows, 789,879
  definition rows, terminal attempt 292,771; SHA `018811df…74d6`) but only
  three Tactix header rows: the obsolete compact-probe filter suppressed the
  newer Tactix definition event. This is an observer-filter failure, not a
  solver crash or soundness defect. Campaign `20260824-0315` / job `1644644`
  reruns only the missing exact-pin Tactix side with validated high-volume
  skip filters; the preserved Kissat side is deliberately not duplicated.
- 2026-08-24 — Tactix-only retry `1644644` completes cleanly in 55 s but
  records zero definition events through Tactix conflict 15,345. Thus the
  accepted repairs shifted Tactix's terminal elimination phase before that
  conflict label; current Kissat still runs its comparable phase at 15,345.
  Conflict-number alignment is no longer a common-state contract. Campaign
  `20260824-0316` captures the complete current-Tactix compact BVE stream and
  will align the terminal phase by entry decisions, candidate order, and
  definition environment before applying the required worse-side comparison.
- 2026-08-24 — Checked compact-Kitten Rank-27 witness `1644641` is a valid
  UNSAT but remains a target miss: 504.297 s wall / 503.787 s internal versus
  the 300 s engineering threshold. Its 1,633,208,191-byte proof (SHA
  `7a9c2c04…1885`) verifies in 106.339 s and is deleted with all scratch.
  The semantic trajectory is exact to the lifecycle witness at 3,747,797
  conflicts, 61,793,863 BVE resolutions, 1,223,465 definition checks, and
  2,505,837,432 definition ticks. The representation repairs therefore deliver
  a clean positive increment—BVE 174.204→94.389 s and internal solve
  579.268→503.787 s—without changing pruning, but leave Rank 27 open at
  1.681× target. Complete-BVE trace `1644645` continues localization against
  current Kissat's 160,758,923 aggregate definition ticks.
- 2026-08-24 — Complete current-Tactix BVE capture `1644645` finishes
  untruncated with 988,372 definition rows. The terminal streams start with
  identical candidates and byte-exact definition attempts 1–8, but entry is
  already different: Tactix is at conflict 15,344 / 5,956,353 decisions versus
  current Kissat at 15,345 / 5,932,210. Attempt 9 is the first payload mismatch
  (different external candidate), so its 7-versus-6 ticks are downstream and
  not a portable same-environment defect. Campaign `20260824-0317` moves back
  to the preserved common conflict-10,000 trajectory and reruns only current
  Tactix to find the earlier worse-side owner; Kissat is not rerun.
- 2026-08-24 — Exact in-place-core follow-on is frozen independently as
  Release SHA `960b31b4…90e7` / debug SHA `4b5c7c94…b0d8` against production
  source-diff SHA `5e015d57…04a3`. Native-LRAT fuzz `1644647` and the required
  full-width B28+fixed-C5 guard `1644648` / aggregate `1644714` are running.
  Exact mp1 parity is already closed by `1644638`; this pin cannot advance to
  Tier D unless fuzz and all 66 checked guard tasks pass with zero residue.
- 2026-08-24 — Current trajectory `1644646` confirms that the cumulative
  latest Tactix no longer shares the old conflict-10,000 learned stream, so the
  prior c14,456 terminal-search lead is stale for this pin. This does not make
  c10,000 itself the owner because both window traces begin mid-search.
  Macro-only campaign `20260824-0320` reuses the frozen current-Kissat macro
  trace and will locate the earliest changed lifecycle boundary from startup;
  only then will a bounded detailed current/current window be opened.
- 2026-08-24 — Macro recheck `1644715` restores the correct common boundary:
  current Tactix matches current Kissat schedule/decisions through the
  conflict-9,449 second-Probe entry (5,761,854 decisions), including exact
  conflict-7,150 Reduce timing. The Probe exit is weaker: Tactix retains 225
  root-fixed-but-not-retired variables and one fewer binary, despite exact
  irredundant/redundant counts, then reaches c10,000 with 598 fewer decisions
  on a different learned trajectory. Campaign `20260824-0321` now applies the
  established compact slot-boundary trace to name the first worse rule.
- 2026-08-24 — Current c9,449 slot trace `1644716` closes portable population
  parity through first Vivify, Sweep/Substitute, Transitive, Backbone, Factor,
  second Congruence, and second early Substitute. The first weaker yield is
  second Vivify: Tactix ends at 574,136 irredundant / 1,902 redundant /
  753,322 binary versus Kissat's exact large-clause counts but 753,323 binary.
  This was initially classified as weaker yield; full multiset campaign
  `20260824-0323` below corrects that net-count inference. Candidate windows
  `20260824-0322` reused frozen current-Kissat Vivify traces with no reference
  rerun. Across all 4,915 paired candidates in the predeclared 0--500 and
  500--1,000 windows, candidate identity/order, decision/probe stream, and
  within-dose cumulative logical-tick deltas are exact. Source-clause payloads
  retain the already known packed-replacement versus in-place representation
  difference, but no paired terminal in these windows directly names the
  missing binary. Thus scheduling and logical effort are closed for these
  scopes; the next owner is the exact pre/post second-Vivify binary-set delta
  and the terminal emission transaction for that pair.
- 2026-08-24 — Added a disabled-by-default, portable binary-multiset snapshot
  to both semantic observers and covered the Tactix path with the event-trace
  unit test plus the Kissat overlay static test. Campaign `20260824-0323`
  freezes both current diagnostic binaries and will enumerate all Vivify
  entry/exit pairs at c9,449. This is an observer-only current/current run,
  not a frozen benchmark-reference rerun; it is designed to replace the
  ambiguous net-count comparison with the exact missing terminal pair.
- 2026-08-24 — The mandatory full-width B28 plus fixed-C5 guard for
  `SP-BVE-DEF-SHRINK-1` is closed: all 66 candidate/comparator tasks are valid
  (17 SAT models and 16 UNSAT proofs in each arm), with zero invalid, missing,
  regression, cleanup, or certificate-residue entries. Candidate PAR-2 is
  1,640.738 s versus 2,223.039 s shipped (0.738061 ratio), so the compact-core
  repair is positive on the complete iteration guard. Native-LRAT fuzz remains
  the only validation prerequisite before its checked Tier-D submission.
- 2026-08-24 — Full c9,449 binary-multiset campaign `1644719` closes the
  apparent second-Vivify weakness without a solver repair. Both solvers add
  the exact same 19 binary copies. Tactix deletes 12 versus Kissat 11 solely
  because Tactix enters with two copies of `(-354369,-354368)` and Kissat with
  one; both exit with one. The rule therefore has equal portable pruning, and
  the 753,322-versus-753,323 net count was duplicate normalization, not a lost
  consequence. Per worse-first analysis, do not pursue this Vivify lead. The
  live owner returns to comparable-work throughput: the exact Rank-27 search
  trajectory takes 503.787 s current Tactix versus 286.30 s current Kissat,
  including 94.389 s versus 16.32 s in elimination.
- 2026-08-24 — Rebuilt current Release after the general binary-set observer
  addition. The suite is 73/74: all executable, proof, configuration, overlay,
  campaign-cleanup, and hygiene tests pass. Only `doc_size` fails at
  `PROGRESS.md` 621/350 lines and 37/5 recent entries plus implementation notes
  578/560; calculus has a reserve warning. Repository authority forbids silent
  documentation compaction, so this known policy failure remains explicit.
- 2026-08-24 — Generated and validated the required three-page Andrew handoff
  report at `report-to-andrew/2026-08-24-0915-rank27-compact-vivify-closure/`.
  It records the checked 504.297 s compact witness, 66/66 guard, exact Vivify
  multiset closure, current/current 503.787 versus 286.30 s gap, and the
  long-definition-image throughput lead for the next agent.
- 2026-08-24 — Exact-source native-LRAT sanitizer fuzz `1644647` passes:
  10,000 cases, zero failures, 5,188 SAT plus 3,576 UNSAT terminating checked
  verdicts (8,764/10,000 oracle coverage), 1,236 explicit five-second
  non-terminations, and 1,459,984 stress-target applications including BVE in
  6,162 cases. Twenty-five bounded timeout CNFs are retained as diagnostic
  inputs; there is zero proof/certificate residue. No soundness, sanitizer, or
  crash trigger fired, so the authorized Claude path was not invoked. Together
  with the 66/66 guard, this authorizes checked Rank 27 and the frozen current
  Tier-D run.
- 2026-08-24 — Committed and pushed the cumulative validated checkpoint as
  `fd30d9d` on synchronized `main`. Frozen current-head Tier-D campaign
  `20260824-0330` passed compute-node SAT/UNSAT validation and was submitted as
  array `1644721` plus aggregate `1644722`: 1,166 tasks, 1,800 s, 16 GiB,
  `-c 2`, throttle 85, independent read-only solver inode 70916851 and SHA
  `960b31b4…90e7`. It runs asynchronously. Checked Rank-27 campaign
  `20260824-0331` freezes the same source in a separate inode and is ready under
  the 1,200 s / 16 GiB / `-c 2` debug contract; submitted as job `1644832`.
- 2026-08-24 — Complete terminal-BVE aggregation sharpens the remaining
  comparable-work gap. In that invocation, current Kissat makes 292,771
  definition attempts for 18,112,153 terminal ticks; current Tactix makes
  369,081 for 280,614,670. Tactix's 32,932 attempts above 1,024 imported
  variables consume 249,343,427 ticks, while Kissat has none in that band.
  The earliest Tactix attempt above 256 variables is external 396947: four
  clauses span 666 variables and cost 674 embedded ticks, whereas Kissat
  declines the same candidate. Paired selected-occurrence campaign
  `20260824-0332` will determine whether this is the first upstream
  long-clause/occurrence representation owner; no admission cap is authorized.
  Submitted as paired c2 job `1644851` without a current-Kissat scoring rerun.
- 2026-08-24 — Selected-occurrence job `1644851` rules out an occurrence cap:
  both solvers carry the same 2+2 physical population, select external 151858
  immediately before 396947, and eliminate 151858 for two charged pairs.
  Kissat then declines 396947 at tick 685 before definition extraction or a
  plan, while Tactix imports four clauses spanning 666 variables, spends 674
  embedded ticks proving SAT, and reaches a rejected oversized pair at tick
  689. Targeted job `1644883` proves both pivots remain unassigned and both
  151858 pairs have identical clause hashes and are tautologies. The exact
  owner is Kissat's source `eliminateclslim=100` input guard: its 338/403-literal
  antecedents make `occurrences_literal` return `UINT_MAX`; Tactix had ported
  the same limit only for generated resolvents. The repair adds a general
  policy-owned antecedent-size bound, leaving the public rule unrestricted;
  focused Release and ASan/UBSan BVE/scheduling/event-overlay tests pass.
  Required full-width B28+fixed-C5 guard `1644893`/aggregate `1644900` and
  exact-source native-LRAT fuzz `1644936` are submitted from independent frozen
  pins. No repaired Rank-27 witness may run until the 66-task guard passes.
- 2026-08-24 — Checked compact-pin repetition `1644832` is valid UNSAT and its
  1,633,208,191-byte proof verifies with the exact prior SHA; all 3,747,797
  conflicts, 177,941,301 decisions, and non-timing counters match the 504.297 s
  witness. Wall is 721.076 s only because it shared node017 with the throttle-85
  Tier-D load: steady-clock search/scaffolding rose 249.410→417.332 s while BVE
  rose just 94.389→100.339 s. This directly closes the suspected pinned-run
  regression as resource contention, not a solver-source regression. Proof,
  staged CNF, and checker scratch are deleted with zero certificate residue.
- 2026-08-24 — The asynchronous frozen current-head Tier-D array has 84 valid
  terminal results among its first 90, but task 21 (`goldcrest-and-11`) emits a
  968,410,268-byte DRAT proof rejected at proof line 13,564,647; unlike two
  separate Cake-LPR heap-capacity failures, this is a correctness incident.
  Older accepted pins verify this instance, so it is not a checker limitation.
  Exact immutable-pin campaign `20260824-0337`, job `1644976`, reruns paired
  DRAT and native LRAT under 16 GiB/1200 s and deletes all proof scratch. Rank-27
  promotion is paused pending classification. A reproduced proof failure will
  enter the repository's bounded Claude repair workflow after a regression is
  established; performance conclusions do not override N1 correctness.
- 2026-08-24 — Goldcrest campaign `20260824-0337` byte-reproduces the rejected
  DRAT stream (968,410,268 bytes, SHA `0a1cef73…6472`) and independently
  verifies native LRAT (10,207,175,603 bytes, SHA `dfb8e9e0…e34a`) in 110.22 s.
  Both scratch proofs and decompressed inputs are deleted. This proves the
  UNSAT derivation is sound and localizes the blocker to DRAT's deletion-by-
  literal identity. Authorized Claude Code is performing the bounded repair;
  Codex will review and validate it independently. Campaign `20260824-0338`
  replayed the same byte-identical DRAT proof in deletion-ignoring plain mode,
  but keeping the complete active population made drat-trim exceed 1,200 s;
  that auxiliary result is inconclusive and all scratch was deleted.
- 2026-08-24 — The invalidated frozen current-head Tier-D array `1644721` and
  aggregate `1644722` were canceled at 10:03 EDT after 286 durable results;
  its manifest now records disposition `rejected-canceled-correctness-failure`
  and names goldcrest result 21 as the invalidator. The cancellation removed
  163,312,830,491 scratch bytes; scratch and retained-certificate residue are
  both zero. Preserve the partial campaign only as rejected diagnostic evidence
  and never use it for scoring or promotion.
- 2026-08-24 — Claude's bounded goldcrest investigation localized the DRAT
  defect to Sweep bridge representation, added a focused collision regression,
  and left a general repair before its account quota stopped the session at
  10:13. Pair-wide `equivalence_bridge_images` protection had retained every
  ordinary physical binary whose literals coincided with a preserved bridge
  target, even though Substitute's proof pass had consumed those ordinary
  copies. The repair counts exact directed bridge entries and lets coincident
  ordinary copies follow the normal rewrite/removal path. Release and
  ASan/UBSan Substitute+Sweep tests pass. Exact combined-source campaign
  `20260824-0339` is replaying goldcrest under 16 GiB/1200 s; no B/C or Rank-27
  promotion is authorized until its ordinary DRAT proof verifies.
- 2026-08-24 — The pre-proof-fix BVE antecedent-limit guard completed 66/66
  valid and residue-free: both arms have 17 SAT models and 16 UNSAT proofs.
  Candidate/comparator PAR-2 is 1743.764/2394.097 = 0.728. The repair
  improves mp1 from shipped 437.966 s to 277.765 s, with 9,050,802→5,429,547
  conflicts and learned-clause length 58.48→55.55. Candidate T105 verifies at
  480.267 s versus shipped 913.729 s. Exact-source sanitizer fuzz completed
  10,000 cases with zero failures: 5,184 SAT plus 3,569 UNSAT checked verdicts,
  1,247 explicit bounded non-terminations, 1,453,888 stress applications, and
  zero certificate residue. These results
  establish promise only: the complete B/C guard and fuzz must be repeated from
  the final post-Claude source before any Rank-27 witness is authorized.
- 2026-08-24 — Final-source validation is staged without consuming the Rank-27
  reservation. Exact goldcrest replay `20260824-0339` is running in allocation
  `1638010` from frozen Release SHA `5813aca3…5ca9`; its ordinary DRAT proof is
  the sole current gate. Full-width B28+fixed-C5 campaign `20260824-0340` and
  ASan/UBSan native-LRAT fuzz `20260824-0341` are prepared from the same source
  (the latter stresses BVE, Substitute, and Sweep), but remain deliberately
  unsubmitted until goldcrest verifies. On success they launch concurrently;
  Rank 27 follows only after the complete guard is valid and residue-free.
- 2026-08-24 — The counted directed-entry Sweep bridge repair passes its exact
  blocking witness. Goldcrest returns UNSAT and ordinary `drat-trim` verifies
  the 1,031,181,788-byte DRAT proof (SHA `c4f93869…ab513`) in 261.516 s; proof,
  decompressed input, and checker scratch are absent afterward. This closes the
  DRAT population defect without weakening deletion checking. Final-source
  full-width B28+fixed-C5 guard `1645170`/aggregate `1645236` and ASan/UBSan
  BVE+Substitute+Sweep native-LRAT fuzz `1645237` are now running concurrently.
  Rank 27 remains blocked until all 66 guard tasks are valid and residue-free.
- 2026-08-24 — Independent exact-source Release suite is 73/74: all solver,
  proof/model, bridge regression, configuration, static-boundary, trace, and
  campaign-cleanup tests pass. Only `doc_size` fails on the already recorded
  living-document debt (`PROGRESS.md` 753/350 lines and 51/5 recent entries;
  implementation notes 595/560); repository authority forbids silently
  discarding the resume/evidence surface to manufacture a green count.
- 2026-08-24 — Worktree audit is cleanly narrowed. Claude's temporary
  `goldcrest-proof-debug` worktree was removed after its lifecycle evidence was
  preserved in campaign `0339`. The dirty `vivify-unit-crash` branch is an
  ancestor of `main`; its watch-invariant/continue-after-unit repair and tests
  landed in `97e1447` and were subsequently strengthened, so that redundant
  worktree was removed after comparison. Only clean historical
  `.claude/worktrees/tierd-next10` survives.
- 2026-08-24 — Final-source mandatory guard `1645170`/`1645236` passes
  66/66: each arm validates 17 SAT models and 16 UNSAT proofs with no invalid,
  regression, missing, cleanup, or certificate-residue entry. Candidate PAR-2
  is 1,670.643 s versus shipped 2,165.228 s (0.771578 ratio); candidate T105
  verifies in 420.837 s and candidate mp1 in 260.633 s. Summary SHA-256 is
  `d65b8e67…4456c`. This authorizes the independently frozen checked Rank-27
  witness while final-source fuzz continues; any later fuzz correctness
  trigger invalidates use of that performance result.
- 2026-08-24 — Final checked Rank-27 job `1645239` is valid UNSAT but remains
  open: 549.172 s wall / 548.645 s internal, 1.831x the 300 s target. Its
  1,514,395,111-byte DRAT proof (SHA `0f99f55a…8bb2d`) verifies in 123.572 s;
  proof, decompressed CNF, and checker scratch are deleted. The preceding
  `1645238` attempt failed in one second before solver launch because the new
  campaign lacked its runner-owned `claims/` directory; adding that standard
  directory changed no pin and produced no certificate.
- 2026-08-24 — The BVE admission repair is nevertheless a decisive mechanism
  win on Rank 27: definition ticks fall 2,505,837,432→115,624,070, definition
  checks 1,223,465→873,301, and BVE 94.389→36.947 s. Total inprocessing falls
  254.377→213.590 s, but search/scaffolding rises 249.410→335.055 s on a changed
  trajectory (3,747,797→3,813,669 conflicts), making total wall 44.875 s worse.
  Against current Kissat's 286.30 s diagnostic run, the next worse-side owner
  is broad throughput: Tactix spends 335.055 s outside inprocessing versus
  Kissat's 188.64 s search despite fewer conflicts, decisions, and propagations;
  Vivify is 66.680 versus 26.19 s, Substitute 36.659 versus 10.42 s,
  Congruence 21.752 versus 5.91 s, and BVE still 36.947 versus 16.32 s. These
  cross-trajectory totals are localization leads, not same-state repair proof.
- 2026-08-24 — Final exact-source ASan/UBSan native-LRAT fuzz `1645237`
  passes 10,000 cases with zero failures: 5,188 SAT plus 3,575 UNSAT checked
  verdicts, 1,237 explicit bounded non-terminations, and 1,458,184 stress
  applications. Coverage reaches BVE in 6,161 cases, Substitute in 5,139, and
  Sweep in 2,753. Twenty-five timeout CNFs are retained as diagnostic inputs;
  there is zero proof/certificate residue. No soundness, sanitizer, or crash
  trigger fired, so the checked Rank-27 miss is final evidence for this pin.
- 2026-08-24 — Validated source/tests landed and pushed as `26a2d9c`. Frozen
  full Tier-D successor `20260824-0343` passed compute-node SAT/model and
  UNSAT/DRAT→Cake-LPR preflight, then launched asynchronously as 1,166-task
  array `1645240` plus aggregate `1645241`: 1,800 s, 16 GiB, `-c 2`, throttle
  85, independent read-only solver SHA `5813aca3…5ca9`, manifest SHA
  `82b56c26…8cff`, and zero retention. Do not wait for it before resuming
  convergence; inspect its aggregate and any correctness trigger first.
- 2026-08-24 — Wrap-up hygiene now requires an active-job/reference/evidence
  audit of `benchmarks/`, exact-path removal of proven-obsolete scaffolds and
  residue, and a post-delete scan. This pass preserves every frozen or active
  campaign and removes the 1,330,575-byte superseded never-submitted `0336`
  scaffold. The documentation process remains opt-in `compact-docs`: unique
  evidence moves to run manifests/reports or verbatim `docs/log/` archives,
  then PROGRESS is reduced to current authorities/gates/jobs/next action and
  at most five recent one-liners.
- 2026-08-24 — Early asynchronous Tier-D audit at 48 results finds zero
  retained/pending-cleanup artifacts and two invalid resource exits: task 41
  (expected SAT) aborts on `std::bad_alloc` at 13,390,560 KiB RSS and task 74
  (baseline UNKNOWN) at 14,732,416 KiB; both partial proofs are hashed/deleted.
  No wrong SAT/UNSAT claim or proof rejection is present in those rows. Keep
  the array running, but its final aggregate must classify the task-41 solve
  loss and any later invalid before the campaign can support coverage/scoring.
- 2026-08-24 — Resume audit at pushed checkpoint `e9c161c` found `main` exactly
  synchronized with `origin/main`, only the clean `tierd-next10` secondary
  worktree surviving, and no intervening repository or solver-campaign work.
  The expanded `0343` audit reached 294 classifications: 103 checked SAT, 84
  checked UNSAT, 98 solver timeouts, three `bad_alloc` resource exits, and six
  CakeML heap-exhaustion checker exits after `drat-trim` verification. On user
  direction, arrays/aggregates `1645240`/`1645241` and predecessor
  `1642950`/`1642951` were canceled to free capacity. Two terminal cleanup
  journals were promoted without solver reruns; all result-less interrupted
  proof scratch was deleted. Both frozen partial campaigns are non-scorable and
  have zero pending, retained, scratch, or certificate residue; their new
  `cancellation.json` records preserve exact counts and disposition.
- 2026-08-24 — Common-state production profile `1645436` assigned 3.2251% of
  conflict-9450 cycles to repeatedly sorting the post-compaction live-variable
  image back into permanent input order. The general Kernel repair stores the
  inverse first-occurrence rank once and resolves its retired holes through the
  existing external-to-internal compaction map. Profile `1645537` preserves
  every non-time counter and removes that owner: exact-state total is
  21.051→20.640 s and inprocessing 10.029→9.627 s. Event-trace `1645547` is
  byte-identical for all 64 rows (SHA `7a6216bb…7cc1`), preserving exact rule,
  candidate/order, tick/cost, scheduling, and pruning images through the second
  Vivify. Full B28+fixed-C5 guard `1645550`/`1645557` passes 66/66 checked with
  zero invalid, regression, cleanup, or residue entry and candidate/shipped
  PAR-2 ratio 0.709919. Exact-source ASan/UBSan native-LRAT fuzz `1645587`
  passes 10,000 cases with zero failures: 5,044 SAT models, 3,381 UNSAT proofs,
  1,575 declared five-second nonterminations, 1,329,770 stress applications,
  all BVE/Substitute/Sweep targets reached, and zero certificate residue.
  Authorized immutable `-c 2` Rank-27 witness `1645699` is checked UNSAT in
  413.830 s internal / 414.375 s wall versus 548.645/549.172 s before. Every
  non-time statistic and the 1,514,395,111-byte proof SHA `0f99f55a…8bb2d`
  are exact; `drat-trim` verifies in 112.340 s and cleanup is zero. The linear
  inverse recovers 134.815 internal seconds on the identical trajectory:
  inprocessing 213.590→167.753 s and search/scaffolding 335.055→246.078 s.
  Rank 27 remains open at 1.379x the 300-second target; next localize the leading
  remaining full-run source owner without reopening propagation counter batching.
  Exact Release is 73/74; only known opt-in documentation debt fails
  (`PROGRESS.md` 862/350 and 62/5 recent entries, implementation notes 595/560;
  calculus reserve warning). Final three-page Andrew report is
  `report-to-andrew/2026-08-24-1215-rank27-import-order-linear/`, TeX SHA
  `07a23253…fe6f`, PDF SHA `a60adf3a…8594`, with no LaTeX box warning.
- 2026-08-24 — Full paired cycles profile `1645700` exactly reproduces current
  Tactix's checked trajectory and current-source Kissat's accepted diagnostic
  counters with zero lost samples/residue. Watched propagation leads both;
  sampled cycles per printed propagation are approximately 215.5 Tactix versus
  174.2 Kissat. Source/disassembly narrows the first portable representation
  owner to Tactix's generic out-of-line vector push on the already capacity-
  reserved assignment trail. A Kernel-owned pre-sized backing store plus
  logical end removes that call without changing the public trail operations.
  Simultaneous conflict-9450 profile `1645702` preserves every non-time counter
  and lowers 17.372→16.624 s, sampled cycles 66.659B→63.787B, and the shared
  vector-push owner 2.78%→0.63%. Event trace `1645704` is byte-identical for all
  64 rows (SHA `7a6216bb…7cc1`), freezing exact rule/candidate/order, tick/cost,
  scheduling, and pruning state. Release is 73/74; only known opt-in `doc_size`
  debt fails (`PROGRESS.md` 903/350 lines and 63/5 recent entries;
  implementation notes 621/560; calculus reserve warning). Focused ASan/UBSan
  Kernel coverage is green. Full B28+fixed-C5 guard
  `1645705`/`1645771` passes all 66 checked tasks at 0.757396x shipped PAR-2
  with zero residue. Exact-source 10k native-LRAT sanitizer fuzz `1645772`
  passes with 5,188 SAT models, 3,577 UNSAT proofs, 1,235 bounded
  nonterminations, every target rule reached, zero failures, and zero residue.
  Checked Rank-27 `1645778` preserves every non-time statistic and the exact
  1,514,395,111-byte proof SHA `0f99f55a…8bb2d`; pinned `drat-trim` verifies it
  in 112.088 s and cleanup is zero. Internal/wall time improves
  413.830/414.375→407.006/407.497 s, so the general representation repair is
  promoted but Rank 27 remains open at 1.357x the 300-second target. Final
  four-page Andrew report is
  `report-to-andrew/2026-08-24-1515-rank27-reserved-trail/`, TeX SHA
  `dfc5c106…babb`, PDF SHA `671586fc…9c8e`, with no LaTeX box warning. Source,
  regression, design note, and report landed as `2c86274d…793c`.
- 2026-08-24 — Exact-pin full profile `1645779` confirms the reserved-trail
  trajectory at 403.887 internal seconds and leaves ordinary propagation at
  approximately 195.780 sampled cycles per printed propagation versus current
  instrumented Kissat's accepted 174.239 (1.12362x); frozen Kissat was not
  rerun. Six simultaneous conflict-9450 source experiments preserve every
  non-time counter, logical tick/cost, candidate/order, scheduling, learning,
  and pruning aggregate. Cold proof-ID clause layout `1645781`, active-only
  assignment `1645783`, binary-source snapshot `1645785`, saved-phase snapshot
  `1645787`, no-ignore specialization `1645789`, and atomic large-watch append
  `1645791` are all rejected and reverted: none improves both complete elapsed
  time and complete cycles materially. The no-ignore arm alone lowers its
  propagation owner 1.22% but regresses the whole prefix; do not reopen any of
  these six without contradictory whole-prefix evidence. The first atomic
  append prototype exposed a `solve_reset` heap corruption on an exact
  one-word-capacity slice. Mandatory bounded Claude repair replaced one grow
  with a checked growth loop and added a reproducer; Claude's 73 code tests and
  independent Release plus ASan/UBSan Kernel/reset checks pass. The repaired
  candidate is neutral and fully reverted, no crashing binary was submitted,
  and every campaign has zero staged-CNF/certificate residue. Accepted code
  remains synchronized at `21aa151b…3465`; next compare the per-list
  watch-head/arena indirection and relocation population against Kissat before
  changing behavior. Required three-page Andrew handoff is
  `report-to-andrew/2026-08-24-1615-rank27-propagation-closures/`, TeX SHA
  `5920d4bd…7da`, PDF SHA `0d8b4c46…975`, with no final-pass LaTeX warning.
- 2026-08-24 — Promoted Congruence's general first-literal membership range
  directory after exact stage/profile/trace parity (`1645800`--`1645805`), full
  checked B28+C5 guard `1645806`/`1645872`, and corrected exact-source fuzz
  `1645874` (10k/0). Checked Rank-27 `1645875` verifies the byte-identical
  1.514 GB proof with zero residue and improves internal time
  407.006→403.855 seconds, still above 300. Frozen full Tier-D remains canceled
  and non-scorable. The Andrew handoff is
  `report-to-andrew/2026-08-24-1815-rank27-congruence-first-literal-range/`;
  TeX SHA `a495b4a1…fc5d`, PDF SHA `b749b7c2…9043`, three pages, and no final-pass
  LaTeX box warning. Final Release is 73/74; only the pre-existing opt-in
  documentation-size debt fails (PROGRESS and implementation notes remain over
  their long-lived budgets; calculus has a reserve warning).
- 2026-08-23 — Repaired and exactly closed the Sweep→Substitute bridge,
  retirement, c3069 environment, and c5471 implication streams.
- 2026-08-23 — Closed decision parity through c10000 and opened the c15385 BVE
  throughput lead with a separately pinned no-trace cost control.
- 2026-08-23 — Repaired linear SubSolver import and DRAT transient-lemma
  identity; opened the one-sided-definition-unit divergence at c15345.

<!-- track-sched:begin -->
N4 verification remains complete at the accepted parity pin. Living mechanism
dispositions and reopening conditions are in `docs/scheduling-parity-ledger.md`.
The Sweep semantic/throughput, counted directed bridge, BVE antecedent-admission,
reserved-trail, Congruence first-literal, batched ITE sign-mask, and dense ITE
conditional-list representation closures remain accepted. Rank 27 remains
above the milestone after valid checked job `1646031` at 399.152 seconds;
resume current Tactix versus current
instrumented Kissat at the earliest common-state weaker-work or
slower-comparable-work divergence without rerunning frozen references. Reaching
300 seconds does not end convergence while justified portable gaps remain.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
