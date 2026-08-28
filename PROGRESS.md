# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and evaluation policy;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4.
Frozen manifests and reports own experimental detail. Git preserves the former
session narrative; this file is only the current resume surface.

## Resume anchor

- **In flight — provisional 40-rank baseline and dense-binary cost repair:**
  homogeneous campaign `20260828-1344-tierd-ranks1-40-live-active-fix`
  has all 80 solver timings. Provisional geometric-mean Tactix/Kissat ratios
  are 1.408× SAT and 1.342× UNSAT; proof checking remains independent and
  non-blocking. Rank 16 is 1.029×, Rank 32 UNSAT is the largest UNSAT loss at
  2.382×, and SAT Rank 6 is 5.377×. A source-mapped Rank-6 profile found
  repeated linear binary-membership scans in BVE and Strengthen. The general
  dense-stream repair preserves exactly 551,149 conflicts and 5,224,515
  decisions while reducing wall 293.372→98.730 s; the model satisfies all
  11,099,040 clauses. Factor remains near its Kissat component cost and is not
  the owner. Release is 76/76 and focused Release, Debug, and ASAN/UBSAN pass.
  N1--N4, classification, production budgets, creation order, and the
  proof-rule/strategy boundary are unchanged; no formula/rank gate exists.

- **In flight — Kissat creation/collection-order production baseline:** the
  pushed tree makes Kissat order unconditional production state and retains the
  CaDiCaL VMTF/phase permutation only as `TACTIX_ARENA_ORDER=1`. No formula,
  density, benchmark, or rank selector can activate it. Jobs `1653454`,
  `1653462`, and `1653468` retain the earlier baselines. The source revisit
  replaced an immutable proof-ID key—which could undo a prior sparse Reduce
  partition—with generation-local current source positions. Collection now
  materializes and clears those positions; Reduce rank ties, root sweep,
  prefix watch retention, zero-watermark compact collection, and the stable
  `{I,R,I,R}`→`{I,I,R,R}` partition share that contract. Jobs `1653494`--
  `1653499` pin the preceding repair and remain immutable. Final-tree Rank-16
  trace `1653529`, homogeneous ranks 1--40 `1653530`/`1653531`, and B28+C5
  `1653532`/`1653534` are submitted. Release is 76/76 and affected ASan/UBSan
  is 6/6. Rule admission/effort, N1--N4, and proof/strategy boundaries are unchanged.
  Proof completion is non-blocking per user direction. Never revert the
  production order or add an ordering gate in response to a regression.
- **Promoted exact Kissat classification and general Factor throughput:** the
  formula-profile matrix and Dose5/Factor5/NoBve/NoBveLight/
  StartupFactorOnly profiles are removed. Pinned Kissat `small`/`bigbig`
  predicates and boundaries are exact; `small` is write-only and `bigbig`
  gates only ordinary fast binary jump-reason assignment, while the learned-
  binary path remains ungated. BVE/Factor/forward-subsume/Vivify production
  effort is formula-invariant at 100/50/100/100 permille. Typed diagnostic arms
  remain explicit and cannot be selected by formula shape, benchmark, or rank.
  Compact Factor watches/remainders, epoch marks, geometric kernel growth, and
  incremental heuristic import state reduce Rank-9 Factor time from 52.335 to
  6.746 seconds without changing its 51,357 factorings or adding a gate.
  Release is 76/76 PASS. B28+C5 is performance-complete 66/66 under the user's
  non-blocking proof-pass assumption at candidate/predecessor PAR-2
  1,433.058/1,329.014 seconds (1.078287x). Homogeneous ranks 1--40 are complete
  for performance: 36 candidate solves, timeouts 15/26/30/36, PAR-2 22,116.682
  seconds, 9.551624% better than the exact-classification pre-repair candidate
  and 2.813095% better than accepted Tactix; current Kissat remains ahead at
  7,682.181 seconds. Claude's `20260827-2550` verdict is PASS: 81/81 corpus,
  30,000/30,000 generated, 15,979 checked SAT models, 10,974 accepted native
  LRAT proofs, direct repaired-path reach, N1--N4 preserved, and zero failure
  or residue. Residual Rank-20 search quality is tracked as a general selected-
  Factor-grid/post-Factor-state discrepancy; never restore a formula gate.
- **Accepted Rank-4 closure:** decision-grade frozen confirmation
  `benchmarks/runs/20260827-0400-tierd-rank4-paired-confirmation/` is stable and
  scorable on node018: Tactix `[15.340,15.440,15.340]` seconds versus current
  Kissat `[7.728,7.728,7.728]`, median ratio `1.984990`, with all six UNSAT
  proofs independently verified and zero residue. Exact tracing localized the
  first portable divergence after 284 identical accepted Factor grids. Both
  heaps pop external literal 521 with score 5 in the same position, but
  accepted Tactix has `armed=0` while current Kissat's per-polarity factor bit
  admits it and applies grid `[521,-31,-32]`. Frozen diagnostics
  `20260827-0630` through `20260827-0660` own that localization.
- **Rank-4 repaired evidence:** Tactix now mirrors Kissat clause-add
  lifecycle by arming exactly the new positive divider and negative quotient
  polarities after an accepted composite grid, without scheduling either
  directly. Frozen candidate/tick campaigns `20260827-0670` and
  `20260827-0675` prove exact parity for all 1,251 Factor pops, 1,309 viable
  seeds, 381 accepted grids, 1,971 ordered grid members, and 340,150 Factor
  ticks. The first nine decisions, first conflict order/set/levels, every
  learned-clause stage, retained counts, and top-32 order are also exact after
  normalizing internal clause IDs and distinct inprocessing-propagation
  accounting. Local release tests are 76/76 PASS. Three-wave release campaign
  `20260827-0680` is decision-eligible: candidate/baseline/Kissat medians are
  10.733/15.340/7.728 seconds, candidate/baseline is 0.699674, and the current-
  Kissat gap falls from 1.984990x to 1.388846x. Candidate conflicts fall from
  983,053 to 647,074 versus Kissat's 651,764. All nine UNSAT proofs verify and
  all arm spreads are below 0.66%. Complete B28+C5 gate `20260827-0690`
  (`1652036`/`1652102`) is 66/66 PASS with 34 checked SAT models, 32 verified
  UNSAT proofs, no solve loss or residue, and candidate/predecessor PAR-2
  1,382.422/1,373.376 seconds (1.006587x). Claude's authoritative verdict for
  frozen campaign `20260827-0700-rank4-factor-fresh-admission-fuzz` is PASS:
  81/81 corpus replays, 30,000/30,000 generated cases, 15,967 independently
  checked SAT models, 10,968 accepted native LRAT proofs, direct reach of both
  fresh-polarity admissions and an accepted fresh-seeded grid, zero failure,
  and zero certificate/scratch residue. The repair is promoted at solver
  commit `3c3164d`; no fuzz repair was required.
- **Historical Rank-32 Congruence-admission evidence:** accepted Tactix
  `3c3164d`/`f2eecac1...dcf` remains the baseline. Its three-wave Rank-32
  median is 412.357 seconds versus current Kissat's 290.015 (1.420119x), with
  all proofs valid and spreads below 0.57%. Removing Tactix's extra
  Congruence new-material gate restores exact current-Kissat order/cost/state
  for 542,604 candidates and improves the candidate median to 405.042 seconds,
  but complete B28+C5 campaign `20260827-0760` fails the frozen performance
  guard at 1.208552 PAR-2 despite 66/66 correctness and zero residue.
  `mod4block` regresses from 19.846 to 237.094 seconds. Frozen diagnostics
  `20260827-0770`--`0775` localize the first candidate-owned effect: at exact
  common conflict 105525 an extra 152-candidate/456-tick Congruence round has
  zero yield and no logical-state change, but its reference-authored binary
  dense resume changes physical propagation order before Backbone; conflict
  clauses first differ at 105527 and durable learning at 105568. Suppressing
  that replay would restore the non-reference baseline pose, not repair an
  implementation defect. The candidate was therefore reverted under the old
  cumulative disposition. The 2026-08-28 no-invented-gates architectural
  requirement supersedes that disposition: the Kissat-authored call is now
  production behavior, and the measured regression is evidence of a general
  dense-resume/propagation-order mechanism discrepancy to repair rather than a
  reason to suppress Congruence. Full historical authority and limitations are in Andrew report
  `2026-08-27-0905-rank32-congruence-admission-negative`.
- **Rank-10 memory diagnostic:** frozen campaign
  `benchmarks/runs/20260827-0130-tierd-rank10-current-memory-boundaries/`
  (`1651511_0`) completed valid SAT/model-verified on node018 at 19,886,212 KiB
  peak RSS with zero certificate residue. Matched historical/current boundaries
  prove a +3,766,948-KiB representation regression at essentially identical
  logical work and state. Persistent per-variable/order images account for the
  parse delta; the dense clause-ID alias image accounts for about 856 MiB by
  Congruence root end; the exact scoped binary-liveness hash adds about 898
  MiB; and value/closure indices own the remainder. Authority:
  `benchmarks/reports/2026-08-27-rank10-memory-regression-localization.md`.
- **Accepted Rank-10 closure:** the first three Rank-10 layout arms are valid and
  zero-residue: A (`1651512`) peaks at 18,302,324 KiB, B (`1651513`) at
  18,301,456 KiB, and C (`1651514`) at 18,067,072 KiB while preserving exact
  recorded state and reducing Congruence internal time. Candidate C's
  Congruence peak is below 16 GiB, but a later untraced owner is not; compiled-
  only slot-boundary diagnostic `1651515` proves Sweep alone raises high-water
  RSS by 1,364,212 KiB. Sparse-owner arm `1651516` is valid SAT/model-verified
  and zero-residue at 16,704,284 KiB, preserving every recorded rule counter
  while removing the formula-wide empty-vector header image. Strict Rank-10
  candidate/current-Kissat confirmation array `1651517` is terminal valid and
  zero-residue: Tactix is 95.633 seconds / 16,703,740 KiB and Kissat is 83.753
  seconds / 15,348,204 KiB. A reviewed order-bit reclassification arm
  `1651519` is valid but rejected under N4; the final rebuild exactly matches
  the `1651517` binary. Complete strict B28+C5 array/aggregate
  `1651521`/`1651587` is terminal PASS: 66/66 valid, 34 checked models and 32
  verified proofs, zero loss/invalidity/residue, and candidate/predecessor
  PAR-2 1,372.952/1,350.169 seconds (1.016874x). Claude's authoritative
  terminal verdict for `20260827-0230-rank10-memory-layout-fuzz` is PASS: 81/81
  corpus and 30,000/30,000 generated cases, 12,910 independently checked SAT
  models, 10,812 accepted native LRAT proofs, direct reach for every changed
  path, zero correctness/sanitizer failure, and zero residue. Rank 32 remains
  next after Rank 4. A genuine new Tactix memory solve loss takes precedence.
  Each solver repair stays isolated and receives its required gate,
  Claude-owned fuzz verdict, report, commit, and push before the next repair is
  promoted.
- **Canceled ranks 1--40 refresh:** campaign
  `benchmarks/runs/20260827-0800-tierd-ranks1-40-accepted-refresh/`, array
  `1652227`, and aggregate `1652264` were canceled immediately at the user's
  wrap-up request. Ten tasks completed with ten independently verified UNSAT
  proofs; 30 started tasks were canceled and 40 never started. No task remains
  pending/running, and certificate/scratch residue is zero. The partial is
  frozen non-scorable and must not select a witness; `cancellation.json` owns
  the exact classification. It also records the prepared correction for the
  predecessor's too-short 4,800-second checker limit: keep the 1,200-second
  solver limit but allow 16,000 seconds for auxiliary DRAT validation.
- **Terminal fresh replacement screen:** campaign
  `benchmarks/runs/20260827-0920-tierd-ranks1-40-accepted-fresh/`, array
  `1652270`, and after-any aggregate `1652302` use the exact accepted pins,
  80 new claims, 12 CPUs/48,000 MB per independent solver task, no `RLIMIT_AS`,
  no node constraint/throttle, and the widened 16,000-second proof-checker
  window. All 80 results are terminal and classified. Tactix has 35 valid
  solves (17 SAT, 18 UNSAT) and current Kissat has 40 (22 SAT, 18 UNSAT);
  every SAT model and UNSAT proof is independently verified. Tactix ranks 15,
  16, 19, 23, and 36 are solver timeouts. There is no OOM, correctness,
  validation, infrastructure, cleanup, or certificate-residue failure, so the
  campaign is scorable for witness selection. Rank 40 leads validated UNSAT
  pairs by absolute gap at 981.277/436.640 seconds (544.637 seconds,
  2.247336x); Rank 38 leads by ratio at 840.217/367.611 seconds (2.285614x).
  `rank-table.md`/`.json` carry every status, runtime, measured peak RSS,
  validation, loss class, and ratio. Summary SHA-256 is
  `fd44f96b7983c5fd21f438ce6ef738eff30f8f496e71b9f7cdec1e78423f9ba9`.
  The canceled `20260827-0800` partial remains excluded from selection.
- **Promoted Rank-5 memory repair:** accepted Tactix peaked at 14,368,044 KiB
  because the 58,633,530-variable DIMACS header eagerly committed cold
  rule/search state before early Lucky returned SAT. The isolated portable
  lifecycle candidate defers 40 bytes/variable of rule state and roughly 35
  bytes/variable of DefaultPolicy search guidance until early Lucky fails.
  Frozen diagnostic `20260827-1040-tierd-rank5-deferred-guidance` is valid
  SAT/model-verified and zero-residue at 10,075,752 KiB, below current Kissat's
  preceding three-wave median 10,402,844 KiB; its result SHA-256 is
  `d1c85448380bdcb29e2f22bec418beb5dac920ffa51a3c07391744f7c04da696`.
  Decision-grade `20260827-1100` has Tactix/current-Kissat median peak RSS
  10,075,752/10,402,844 KiB (0.968557x) and median runtime 21.550/29.913
  seconds (0.717080x), with stable three-wave timing, six checked models, and
  zero residue. Release/event suites are 76/76 and 75/75 PASS. Rank-4 guard
  `20260827-1045` is stronger than predeclared: all 33,138 trace lines through
  100 conflicts are byte-identical to accepted Tactix. Complete B28+C5 gate
  `20260827-1110` (`1652381`/`1652402`) is 66/66 PASS with 34 checked models,
  32 verified proofs, zero loss/invalidity/residue, and candidate/predecessor
  PAR-2 1,326.154/1,329.271 seconds (0.997655x). Claude's authoritative verdict
  for `20260827-1120-rank5-deferred-state-guidance-fuzz` is TERMINAL PASS:
  81/81 corpus, 30,000/30,000 generated cases, 16,041 independently checked
  SAT models, 11,035 accepted native LRAT proofs, direct positive/negative
  lifecycle reach, zero correctness/sanitizer/invariant failure, and zero
  residue. No repair workflow was triggered. The repair is promoted by the
  coherent solver/evidence commit that owns this PROGRESS update.
- **Active Rank-4 follow-up:** immutable accepted Tactix/current-Kissat bounded
  diagnostics `20260827-1030` and `20260827-1050` show all 20,355 comparable
  portable decision/analysis records exact through Kissat conflict 1,000.
  The residual accepted 10.733/7.728-second gap is therefore downstream or
  throughput-owned, not an immediate post-Factor-repair strategy divergence.
  Extend the frontier only with bounded source-backed windows; preserve N1--N4.
- **Active Rank-32 memory localization:** accepted-source `20260827-1130`
  proves retained inprocessing allocation owns the 566,348-KiB peak; rejected
  BVE-scratch `1140` saves only 7,808 KiB and is reverted/unpromoted. Capacity
  and allocator runs `1200`/`1210` are valid, proof-verified, zero-residue, and
  trajectory-identical. Late allocation is 333,394 KiB used heap, 127,957 KiB
  free heap, and 92,412 KiB mmap; retention matters, but used plus mmap remains
  425--450 MiB versus Kissat's 299,988-KiB peak. Persistent-owner `1225` names
  267.56 MiB late: reconstruction 86.37 MiB, clause aliases 45.37 MiB, and proof
  indices 35.29 MiB lead. Dense-owner replacement `1245`/job `1652473` is valid
  UNSAT/proof-verified/zero-residue and exact outside telemetry; the separate
  dense occurrence index adds 84.44 MiB late, leaving a stable 63.8 MiB gap.
  Its failed `1240` scaffold is frozen pre-solver/non-scorable with no reuse.
  Policy-owner `1300`/job `1652474` is valid/proof-verified/zero-residue and
  names only 13.31 MiB. Dense-allocation `1315`/job `1652475` is valid/proof-
  verified/zero-residue and exact outside telemetry: 834,922 allocations use
  92.86 MiB, 8.42 MiB above requested dense capacity. Reconstruction capacity
  rounding can save at most 22.37 MiB (~4% peak); defer both it and a riskier
  flat dense rewrite behind the five SAT timeout losses at ranks 15/16/19/23/36.
  Every diagnostic uses `-c 12`/48,000 MB, no address-space cap, constraint,
  exclusivity, or throttle, plus the 16,000-second checker; none reopens the
  rejected Congruence lead or supplies timing evidence.
- `.claude/worktrees/tierd-next10` is the sole historical secondary worktree
  and remains clean. The interactive shell allocation is not a solver campaign.

## Immutable authorities

- The frozen rank-selection authority is
  `benchmarks/runs/20260818-0142-tierd-ranks21-40-predeclared/manifest.json`;
  audit the corresponding ranks 1--20 authority before submitting the new
  1--40 comparison. Never infer membership from filenames or rerun a frozen
  scoring reference merely to fill a diagnostic field.
- Rank 27 remains immutable input
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`,
  SHA-256 `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
  Accepted conflict-9450 semantic SHA is
  `3b87bb36da711bcef3ca877f3922e9fa5f8a3c6ae9d638c1ae18d4965bd92c5f`;
  accepted 64-row trace SHA is
  `7a6216bb86c236512dea03de76c4c9ab2ffdd745bd7ca7b9fa5507e708987cc1`.
- Current instrumented Kissat comparator authority is commit `8af8e56`, binary
  `b6a9b1d0...644d5`; it is diagnostic, not scoring. The once-only frozen
  Kissat 4.0.4 campaign
  `20260825-0010-tierd-kissat-4.0.4-16g-1800s` classifies all 1,166 tasks in
  `results.json` (758 valid; 408 timeout/resource/checker invalid), has zero
  residue, and must not be rerun or silently scored through invalid rows.
- User-canceled full Tier-D campaigns `1645240`/`1645241` and
  `1642950`/`1642951` are frozen non-scorable partial campaigns. Their
  `cancellation.json` files own exact classifications; pending, retained,
  scratch, and certificate residue are zero.
- The latest complete B28+C5 authority for the accepted Sweep increment is
  `benchmarks/runs/20260827-0010-BC-sweep-phase-word-guard/`. It is 66/66 valid,
  zero-residue, with candidate/predecessor PAR-2 1,362.562/1,361.275 seconds
  (1.000945x). Summary SHA-256 is
  `ac59145977a044af299d703c24ea5dc84bd25c25ded4ba85294891c5d6e4cae1`.
- Claude Code exclusively prepares, submits, monitors, inspects, and reports
  solver fuzz jobs under `DEVELOPMENT_PLAN.md` §8.2. Codex accepts Claude's
  terminal verdict and does not probe fuzz artifacts. Generated legs use 50
  physical cores when capacity permits: 100 logical CPUs, 400,000 MiB, and 50
  workers with an immutable read-only harness snapshot.

## Evaluation contract

- Every solver invocation is its own SLURM job or array task. Never serialize
  solver arms in one allocation. Full-width tier arrays have no throttle and no
  node constraint; targeted diagnostics prefer node018 when available.
- Decision-grade long timing uses three independent paired waves. Each job
  requests `-c 12` and exactly `--mem=48000M`, installs no process address-space
  limit, binds one solver thread, and stages on verified node-local scratch.
  SLURM task cgroups are the sole memory enforcer. Decide from the median paired
  ratio and full range; reject absolute claims if either arm spans more than 2%.
  Fuzz and correctness gates are not triplicated.
- Before promoting a repair, run the complete frozen Tier B plus predeclared
  mechanism-relevant Tier C under the strict 16 GiB allocation/1,200-second
  contract (`-c 4`, `--mem=16G`, no `RLIMIT_AS`), independently validate every
  SAT model and UNSAT proof, require no unexplained solve loss, and leave zero
  certificate residue.
- Prioritize the earliest common-state point where Tactix performs equal
  normalized work materially slower or produces weaker learning, pruning,
  simplification, or retained state at comparable cost. Preserve exact rule and
  candidate order, tick/cost, schedule, and resulting state while common.
- Never introduce benchmark names, ranks, hashes, deadlines, per-instance arms,
  or tuned constants into solver behavior. A soundness defect, crash, sanitizer
  finding, rejected model, or rejected proof immediately triggers the mandatory
  bounded Claude repair workflow.

## Closed leads and reopening rules

- Sweep semantic/throughput parity, the counted directed bridge repair, BVE
  antecedent admission and flat-slice storage, decision-journal laziness,
  active-only decision selection, deferred Congruence collection,
  Substitute affected-owner traversal, import-rank inverse, reserved trail,
  Congruence first-literal/sign-mask/dense-slot indices, and the current Sweep
  phase-word repair are accepted general mechanisms.
- Collection throughput remains a secondary lead: exact prefix work attributed
  about 4.91% to whole collection, and the rejected forwarding traversal only
  0.33%. Fused-forward and arena-prepass optimizations remain closed negative.
  The current generation-local position replay is an architectural state repair:
  it emulates Kissat's in-place replacement position before a later sparse
  physical watermark can consume it. Do not generalize that repair into an
  unrelated collection optimization without a new exact call-reason owner.
- Residual BVE has no current comparable-state worse-side owner; Congruence's
  three-inline and two-inline table layouts and Substitute call-level deferral
  are rejected. Equal-score Rank-27 heap choices are neutral. Do not reopen any
  of these without contradictory exact source-backed evidence.

<!-- track-sched:begin -->
N4 verification remains complete at the accepted parity pin. Living mechanism
dispositions and reopening conditions are in `docs/scheduling-parity-ledger.md`.
Exact Kissat classification adds no scheduling consumer: `small` is write-only
and `bigbig` affects only ordinary binary jump-reason assignment. The removed
formula profiles cannot alter BVE, Factor, forward-subsume, or Vivify admission
or effort. The accepted Factor representation/growth repair preserves selected
work and policy while the open Rank-20 quality row owns the next general
mechanism comparison.
The accepted Sweep, Rank-10 layout, and Rank-5 lifecycle repairs change no
schedule, rule, candidate, tick, or policy state. The accepted Rank-4 repair
changes only the portable per-polarity admission lifecycle proven above and
leaves the rule instance surface unchanged. Congruence now retains the exact
Kissat call surface; its known dense-resume regression is a mechanism lead, not
authority for a non-reference admission gate.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
