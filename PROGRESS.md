# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and the debug contract;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4
evidence. This file is the current resume surface. Frozen manifests and reports
own experimental detail and history.

## Resume anchor

- Worktree `/home/hwu/SAT-solving`, branch `main`. Accepted solver baseline
  `16d46d5` promotes the cumulative decision-journal,
  deferred-Congruence-collection, and Substitute-affected-owner source over
  Vivify checkpoint `d8da6a1`. Production Release SHA is
  `acf9e716…2e013`; complete B28+C5 and Claude-owned fuzz gates are terminal
  PASS with zero solve loss, correctness failure, or certificate residue.
  Fuzz submission now pins a read-only, content-hashed copy of every executed
  harness/corpus input, eliminating the post-oracle live-script-offset failure
  exposed by the passing campaign. Resume current-Tactix/current-Kissat broad
  search-quality localization from `16d46d5`. Do
  not reopen rejected collection, Substitute, residual BVE, Congruence-layout,
  or equal-score heap-tie guesses absent new exact worse-side evidence.
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
- On later user direction, frozen Kissat 4.0.4 Tier-D reference campaign
  `20260825-0010-tierd-kissat-4.0.4-16g-1800s` ran as array/aggregate
  `1646252`/`1646295`. Its 1,166 tasks use accepted Kissat SHA
  `3aa8e41f…d49c`, two CPUs, 16 GiB, an exact 1,800-second solver timeout, no
  node constraint, and throttle `0-1165%40`. Fixed local SAT model and UNSAT
  DRAT→LRAT→Cake validation passed with zero residue before submission.
  Complete `results.json` classifies all 1,166 tasks before aggregate `1646295`
  fails only in presentation because the campaign omitted
  `year-membership.tsv`. There are 758 valid rows (405 SAT + 353 UNSAT) and 408
  invalid rows: 367 solver timeouts, three solver OOM exits 134, and 38
  certificate-pipeline failures (23 CakeML heap exhaustion, nine drat-trim
  memory exhaustion, three drat-trim timeout outputs, two trivial-UNSAT
  drat-trim exit-1 rows despite `s VERIFIED`, and one CakeML DIMACS parse
  failure). Resource and checker invalidities are excluded, not solver
  correctness failures. Missing tasks, interrupted claims, pending cleanup,
  retained artifacts, orphan artifacts, and certificate residue are all zero.
  Results SHA is `9489bfcf...69ac`. No frozen solver/checker is rerun; any use
  must retain this validity filter and the comparison-only/once-only contract.
- Fuzzing ownership is now explicit in `DEVELOPMENT_PLAN.md` §8.2: Claude Code
  prepares, submits, monitors, inspects, and reports every solver fuzz job.
  Codex does not probe fuzz logs or artifacts and accepts Claude's terminal
  pass/failure report; failure returns directly to Claude's bounded repair
  workflow. Benchmark guards, proof witnesses, and performance diagnostics
  retain their separate independent-validation contracts.
- 2026-08-26 21:40 EDT immutable fuzz-harness closure — Claude's authoritative
  terminal PASS for campaign
  `20260826-1330-decision-congruence-substitute-consolidation-fuzz` classified
  the exit-127 tails of jobs `1648215` and `1648217` as post-oracle shell
  artifacts: a 16-byte live edit to `tools/fuzz/run.sh` shifted Bash's saved
  file offset while it waited for `fuzz.py`, causing the resumed interpreter to
  execute a later option as a command. Solver/oracle work was already complete
  and clean. `tools/fuzz/slurm.sh` now copies only the required runner, Python
  modules, stress scenarios, and corpus into a unique read-only
  `build/pin-fuzz-harness-*` directory; the batch executes that pin, records
  its aggregate SHA-256, deletes it only on success, and retains it on failure
  for Claude. `run.sh` accepts explicit repository/output roots so snapshot
  execution preserves solver/oracle and failure-bundle locations. Regression
  test `fuzz_slurm_snapshot` proves a mocked three-physical-core submission is
  exactly `-c 6`, `--mem=24000M`, three workers, contains no live-runner path,
  and reproduces the reported snapshot hash. Shell syntax plus the focused
  six-test tooling/documentation set pass. No solver behavior, fuzz seeds,
  case count, or oracle contract changed, and no replacement fuzz campaign was
  run. Frozen reports are
  `benchmarks/reports/2026-08-26-immutable-fuzz-harness.md` and
  `report-to-andrew/2026-08-26-2140-immutable-fuzz-harness/`.
- 2026-08-26 21:50 EDT broad-search collection confirmation — accepted solver
  baseline remains `16d46d5` (repository/tooling checkpoint `498407c`) and the
  worktree source is clean. Prior exact evidence left collection frequency and
  sparse whole-arena relocation open: conflict-9450 Tactix scanned 5.1075x the
  aggregate watch entries of the conservative current-Kissat conflict-10000
  envelope, while fused compact-and-forward was neutral and rejected. Because
  accepted Vivify and Congruence repairs deliberately removed collection
  boundaries, observer-only campaign
  `20260826-2200-rank27-current-collection-observer` remeasures current source
  before any behavior change. Immutable observer pin SHA is
  `12df42b9…fd25`; the observer source is frozen separately and main source was
  restored byte-for-byte. Single solver job `1650175` completed on node018
  with `-c 12`, exactly 48,000 MiB scheduler memory, a 16 GiB process cap, and
  the immutable conflict-9450 input. It requests no proof, executes no second
  solver, and left zero staged-input/certificate residue. It reproduces exact
  accepted semantic SHA `3b87bb36…92c5f` and the current 22-call population.
  Those calls scan 59,408,930 watch entries versus accepted current-Kissat job
  `1647659`'s 19,062,574, a 3.1165x aggregate gap; per-call population is
  already equal within 0.84%. perf loses zero samples and assigns only 0.33%
  to the already rejected forwarding traversal. This confirms invocation
  frequency as the remaining collection owner but provides no contradictory
  evidence to reopen the closed Substitute deferral, stable-span Vivify,
  fused traversal, or quarantined arena-prepass arms. Continue broad-search
  localization outside those closures. Current Kissat was not rerun.
- 2026-08-26 22:00 EDT current broad-search owner profile — current-source
  RelWithDebInfo conflict-stop pin `3cd454de…ed87` passes five focused Kernel,
  scheduling, reset, proof-rule-sync, and rule/policy-boundary tests. Single
  Rank-27 job `1650249` is submitted to node018 with `-c 12`, exactly 48,000
  MiB scheduler memory, a 16 GiB process cap, and one solver invocation. It
  records a DWARF cycles call graph at the exact conflict-9450 common boundary.
  Current Kissat is not rerun: accepted profile SHA `e506ae7f…2924` from
  campaign `20260826-1040` remains the source-owner comparator. Exclude all
  closed propagation, decision, equal-score heap, collection, Vivify,
  Substitute, residual BVE, and Congruence arms; cross-run cycles are
  localization-only. Require exact semantic SHA `3b87bb36…92c5f`, zero lost
  samples, and zero staged-input/certificate residue before using the profile.
  The job completed `0:0` in 15 seconds. It reproduces exact semantic SHA
  `3b87bb36…92c5f`, reports 12.828 seconds internal / 13.61 wall, records
  49.751B approximate cycles in 2,665 samples with zero lost, and leaves zero
  scratch or certificate residue. The leading samples remain in already
  closed or not-worse owner families: propagation, decision/heap selection,
  collection, Substitute, and Congruence. In particular, sampled binary-live
  cost does not override the stronger exact substage evidence that priced
  Tactix binary-liveness construction at 85 ms against current Kissat's 198 ms
  broader dense/binary preparation. No source repair is authorized. Continue
  semantic-quality tracing beyond the neutral equal-score decision 5,795,255
  and name the first unequal-score, candidate-set, learning, pruning, or
  retained-state worse-side owner before changing behavior.
- 2026-08-26 22:40 EDT broad-search semantic-quality continuation — retained
  control traces agree on every conflict and learned clause through conflict
  9,459 despite the closed neutral equal-score choice at recorded decision
  5,795,255. The first later decision mismatch is recorded decision 5,795,465:
  Tactix chooses `-117204`, current Kissat `-117206`, after the common
  `-117212`. Fresh current-source Tactix event-trace pin SHA is
  `2b3a0d06…f2799`; seven focused scheduling/strategy/trace/proof-boundary/
  reset tests pass. Campaign `20260826-2240` is prepared as two independent
  array tasks on node018, each `-c 12`, exactly 48,000 MiB, and one solver
  invocation as job `1650364`. It will compare exact candidate scores,
  topology, pop path,
  phase, and the first resulting conflict. A difference downstream of the
  neutral tie is not an independent repair owner; no behavior change follows
  from this observer alone.
  Both tasks complete `0:0` simultaneously in 33 seconds. Conflict 9,459 has
  the same learned clause, and both sides execute the same 87 score bumps in
  the same order (payload SHA `52031fda…bda3`). At the mismatch, candidates
  117204 and 117206 are present on both sides with the bit-identical score
  `364280533501.80402`; both phases are negative and random choice is inactive.
  Tactix/Kissat merely expose different members of the equal-score cohort at
  the heap root. The later conflict-9,460 difference is downstream of these
  neutral topology choices, not an independent weaker Tactix owner. Close this
  arm, preserve the tie closure, and resume source-equivalent cost localization.
- The user-directed consolidation sequence is complete. Vivify/collection
  produced the sole supported repair and passed the complete gate plus one
  Claude-owned fuzz campaign. The isolated Substitute deferral regressed full
  Rank 27 and is reverted; residual BVE has no current common-state worse-side
  owner; two Congruence closure layouts were exact but negative and are
  reverted. Broad search-quality convergence is now first: freeze current
  Tactix and current instrumented Kissat, then find the earliest portable
  common-state divergence where equal work costs more or equal cost produces
  weaker learning, pruning, simplification, or retained state. Preserve exact
  tick/cost, candidate order, scheduling order, and resulting pruning through
  the common prefix. Do not stop at 300 seconds, and do not reopen closed arms
  without contradictory source-backed evidence.
- 2026-08-25 23:23 EDT consolidation checkpoint — `main` and `origin/main`
  remain synchronized at clean checkpoint `23a3a15`; the only tracked source
  delta is the isolated, explicitly unfuzzed Vivify cross-band reclamation
  candidate (source-diff SHA `a8cd5650…5fdd`). The user-authorized workflow now
  continues Vivify/collection, Substitute, residual BVE, and Congruence as
  separate investigation increments without fuzzing: each owns immutable pins,
  a manifest, focused tests, and performance evidence, and none may be called
  promoted or final. At consolidation, compose only supported repairs, run the
  complete combined B28+C5 checked gate, then one Claude-owned fuzz campaign
  before report/commit/push. Any intervening correctness failure still enters
  the mandatory Claude repair workflow. Every solver invocation is its own
  SLURM job or array task; never serialize two solvers in one allocation, and
  prefer node018 for targeted diagnostics. The eventual consolidation fuzz
  plus report/commit/push is a checkpoint, not an endpoint: immediately
  continue from that accepted pin with the next demonstrated current-Tactix/
  current-Kissat gap.
- The isolated Vivify batching candidate passes its focused Vivify/N1,
  scheduling, trace, strategy, solve-reset, proof-rule-boundary, and proof-sync
  tests. Exact Rank-27 prefix jobs `1647788`/`1647790` preserve the accepted
  64-row event trace SHA `7a6216bb…7cc1` and lower prefix Vivify
  1.730→1.146 s. The checked B+C screen `1647791`/`1647798` is 12/12 valid
  (three SAT models plus three UNSAT proofs per arm), residue-free, and lowers
  Vivify on all six witnesses, but several complete trajectories are nonexact.
  Correct independent full Rank-27 jobs `1647806`/`1647807` are valid UNSAT and
  residue-free on node017: candidate/predecessor wall is 427.29/433.41 s,
  Vivify 38.473/52.515 s, inprocessing 142.955/151.328 s, and RSS
  606,616/688,076 KiB. All printed effects match except final search-mode ticks
  10,353,221,162/10,353,221,174 (-12), so the result is positive investigation
  evidence but fails exact-state acceptance. Earlier sequential jobs `1647799`
  and partial `1647805` are archived and excluded from every claim.
- Independent PRP traces `1647808`/`1647809` localize the earliest nonexact
  effect before BVE semantic work: at conflict 14,770 the formula, state,
  Strengthen effects, BVE candidate order, first pivot 112675, and starting BVE
  ticks are identical, but the policy limit is 1,498,367 predecessor versus
  1,498,368 candidate. Source inspection assigns that allowance to
  `DefaultInprocessScheduler::set_entry_limit` and its search-tick
  now/anchor/reference computation. Observer-only independent jobs
  `1647810`/`1647811` completed SAT on preferred node018; both complete models
  independently verify and residue is zero. They prove the BVE difference is
  exactly the search-tick reference 14,983,678→14,983,680, not BVE formula or
  representation population. The first earlier meter differences appear after
  the phase-2 outer-0 Vivify dose: candidate Substitute +1, Vivify +6, and
  TransitiveReduce -1. Independent semantic-candidate traces
  `1647812`/`1647813` each ran one solver on node018 and both returned SAT.
  Their wrappers failed only because the copied checker launcher lacked its
  adjacent Python module; after restoring that immutable module, both saved
  models independently satisfy all 1,060,646 clauses, with zero residue and no
  solver rerun. The 158,607-row traces per arm localize the first cumulative
  difference at conflict 2,958, irredundant Vivify ordinal 26: same clause,
  pose, candidate order, decisions, formula/result state, and propagation
  count, but cumulative ticks are 192,565 candidate versus 192,567 predecessor.
  Exact watch trace `1647814`/`1647815` assigns the saving to one fewer large
  dereference and relocation at scan 177388 (373 versus 375 total ticks over
  the candidate), with identical pruning. Current Kissat performs no collection
  inside `kissat_vivify`, so this avoided eager relocation is source-aligned.
  The first unfavorable cumulative crossing occurs at ordinal 36: exact
  node018 watch jobs `1647816`/`1647817` are checked SAT and residue-free; both
  scan the same 67 literals and end with the same 435 watch words, 18 binary
  assignments, 48 large assignments, garbage result, and propagation count.
  Candidate enters with 447 versus 443 watch words, performs 55/5 versus 53/3
  large dereferences/relocations, and costs 249 versus 245 ticks, moving the
  cumulative delta from -2 to +2. This is a bounded cost tradeoff from keeping
  in-round, reference-aligned watch state, not weaker learning or pruning.
  Complete PRP effects remain identical and candidate Vivify time under trace
  falls 8.438→7.124 s. Together with the valid long Rank-27 and B+C evidence,
  retain this isolated batching repair as supported but explicitly unfuzzed
  consolidation material; it is not promoted or final. No fuzz campaign is
  active or due. Substitute residual stage `20260825-2350` freezes current
  Tactix observer SHA `531a7972…2134` separately and reuses current Kissat's
  accepted `1647662` observer evidence without rerunning it. Single-solver job
  `1647818` is submitted to preferred node018 with two CPUs, 16 GiB, a
  conflict-9450 stop, substage/allocation tracing, and perf. The promoted
  internal-import-rank repair remains closed. The leading residual hypothesis
  is the 325 ms of changed-round arena collection left in Tactix's exact stage:
  Kissat performs no collection inside `substitute_rounds` and instead rewatches
  once after the call. Submission is not a result, source is unchanged, and
  this stage remains explicitly unfuzzed.
- Residual observer `1647818` completed 0:0 on node018 in 18 seconds with the
  exact accepted conflict-9450 semantic counters: 13 canonical rounds, 335,924
  images, 21,568,289 Substitute ticks, 59,108 substituted variables, and zero
  residue. Current substage totals are snapshot 162 ms, binaries 334 ms, large
  clauses 189 ms, post-large 38 ms, and changed-round collection 286 ms;
  `kernel_collect_garbage` remains a 2.83% complete-prefix cycles owner with no
  lost samples. The isolated call-level reclamation candidate now uses existing
  persistent clause-order aliases to traverse later rounds in exact logical
  order and collect once at call exit, matching Kissat's no-collection-inside-
  `substitute_rounds` lifecycle. Focused Release guards pass 16/16. Independent
  one-solver jobs `1647819` predecessor and `1647820` candidate are submitted
  in parallel to preferred node018 with identical conflict-9449 trace,
  Substitute substage, and perf observers. The candidate is unfuzzed and not
  promoted or final; exact common-prefix parity is required before longer
  Rank-27/B+C evaluation.
- Jobs `1647819`/`1647820` completed independently on node018 with intentional
  conflict-9450 UNKNOWN and zero residue. Both arms retain identical semantic
  counters and byte-identical 64-row trace SHA `7a6216bb…7cc1`, fixing common-
  prefix rule/candidate/scheduling, learning, pruning, and result state. The
  candidate removes 336 ms of per-round collection and lowers rewrite substage
  total 1,357→1,039 ms, but one call-end collection remains inside reported
  rule time and the heavy observer shows neutral total/Substitute timing. More
  importantly, Substitute ticks rise exactly 21,568,289→21,570,089 (+1,800):
  Tarjan charges cache lines for complete watch-list sizes as current Kissat
  does, and the first retained physical population difference is +128 global
  watch words before canonical SCC round 5. SCCs, representatives, images,
  retirements, and pruning are unchanged, so this is a source-lifecycle cost
  difference rather than weaker work, but it prevents exact-cost acceptance.
  Keep the repair isolated and unfuzzed; per the required long-horizon policy,
  price its frozen production pair over full Rank 27 and the predeclared six
  high-Substitute B+C instances before disposition. No full gate or fuzz is
  authorized yet. The residual observer manifest is now terminal with its
  exact counters, artifact hashes, and zero-residue outcome. Frozen long-run
  production SHA `ad8fe07f…c6ad6f` is running against isolated Vivify
  predecessor `4b3f7467…bc5e6a`: B+C array `1647821` has twelve independent
  one-solver tasks on node018 plus non-solver aggregate `1647828`; full
  Rank-27 predecessor `1647822` and candidate `1647823` are independent
  node018 jobs. Both now complete UNSAT with zero residue, and reject the
  Substitute batch decisively: candidate/predecessor internal time is
  562.824/552.836 s, wall 563.34/553.35 s, Substitute 28.651/27.347 s, and
  solver RSS 768,996/606,948 KiB. The candidate also changes conflicts,
  decisions, propagations, Substitute work/effects, scheduling, Sweep, BVE,
  Congruence, learning, pruning, and retained state. This fails N4 and
  whole-run acceptance; the Substitute-only source delta has been removed,
  the independent Vivify candidate remains, and the reverted build plus 13
  focused N1/N4/rule/proof/reset tests are green. The still-running immutable
  T105 tasks will be classified and aggregated, but cannot rescue this repair.
  They now complete with aggregate `1647828`: all 12/12 B+C tasks are valid
  (three checked SAT models and three verified UNSAT proofs per arm), with no
  invalidity, cleanup failure, or residue. Candidate/shipped PAR-2 is a neutral
  767.397/768.797 s (0.998179), but the intended owner is not repaired: T105
  Substitute worsens 32.888→33.671 s despite an unrelated changed-trajectory
  total gain, and the circuit's Substitute saving accompanies a total loss.
  Summary SHA is `36fc72a2…7a1844`. Correctness passes; causal performance and
  exact-state acceptance fail, so no full combined gate or fuzz is authorized
  for this rejected Substitute increment.
- Residual BVE disposition `20260826-0015` reuses frozen authorities and makes
  no source change. Current Vivify-only Tactix job `1647807` spends 15.958 s
  across 1,699,306 BVE applications, 346,693 eliminations, 873,301 definition
  checks, and 115,624,070 definition ticks; frozen optimized current Kissat
  jobs `1644639`/`1644640` spend 16.32 s across 1,621,010 attempts, 364,315
  eliminations, 1,137,044 checks, and 160,758,923 definition ticks. The full
  trajectories differ, so the yield totals cannot prove a same-state weakness;
  throughput is not worse, and the last exact worse-side event was the already
  closed antecedent-size admission defect. N4 therefore authorizes no residual
  BVE edit. Reopen only on a new exact common-state tick/order/yield divergence
  or comparable-work profile loss; proceed to the larger frozen residual
  Congruence closure representation owner.
- Residual Congruence observer campaign `20260826-0020` freezes current
  Vivify-only semantic observer SHA `531a7972…2134` and submits single-solver
  node018 job `1647836`. It uses the existing allocation trace only—no source
  or behavior delta—to refresh exact conflict-9450 gate, immutable value-index,
  representative-index, work/reschedule, closure-table, memory, and timing
  populations. Frozen current-Kissat job `1645878` and accepted funnel/DWARF
  traces remain the comparator; rejected page-tail and key-borrow arms stay
  closed. No Congruence prototype is authorized until this exact current
  population is classified. Job `1647836` now completes 0:0 in 17 s and
  reproduces 9,450 conflicts, 5,795,248 decisions, 96,338,812 propagations,
  1,734,747 gates, 58,738 merges, 181 units, and 445,868,596 ticks; cumulative
  Congruence is 1.830 s. The three rounds build 2,434,835 / 1,651,958 /
  1,580,832 immutable value nodes. Round 1 runs 729,809 items (9,343
  reschedules) and already holds 1,135,883 representative nodes at item
  500,000; later rounds have no closure reschedules. Trace SHA is
  `898ccb3c…0733e2`, residue zero. This confirms duplicate populations but not
  yet a safe mutable-list design. Observer-only key-shape job `1647837` now
  measures exact canonical-size bins and final table/node populations on
  node018; its focused N1/N4/proof/reset suite is 6/6.
  Job `1647837` completes exact and residue-free. Across 1,606,240 Canon::Ok
  keys, 1,472,153 are size two and 102,123+216 are size three: 98.01% fit
  three literals. The tables copy 3,395,476 key literals into 1,541,114
  records, while representative indices allocate 3,565,639 nodes; only round
  1 reschedules (9,343). This authorizes a source-semantic three-inline-literal
  record (natural ITE arity) with paged tails for longer keys and smaller paged
  allocations. It must retain identical hashes/equality, insertion/replacement
  order, ticks, candidates, proof behavior, and resulting state. Observer log
  SHA is `144b7af8…1d48f`; trace SHA `e95eb099…1e049`; observer source is removed
  before the isolated representation prototype.
- Isolated Congruence prototype `20260826-0021` implements the authorized
  source-semantic table representation: the natural three-literal prefix is
  inline in each closure record, only longer-key suffixes spill into paged
  storage, and allocation requests are bounded to 1.5 MiB record / 1 MiB tail
  pages. Hashing, equality, table insertion/replacement, traversal, gate ids,
  ticks, proofs, and scheduling are unchanged by construction. Release builds
  cleanly and the selected Kernel/proof/scheduling/strategy/event-trace/
  Congruence/API/reset suite passes 8/8; the Congruence test includes an
  arity-65 AND and therefore exercises the spilled tail. Immutable event-trace
  pins are predecessor `531a7972…2134` and candidate `5596fffa…420a`.
  Independent one-solver jobs `1647838`/`1647839` are submitted in parallel to
  node018 with the same conflict-9450 stop, compact conflict-9449 trace, exact
  counters, and memory/timing stages. Require exact candidate/order/tick/
  scheduling/learning/pruning/result state before pricing the representation.
  This repair remains isolated and unfuzzed, so it is neither promoted nor
  final.
  Both jobs complete 0:0 together on node018 with intentional UNKNOWN and zero
  residue. Exact state is preserved: all semantic counters match, all 270
  logical memory-stage rows match after removing only RSS/VM, and both 64-row
  event traces are byte-identical at accepted SHA `7a6216bb…7cc1`. The
  three-inline layout is rejected on its owner: first-round closure-loop RSS
  falls 375,716→367,392 KiB and VM 452,752→387,216 KiB, but whole-process peak
  is 395,460→398,488 KiB, closure is 628→635 ms, and Congruence is
  1,984→2,039 ms. No long run or fuzz is warranted for a same-prefix negative.
  The frozen size distribution supports a distinct two-inline layout: 91.65%
  of keys remain fully inline, records shrink 24→20 bytes, and only 313,248
  cumulative suffix literals spill. Test that separately against the same
  predecessor before any long evaluation.
  Separate inline-two campaign `20260826-0023` freezes predecessor observer
  `531a7972…2134` and candidate `e35f2df8…3edd`; Release and the same eight
  focused guards are green. Independent single-solver jobs `1647840`/`1647841`
  are submitted together to node018 under the exact conflict-9450 trace and
  memory/timing contract. The candidate is isolated and unfuzzed.
  Both jobs start together and complete 0:0 in 44 seconds with intentional
  UNKNOWN and zero residue. The 64-row traces are byte-identical at
  `7a6216bb…7cc1`, all semantic counters match, and all 270 logical memory rows
  match after removing only RSS/VM. The intended owner is directionally
  positive: first-round closure-loop RSS 377,208→367,392 KiB and VM
  452,752→387,216 KiB, whole-process peak 398,488→396,952 KiB, cumulative
  closure 639→624 ms (match/table/proof 252→234 ms), Congruence
  1,998→1,983 ms, and total prefix 39,940→39,655 ms. This small exact-stage
  result authorizes only independent full Rank-27 arms plus a predeclared
  checked six-instance B+C high-Congruence screen; no full gate or fuzz yet.
  Long evaluation is frozen in separate campaigns. Independent proofless
  Rank-27 jobs `1647844` predecessor and `1647843` candidate each run exactly
  one Release solver on node018 with two CPUs, 16 GiB, and 1,200 seconds.
  Checked B+C array `1647842` plus after-any aggregate `1647849` runs twelve
  one-solver tasks at throttle four on node018. The six instances were selected
  before submission from frozen accepted guard `20260824-2332` as the top four
  Tier-B and top two Tier-C Congruence-time exposures: PRP, hwmcc17, veer,
  DivS, T105, and the 500-gate circuit. Each arm must independently validate
  three SAT models and three UNSAT proofs with no loss or residue. Submissions
  are not results; the repair remains isolated and unfuzzed.
  Both long controls are terminal and reject the two-inline representation.
  The checked screen passes 12/12 (three verified SAT plus three verified UNSAT
  per arm), zero invalidity/loss/cleanup/residue, and every paired non-time
  statistics image is exact. Summed Congruence improves only
  44,964→44,605 ms (-0.80%; four of six improve), while PAR-2 regresses
  742.033→747.082 s. Independent Rank-27 jobs start together and are exact at
  normalized SHA `10372d94…ffd0`, but Congruence worsens
  18.088→18.171 s, internal/wall 544.007/544.52→546.175/546.69 s, and observed
  RSS 606,620→690,300 KiB. This does not close the current-Kissat gap and is
  not robustly positive over the required horizon. Reject and revert only this
  isolated Congruence delta; no combined gate or fuzz is authorized for it.
  **Open residual Congruence issue:** the refreshed current-Tactix
  conflict-9450 observer is about 1.830 s versus the accepted current-Kissat
  comparator at 1.644 s (approximately 186 ms / 11% slower). The three-inline
  and two-inline ClosureTable record layouts are closed negative above and must
  not be reopened as generic key-layout guesses: both preserve exact state, but
  neither improves full Rank 27, and both are fully reverted. Reopen Congruence
  only by tracing the next exact common-state worse-side owner inside closure
  matching/maintenance (canonical keys, table probes/equality, proof work,
  representative occurrence maintenance, rescheduling, and forwarding), with
  exact ticks/order/state and a positive full Rank-27 plus B+C result before
  consolidation. Current solver behavior is unchanged by these experiments.
  Congruence source is now exactly clean, the rebuilt Release returns to
  Vivify-only SHA `4b3f7467…bc5e6a`, and eight focused tests pass. This reaches
  the user-authorized consolidation point with Vivify as the sole supported
  repair; rejected Substitute, residual BVE, and Congruence candidates are not
  mixed into it. Complete frozen B28+C5 checked array `1647857` plus after-any
  aggregate `1647866` is submitted to node018 as 66 independent one-solver
  tasks (`0-65%8`), two CPUs, 16 GiB, 1,200 s solver and 4,800 s checker
  contracts. It compares accepted predecessor `c7c46bfa…57d4` with exact
  candidate `4b3f7467…bc5e6a`; matrix SHA is `9cc79541…0e2`. Require 17 checked
  SAT plus 16 verified UNSAT per arm, no solve loss/invalidity/cleanup/residue,
  and PAR-2 at most 1.10. Submission is not a result. Only a terminal PASS
  authorizes one Claude-owned fuzz campaign before report/commit/push, after
  which convergence must immediately resume at the next Kissat gap.
- 2026-08-26 Claude-owned consolidation fuzz campaign
  `20260826-0103-vivify-crossband-reclamation-fuzz` — authorized by the
  terminal-PASS consolidation gate `20260826-0040-BC-vivify-batch-consolidation`
  (66/66 valid, 17 checked SAT + 16 verified UNSAT per arm, zero
  invalidity/loss/cleanup/residue, PAR-2 0.9688, summary SHA `4ef52978…9925`).
  Prep job `1647924` is terminal PASS on node017: exact source identity (base
  `23a3a15`, src/tests diff SHA `a8cd5650…5fdd`) verified before and after a
  fresh ASan/UBSan Debug pin build (pin SHA `93997df9…ecc4`, mode 0550),
  focused ctest 10/10, and the reachability probe proved every fatal row: 13
  of 1,500 deterministic quick-budget DefaultPolicy cases fire Vivify with
  vivified≥1, gdb direct-trace witnesses on five of them hit the changed
  cross-band deferred rebuild (vivify.cpp:1646 if batch.changed) and dose-end
  batched collect (default_policy.cpp:3111 if reclamation.changed) in 5/5,
  and stress/random shapes fire their rows. Four independent SLURM jobs are
  submitted from that immutable pin: DefaultPolicy native-LRAT leg `1647925`,
  Vivify-targeted stress native-LRAT leg `1647926`, random-policy native-LRAT
  leg `1647927` (each corpus + 10,000 generated cases, kissat differential,
  independent model/proof oracles, fatal reach rows), and gdb cross-band
  reach phase `1647928` with full per-case oracles. Submission is not a
  result; Claude Code exclusively owns monitoring, artifact inspection, and
  the terminal report, and Codex does not probe fuzz artifacts.
  Claude Code's authoritative terminal report now classifies the campaign
  TERMINAL FUZZ PASS. All four jobs completed 0:0; source identity and both
  binary identities re-verified at terminal inspection. Corpus is 81/81
  replays (27 per leg) and generated fuzz 30,000/30,000 with zero failures:
  15,797 independently checked SAT models, 10,889 native LRAT proofs accepted
  by pinned `lrat-check`, kissat differential agreement on all 26,686 verdict
  cases, and 3,314 bounded five-second nonterminations carrying no oracle
  claim. Fatal reach rows are satisfied at the source in every leg (default
  Vivify/VivifyChanged 99, stress 5,869/4,310 with 2,282,598 target
  applications, random Vivify 2,458), and the changed cross-band path is
  directly witnessed: 8/8 gdb reach cases hit the deferred inherited-order
  rebuild (31 hits) and dose-end batched collect (11 hits), each oracle-clean.
  Zero sanitizer/crash/invariant findings, zero model/proof rejections, zero
  certificate or job residue. Terminal authority is
  `benchmarks/runs/20260826-0103-vivify-crossband-reclamation-fuzz/`
  (manifest `status: terminal-pass`). This closes the consolidation fuzz
  gate; Codex accepted Claude's report without probing fuzz artifacts.
- 2026-08-26 09:45 EDT Vivify wrap-up — frozen B28+C5 manifest
  `20260826-0040-BC-vivify-batch-consolidation` is terminal PASS with summary
  SHA `4ef52978…9925`: 66/66 valid, 17 checked SAT models plus 16 verified
  UNSAT proofs per arm, candidate/predecessor PAR-2
  1477.077/1524.592 seconds (0.968834x), and zero invalidity, solve loss,
  cleanup failure, or certificate residue. Claude's terminal fuzz PASS above
  completes the one authorized consolidation campaign. A fresh Release build
  reproduces candidate SHA `4b3f7467…bc5e6a`; CTest is functionally 73/74,
  with only the already recorded opt-in documentation-size debt failing.
  Final three-page Andrew report is
  `report-to-andrew/2026-08-26-0945-rank27-vivify-crossband-reclamation/`;
  TeX SHA is `e77d3f51…fc911`, PDF SHA is `51322aa6…e6b28`, and the second
  LaTeX pass has no box or undefined-reference warning. This coherent Vivify
  increment is ready to commit and push; afterward immediately freeze that
  commit as the broad search-quality baseline.
- 2026-08-26 11:31 EDT broad-search decision-journal checkpoint — Exact
  common-profile source/disassembly localizes a production-only equal-work
  owner: with `TACTIX_ENABLE_RULE_JOURNAL` compiled out, the default decision
  path still constructed `"literal=" + to_string(external decision)` before
  the journal helper discarded it on every decision. A lazy helper now
  evaluates this diagnostic description only in journal builds and invokes
  the unchanged decision transaction directly otherwise. Journal-off focused
  tests pass; journal-on record/replay still emits the exact `literal=` entry.
  Prefix job `1647957` preserves accepted normalized SHA `3b87bb36…92c5f`
  and lowers sampled cycles 50.902B→50.405B; exact trace job `1647959` is all
  64 rows byte-identical at accepted SHA `7a6216bb…7cc1`. Full independent
  Rank-27 job `1647961` is exact UNSAT at normalized SHA `10372d94…ffd0` and
  improves internal/wall 365.249/365.72→359.799/360.29 s; all 175,272,633
  decisions and every printed effect/tick/state field match, while search plus
  scaffolding falls 233.567→228.324 s. Predeclared highest-decision B+C screen
  `1647963`/`1647964` passes 12/12: two checked SAT models and four verified
  UNSAT proofs per arm, every paired non-time image exact, candidate/shipped
  632.261/635.585 s (0.994770), all six pairs faster, and zero invalidity,
  solve loss, cleanup failure, or certificate residue. Retain this isolated
  source repair for the later combined B28+C5 gate and one Claude-owned fuzz
  campaign, but do not call it promoted/final. The equal-score Rank-27 heap
  split observed by `1647955` has identical scores/bump order and only
  pre-existing heap topology, so it is neutral and closed without behavior
  change. Resume the next exact current-Kissat worse-side owner immediately.
- 2026-08-26 11:42 EDT current collection-lifecycle checkpoint — Read-only
  source backtraces in campaign `20260826-1140` classify all 24 collection
  entries reached through Rank-27 conflict 9449: seven sparse-root, two
  Congruence, eight Substitute, four Vivify, two phase-tail, and one whole
  variable-compaction call. The accepted Vivify cross-band repair therefore
  removed twelve of the old sixteen Vivify calls and reduced the frozen
  Tactix population from 36 to 24, while current Kissat's accepted envelope
  remains seven sparse collections. The 17 remaining maintenance traversals
  are chronologically source-owned; the earliest extra one is Congruence
  forward subsumption at conflict zero. Corrected GDB job `1647977` reached
  the complete observed call population but GNU timeout sent SIGTERM at 300 s
  before the conflict-9450 stop could print a verdict/statistics image, so it
  is ownership evidence only—not solver correctness, exact terminal state, or
  performance evidence. Initial job `1647976` failed before solver execution
  on an inlined debugger expression. Both attempts leave zero staged-input,
  scratch, proof/model, checker, and certificate residue; trace SHA is
  `9efc4b04…498b8`. Continue with frozen exact Congruence/common-state evidence
  and source substages; do not reopen the rejected key layouts, fused
  compact-forward arm, quarantined arena prepass, or Substitute deferral.
- 2026-08-26 12:08 EDT isolated Congruence collection-lifecycle stage — Source
  and frozen stage evidence show the first productive Congruence forward pass
  packs about 587 thousand live arena words, then the immediately following
  Substitute packs the arena again; current Kissat reconnects Congruence
  watches without rule-local collection. The isolated general candidate now
  rebuilds the live large-watch image in the same physical live-clause order
  and leaves reclamation/Ref forwarding/generation invalidation to the
  existing phase boundary. It contains no benchmark condition or strategy,
  tick, proof, candidate, or schedule change; 15/15 focused N1–N4, proof,
  Congruence/Substitute/Sweep, trace, boundary, and reset guards pass. Exact
  node018 prefix array `1648078` reproduces accepted normalized SHA
  `3b87bb36…92c5f` in both arms and removes exactly the two productive
  Congruence collections (current population 24→22). Normal prefix timing is
  neutral/noisy at 13.097→13.094 s and sampled cycles rise 0.52%, while
  independent trace array `1648079` is byte-identical for all 64 rows at
  `7a6216bb…7cc1` and under trace overhead lowers Congruence
  1.804→1.686 s and total 37.158→36.963 s. Per the required long-horizon
  policy, neither accept nor reject from this short disagreement. Production
  predecessor/candidate SHAs are `fd22885a…e0950` / `184a6536…92a7a`;
  independent full Rank-27 array `1648082` and checked six-instance
  Congruence-relevant B+C array/aggregate `1648083`/`1648085` are submitted to
  node018. Every task invokes one solver; the screen requires three checked
  SAT models plus three verified UNSAT proofs per arm and zero invalidity,
  solve loss, cleanup failure, or certificate residue. Candidate remains
  isolated, unfuzzed, and not promoted/final.
- The Congruence lifecycle long controls are now terminal and retain the
  isolated repair. Full Rank-27 array `1648082` is exact UNSAT in both arms at
  normalized SHA `10372d94…ffd0`: candidate/predecessor internal time is
  356.788/358.115 s, Congruence 13.366/14.432 s (-1.066 s, -7.39%), and
  inprocessing 129.640/131.202 s; all 175,272,633 decisions and every printed
  work/tick/effect/state field match. Checked B+C array/aggregate
  `1648083`/`1648085` is terminal PASS 12/12 with three verified SAT models
  and three verified UNSAT proofs per arm, byte-identical paired non-time
  images, and zero invalidity/loss/cleanup/residue. Congruence improves on all
  six instances and sums 43.935→42.004 s (-1.931 s, -4.40%); total time is
  noisy-negative 698.780→702.401 s (+0.52%) despite exact work, while maximum
  RSS falls 2.931→2.914 GiB. Together with the exact Rank-27 owner/total win,
  retain this general repair as supported unfuzzed consolidation material;
  it is not promoted/final. A strengthened standalone Congruence N1 test now
  proves stable Refs, no watches into garbage before the deferred boundary,
  and correct later forwarding/reclamation. Next frozen Substitute evidence
  identifies a separate candidate: six of thirteen canonical rounds produce
  zero binary images, yet canonical Substitute still walks/compacts the global
  binary-watch population. Preserve exact global import order while visiting
  only affected owners; do not mix this next edit into the already frozen
  Congruence pins.
- 2026-08-26 12:43 EDT isolated Substitute affected-owner stage — The
  canonical path now marks retiring literals plus their binary mirror owners,
  then retains the former global import/sign and per-list order over only that
  induced population. Sweep/session behavior, proof work, candidate set,
  ticks, scheduling, large rewriting, and collection are untouched. Focused
  guards pass 16/16. Initial prefix array `1648097` completed both exact solver
  runs but failed only when its wrapper tried to copy an event-trace-only file
  compiled out of Release; corrected immutable-pin array `1648101` is terminal
  exact. Both arms reproduce normalized SHA `3b87bb36…92c5f`; total
  13.071→12.830 s, Substitute 1.413→1.176 s (-16.8%), and sampled cycles
  50.458B→49.430B (-2.04%) with zero loss. Intended substages fall snapshot
  161→50 ms and binary rewrite/compaction 248→127 ms; lemmas, large work,
  collection, and all semantic effects remain equal. Independent trace array
  `1648098` is byte-identical for 64 rows at `7a6216bb…7cc1` and lowers traced
  Substitute 3.645→3.350 s. Production predecessor/candidate pins are
  `184a6536…92a7a` / `acf9e716…2e013`; full Rank-27 array `1648103` and checked
  high-Substitute B+C array/aggregate `1648104`/`1648106` are submitted to
  node018. Each task invokes one solver; the screen requires three checked SAT
  plus three verified UNSAT claims per arm and zero invalidity/loss/residue.
  Candidate remains isolated, unfuzzed, and not promoted/final.
- The Substitute long controls are terminal and retain the isolated repair.
  Full Rank-27 array `1648103` is exact UNSAT in both arms at normalized SHA
  `10372d94…ffd0`: internal time falls 358.157→354.539 s (-3.618 s,
  -1.01%), Substitute 21.704→18.528 s (-14.63%), inprocessing
  129.997→126.653 s, and maximum RSS 672,116→608,084 KiB; all
  175,272,633 decisions and every printed rule work/tick/effect, scheduling,
  learning, pruning, and retained-state field match. Checked B+C
  array/aggregate `1648104`/`1648106` is terminal PASS 12/12 with three
  independently checked SAT models and three verified UNSAT proofs per arm,
  zero invalidity, solve loss, cleanup failure, or certificate residue, and
  candidate/predecessor total 716.174/720.387 s (0.994152x). Substitute time
  improves on all six instances: 1126→945, 2031→1801, 1739→1625,
  579→462, 15236→14304, and 31860→28433 ms. Four paired non-time
  images are byte-identical. FmlaEquivChain differs only by one residual
  binary-propagation pricing classification; the circuit differs only by nine
  such classifications and their 28 logical mode ticks. A source audit fixes
  the cause: omitted unrelated-list `compact()` calls no longer incidentally
  refresh conservative `binaries_first` metadata, while watch contents/order,
  decisions, conflicts, propagations, every inprocessing effect, schedule,
  learning, pruning, verdict, and semantic state stay equal. This bounded
  cost-only delta does not weaken the solver; retain the general repair for the
  combined B28+C5 consolidation gate. It remains unfuzzed and not
  promoted/final.
- 2026-08-26 12:39 EDT combined consolidation gate submitted — The accepted
  Vivify baseline (`4b3f7467…bc5e6a`, commit `d8da6a1`) and exact combined
  decision-journal + Congruence-lifecycle + Substitute-affected-owner candidate
  (`acf9e716…2e013`, source diff `f052afee…a90cc`) are frozen in campaign
  `20260826-1300-BC-decision-congruence-substitute-consolidation`. Immutable
  matrix SHA is `9cc79541…c30e2`; all 28 Tier-B and five predeclared Tier-C
  input hashes plus pinned validators are recorded in its manifest. Complete
  66-task checked array `1648120` and after-success aggregate `1648129` are
  submitted to node018 with one solver invocation per task, `-c 2`, 16 GiB,
  and 1,200 s solver limit. Passing requires 17 independently checked SAT
  models and 16 verified UNSAT proofs per arm, no solve loss/invalidity,
  complete cleanup, zero certificate residue, and at most 1.10x frozen PAR-2.
  The source remains unfuzzed and not promoted/final; terminal PASS authorizes
  exactly one Claude-owned exact-source fuzz campaign.
- The combined consolidation gate is terminal PASS. Array `1648120` completed
  all 66 independent tasks 0:0 on node018 and aggregate `1648129` completed
  0:0. Each arm has 33/33 valid results: 17 independently checked SAT models
  and 16 independently verified UNSAT proofs. Invalidity, missing tasks, solve
  loss, cleanup failure, and certificate residue are all zero. Candidate/
  accepted-baseline PAR-2 is 1446.060/1449.704 s (0.997486x), and peak RSS is
  2.914/2.930 GiB; summary SHA is `b57977a…eec4`. A fresh local Release build
  is byte-identical to candidate `acf9e716…2e013`; the complete suite is
  functionally 73/74, with only the already recorded opt-in documentation-size
  debt failing. Gate PASS now authorizes the one Claude-owned exact-source
  fuzz campaign. The repairs remain unfuzzed and not promoted/final until
  Claude's authoritative terminal verdict.
- 2026-08-26 13:56 EDT consolidation-fuzz handoff remains open — Claude Code
  prepared and owns the exact-source campaign, but three continuation attempts
  ended at the Claude API boundary with DNS `ENOTFOUND` before returning a
  terminal report. This is neither PASS nor FAIL. Codex has not inspected fuzz
  jobs, logs, results, or artifacts. Independent current-source Release remains
  functionally 73/74 (all 73 solver/proof/model/N1–N4/boundary/cleanup tests
  pass; only known documentation-size debt fails), and candidate SHA remains
  `acf9e716…2e013`. The separate Andrew consolidation report is compiled as a
  pending handoff; source is still unpromoted and must not be committed until
  Claude supplies its authoritative terminal verdict.
- Claude subsequently reconnected with external-network permission. Its own
  report classifies prep attempt 1 as green except for an unsatisfiable
  recipe-reach check—not a solver correctness, sanitizer, crash, model, or
  proof failure—and says its temporary build directories were removed. Claude
  owns repaired prep job `1648211`, which uses the same-case deferred
  Congruence-rebuild/collection witness and deciding-case lazy-journal selector;
  its live watcher has not yet returned a terminal verdict. After reconnecting,
  Claude's own interim audit reports every completed corpus leg 27/27 with zero
  failures, exactly 25 diagnostic timeout input bundles retained per completed
  leg, zero certificate residue, and two original campaign legs still running.
  This is not a second campaign or a repeated timing experiment. Codex has not
  queried the jobs or opened any fuzz artifact. Continue the same Claude-owned
  campaign through those last legs, terminal identity/cleanup audit, and
  authoritative PASS/FAIL before promotion.
- Independent exact-source review rebuilds ordinary Release to the frozen
  consolidation candidate SHA `acf9e716…2e013`; the full Release suite remains
  73/74, with every functional/N1–N4/proof/model/cleanup test passing and only
  the pre-existing documentation-size debt failing. Fresh Debug focused
  journal/Substitute/Congruence/boundary/solve-reset tests pass 5/5 under
  ASan/UBSan. An initial local LeakSanitizer invocation was infrastructure-
  invalid because LSan cannot run under the container ptrace wrapper; disabling
  only leak detection makes the identical binaries pass, with no sanitizer or
  solver failure reported. Claude's cluster campaign remains the authoritative
  full sanitizer/fuzz gate.
- Claude's resource-shape audit finds the current consolidation campaign
  under-parallelized: its six independent generated/stress leg jobs each use
  `-c 2` and two workers, for about six physical cores at peak, with no arrays
  or throttle. This does not reduce case/oracle coverage, but it stretches two
  stress legs to roughly four hours. Because those legs were already at
  8,700/10,000 and 9,000/10,000 with zero failures when discovered, retain and
  finish them rather than discard valid work or repeat fuzzing. Allocation-only
  probe `1648629` validates the future 50-physical-core shape: `-c 100`,
  `--mem=400000M`, mask `0-49,96-145`, and 50 workers. The generic fuzz wrapper
  now makes physical cores explicit and no longer mistakes its old 50 logical
  CPUs for 50 physical cores. Future Claude-owned generated fuzz legs use this
  contract when capacity permits; narrower prep/reach work requires a recorded
  reason. No solver ran in the probe.
- Claude's 18:00 ownership audit reports Sweep-stress job `1648217` completed
  its corpus 27/27 and generated 10,000/10,000 with zero failures before SLURM
  recorded wrapper exit 127. All 2,541 SAT models and 3,451 UNSAT proofs in its
  5,992 verdict cases passed their independent oracles; 4,008 bounded
  nonterminations carry no oracle claim, every required rule reached, and the
  target fired 893,020 times. The post-fuzz exit is an infrastructure artifact:
  commit `8a9c8f7` changed only a comment length in live `tools/fuzz/run.sh`
  while Bash was blocked inside `fuzz.py`; on return its stale file offset
  entered the shifted final line and treated `--require-reached` as a command.
  This is not a solver, sanitizer, model, proof, or fuzz-acceptance failure.
  Congruence-stress `1648215` is 9,800/10,000 with zero failures and is expected
  to encounter the same benign post-summary wrapper exit. Do not touch the live
  script again until it terminates. After Claude's terminal audit, make future
  fuzz submissions execute an immutable harness snapshot so repository edits
  cannot alter a running job.
- Claude's authoritative terminal report classifies campaign
  `20260826-1330-decision-congruence-substitute-consolidation-fuzz` PASS and
  updates its manifest to `terminal-pass`. All five generated legs complete
  50,000/50,000 with zero failures: 20,927 SAT models and 17,727 native-LRAT
  UNSAT proofs validate independently, and pinned Kissat agrees on all 38,654
  verdict cases. Corpus is 135/135 with zero failures; 40/40 dedicated reach
  cases are oracle-clean and exceed every lazy-journal, deferred-collection,
  affected-owner, productive/zero-image, and session floor. Claude reports no
  sanitizer finding, crash, invariant abort, rejected model/proof, or
  differential disagreement. Bounded five-second nonterminations carry no
  oracle claim; exactly 25 diagnostic inputs per leg are retained. Certificate,
  proof, model, checker, and node-local scratch residue are zero. No job was
  cancelled or replaced. Stress jobs `1648215`/`1648217` end 127 only after
  their complete accepted fuzz summaries because the live `run.sh` byte-offset
  shifted during commit `8a9c8f7`; Claude records this as a post-oracle
  infrastructure artifact, not a fuzz failure. The complete B28+C5 gate plus
  this PASS authorizes source promotion and immediate continuation.
- 2026-08-26 timing-stability investigation — The complete cluster study is
  preserved in
  `benchmarks/reports/2026-08-26-rank27-timing-stability.md` and its Andrew
  TeX/PDF draft. Isolated current Tactix proof-off median is 367.312 s and
  proof-on 370.893 s; deliberate node co-residency adds 52 s / 14.23% cycles
  at unchanged instructions. Local-NVMe staging, page-fault, filesystem, and
  proof-write evidence excludes I/O as the 300–500 s cause. User review rejects
  whole-node reservation even for diagnosis: the completed isolation controls
  remain historical causal evidence, but no future job may request
  `--exclusive=user --mem=0`. Exact frozen Kissat 4.0.4 itself varies
  278.187→320.307 s (+15.14%) on the same input while producing the identical
  independently verified 1,320,603,112-byte proof. New exact-work Kissat
  campaign `20260826-1430-rank27-kissat-c4-timing-variation` proves the
  placement mechanism: simultaneous packed `-c 4` runs span 253.90–321.32 s
  as three solvers share one six-core L3 domain, while three separated `-c 4`
  controls span only 258.60–260.12 s. Reserving exactly one L3 domain with
  ordinary shared-node `-c 12` collapses six simultaneous Kissat controls to
  252.75–260.76 s. The same `-c 12` remedy on accepted Tactix array `1648205`
  reproduces exact normalized trajectory SHA `10372d94…f1ffd0` in
  367.045–372.354 internal seconds, median 368.307 and 1.441% full range; all
  six are below 400 s and residue-free. The user-approved policy now uses
  `-c 12` and an exact 48,000-MiB scheduler allocation per timed job, while
  preserving the 16-GiB solver process limit, one bound hardware thread, and
  node-local scratch. A decision-grade comparison consists of three independent paired
  waves: one separate candidate job and one separate baseline job run
  concurrently per wave, with placement crossed where practical. Report the
  median paired ratio/full spread; >2% per-arm spread rejects an absolute
  claim. This repetition applies only to performance evidence used to accept
  or reject a repair. Fuzzing remains exactly one Claude-owned campaign and is
  not repeated; correctness gates and ordinary probes retain their existing
  contracts. `-c 4` remains ordinary functional capacity but is rejected for
  long timing. Proof generation and checking remain separate.
- Allocation-only probe `1648628` verifies the final resource spelling:
  `SLURM_CPUS_PER_TASK=12`, `SLURM_MEM_PER_NODE=48000`, and mask
  `0-5,96-101`, exactly one six-core L3 domain. `48000M` permits 16 timing
  allocations within configured node memory 769,024 MiB; `48G` would round up
  and permit only 15. The probe wrapper's only nonzero step was an optional
  cgroup-v2 `memory.max` read on this cgroup-v1 cluster; no solver ran. Initial
  probe `1648627` wrote its log to compute-local scratch and is
  infrastructure-invalid. Neither probe is solver or fuzz evidence.
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
  **Correction / promotion reopened:** the `7c5a38…` predecessor used by the
  later Vivify controls is not build-equivalent to the 399.152-second witness
  binary `60fe8e…`. Binary strings and symbols prove `7c5a38…` contains
  compiled `TACTIX_STOP_AFTER_CONFLICT` / `TACTIX_SWEEP_TIME` diagnostics;
  neither `60fe8e…` nor clean candidate `067f7f…` contains them. A fresh
  Release-default build from an archive of commit `a4b11964` reproduces
  `60fe8e…` bit-for-bit (1,260,152 bytes), proving the committed production
  source exactly represents the 399-second binary and there was no hidden
  pre-commit solver-source change. Thus exact-state/proof/fuzz correctness and
  the Vivify-local speedups survive, but the B+C total comparison and all
  whole-Run controls against `7c5a38…` are build-confounded. The Andrew report
  above is a frozen historical artifact superseded on that promotion claim.
  Reclose only with a direct clean `60fe8e…` versus clean `067f7f…` Rank-27
  control under the same allocation; BVE may finish its already-running guard
  but must not advance to Rank 27 or promotion meanwhile.
  Corrected clean-control job `1647411` now runs those exact immutable binaries
  serially in one allocation on the same node: predecessor first, then
  candidate, two CPUs, 16 GiB, 1,200 s each, no compiled conflict-stop strings,
  and zero proof generation. Require exact non-time state and terminal UNSAT;
  the predecessor's absolute result tests current reproducibility of 399.152 s,
  while the same-allocation delta is the only admissible whole-run Vivify price.
  Serial clean result is exact-state positive: both arms' complete non-time
  images have SHA `14f7da2d…6194`; predecessor/candidate is
  553.948/551.564 s internal (-2.384, ratio 0.995696), Vivify
  68.196/60.155 s (-11.79%), and inprocessing 198.858/192.124 s (-6.734).
  Outside-inprocessing varies +4.351 s, RSS is effectively flat, and residue
  is zero. This removes the `7c5a38…` build confound and establishes a clean
  non-regressive whole-run Vivify delta under the shared node017 allocation.
  Exact `60fe8e…` predecessor has now returned exact-state UNSAT in 553.948 s
  internal / 554.47 s wall on node017, proving the identical executable's
  399→554 jump is not a source or build change. Candidate `067f7f…` continues
  serially in that allocation. On explicit user direction, standalone causal
  job `1647447` runs the same exact `60fe8e…` artifact on node018 under the
  same two-CPU/16 GiB/1,200 s contract. Its node constraint makes it
  non-scoring; require exact state and zero residue before comparing absolute
  time to both historical 399.152 s and contemporaneous node017 553.948 s.
  Node018 job `1647447` returns exact `60fe8e…` UNSAT in 394.107 s internal /
  394.59 s wall, recovering and slightly improving the historical behavior.
  The identical binary's contemporaneous node017/node018 spread is 159.841 s,
  conclusively assigning the absolute 399→554 jump to execution environment /
  load rather than a pre-`a4b11964` source regression. Cleanup/residue are
  zero. Immediate clean companion `067f7f…` job `1647466` is now running on
  node018 with the same user-directed causal contract; require exact non-time
  state and price its total/Vivify delta against `1647447` before disposition.
  Clean node018 companion `1647466` is exact-state UNSAT in 386.665 s internal
  / 387.17 s wall. Its complete normalized non-time image is byte-identical to
  `60fe8e…` at SHA `14f7da2d…6194`. Candidate/predecessor is
  386.665/394.107 s (-7.442, ratio 0.981117), Vivify 47.798/53.908 s
  (-11.33%), inprocessing 153.326/160.522 s (-7.196), and outside time is flat
  (-0.247); RSS differs by only +72 KiB and residue is zero. Thus the clean
  Vivify candidate both recovers the ~399-second behavior and improves it.
  Together with the exact serial node017 delta, proof/fuzz correctness, and
  full gate, this recloses Vivify promotion; the superseding correction must
  be reported and committed before any BVE Rank-27 work resumes.
  Because the earlier Vivify B+C comparator was the instrumented `7c5a38…`
  build, corrected clean-pin full-width B28+C5 array `1647483` and aggregate
  `1647536` are now running with exact `60fe8e…` predecessor and exact
  `067f7f…` candidate. All 66 tasks retain the frozen two-CPU/16 GiB/1,200 s
  contract and independent model/proof checks. Final correction remains open
  until this aggregate reports 66/66 validity, exact paired non-time images,
  no solve loss, and zero cleanup/certificate residue.
  Corrected aggregate `1647536` passes: 66/66 valid, each clean arm has 17
  independently checked SAT models and 16 independently checked UNSAT proofs,
  all 33 paired normalized non-time images are exact, and invalidity, solve
  loss, cleanup failure, and certificate residue are zero. Clean
  candidate/predecessor PAR-2 is 1542.691/1576.572 s (0.978510); summary SHA
  is `440ba892…3baa`. This fully supersedes the instrumented-predecessor gate
  and closes the last Vivify promotion requirement.
  Final BVE-free Release validation is 73/74: all executable, solver,
  proof/model, trace, cleanup, proof-rule/strategy-boundary, and solve-reset
  tests pass; only the already-recorded documentation-size policy fails on
  `PROGRESS.md` / implementation-notes line budgets. The superseding
  three-page Andrew report is
  `report-to-andrew/2026-08-25-0935-rank27-vivify-clean-build-correction/`,
  TeX SHA `189fab4b…cf20`, PDF SHA `361add15…ebf`, compiled twice without box
  warnings. It records the essential correction: the approximately 399-second
  behavior was recovered by running the exact unchanged `60fe8e…` executable
  on node018, not by a source repair; clean `067f7f…` then improves that result
  to 386.665 s with the exact accepted logical image.
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
  The frozen prototype has now been restored byte-for-byte on promoted commit
  `fbb2850` (BVE source SHA `49b72a…0b77`, diff SHA `63ea569e…def`) and again
  passes the focused Release proof/scheduling/event-trace/BVE/solve-reset set
  5/5. Full-length proofless T105 stage `1647289` runs the immutable promoted
  Vivify predecessor then candidate in one two-CPU/16 GiB allocation with a
  1,200-second limit per arm. Require identical terminal/non-time state and
  zero residue before interpreting BVE and total timing; no full guard or
  Rank-27 BVE job is authorized by submission alone.
  Terminal T105 result is decisively positive. Both arms return exact-state
  UNSAT, and removing timing lines gives byte-identical complete solver SHA
  `7f9530cc…bd34`, including 6,178,985 BVE applications and every exact
  tick/bound, effect, schedule, learning, pruning, and retained-state counter.
  BVE falls 108.667→52.943 s (-51.28%), inprocessing 286.971→228.413 s,
  internal total 426.944→364.015 s (-14.74%), wall 430.00→367.09 s, and RSS is
  flat/slightly lower. Staged input, scratch, and certificate residue are zero.
  This authorizes the predeclared frozen B28+C5 guard; Rank 27 and promotion
  remain blocked on that complete independently checked result.
  Full-width frozen B28+C5 array `1647325` plus after-any aggregate `1647380`
  are now running from immutable predecessor/candidate/checker pins. All 66
  tasks use two CPUs, 16 GiB, and 1,200 s with no node constraint or throttle;
  each SAT model and UNSAT proof is independently checked and deleted before
  atomic result publication. Do not interpret partial timing or launch Rank 27
  until the aggregate reports every result, invalidity, solve loss, cleanup,
  and certificate-residue classification.
  Aggregate `1647380` is now complete-passed: 66/66 valid, each arm has 17
  independently checked SAT models and 16 independently checked UNSAT proofs,
  all 33 normalized non-time pairs are exact, and invalidity, solve loss,
  cleanup failure, and certificate residue are zero. Candidate/predecessor
  PAR-2 is 1514.815/1539.885 s (0.983720); summary SHA is
  `338b66b1…49a`. This closes the BVE guard but is deliberately parked: no BVE
  Rank-27 run or promotion occurs until corrected clean Vivify control
  `1647411` resolves the higher-priority 399-second behavior.
  The clean Vivify correction is now closed and pushed at `4777da6`, so the
  frozen flat-slice source has been restored byte-for-byte and rebuilt to the
  already-guarded candidate SHA `aeb14e2a…7f62`; focused Release validation is
  again 5/5. Immutable proofless Rank-27 serial control `1647569` now runs
  promoted-Vivify predecessor `067f7f3a…70bd` then BVE candidate
  `aeb14e2a…7f62` in one allocation at two CPUs, 16 GiB, and 1,200 s per arm.
  Require terminal exact-state UNSAT, byte-identical complete non-time images,
  material BVE-owned savings, no total/RSS regression, and zero residue before
  pricing. Even a positive control does not promote: Claude Code exclusively
  owns the required exact-source fuzz, followed by an independently checked
  Rank-27 proof witness.
  Rank-27 control `1647569` is decisively positive and exact. Both serial arms
  return UNSAT and their complete normalized non-time images are byte-identical
  at SHA `14f7da2d…6194`, including 346,693 eliminations, 64,242,959
  resolutions, every BVE gate/definition/tick/bound/effect counter, all rule
  scheduling, search, learning, pruning, and retained state. Candidate versus
  predecessor is 419.772/498.082 s internal (-78.310, ratio 0.842777), BVE
  16.229/37.975 s (-57.26%), and inprocessing 150.772/182.996 s. RSS rises a
  bounded 5,564 KiB (0.88%) from reusable retained capacity; cleanup and
  certificate residue are zero. Serial outside-rule timing is not causally
  assigned, but the exact same-work BVE owner now reaches the approximately
  16.32-second current-instrumented-Kissat level that motivated the repair.
  This authorizes an independently checked Rank-27 proof witness. Promotion
  remains blocked only on that witness and Claude Code's authoritative fuzz
  terminal report; Codex does not inspect fuzz job `1647571` or its artifacts.
  Immutable proof-producing witness `1647572` is now submitted with candidate
  SHA `aeb14e2a…7f62`, pinned `drat-trim`, the exact Rank-27 input, two CPUs,
  16 GiB, a 1,200-second solver limit, and a 4,800-second checker limit. It must
  verify the complete UNSAT proof against the original decompressed CNF and
  atomically publish zero proof/input/checker/scratch residue; submission alone
  carries no correctness or promotion claim.
  On explicit user direction, standalone causal job `1647573` now runs exact
  BVE candidate `aeb14e2a…7f62` on node018 at two CPUs, 16 GiB, and 1,200 s.
  It is node-constrained and non-scoring. Require the accepted exact non-time
  SHA and zero residue, then compare directly with same-node promoted-Vivify
  `1647466` at 386.665 s / 30.069 s BVE and historical clean `1647447` at
  394.107 s / 30.719 s BVE; also report the identical candidate's node017
  `1647569` result without assigning cross-node causality to the source.
  Checked Rank-27 witness `1647572` passes. Candidate returns exact-trajectory
  UNSAT in 573.973 s internal / 574.542 s wall in proof mode; pinned
  `drat-trim` verifies the 1,514,395,111-byte proof in 146.366 s with
  `s VERIFIED`. Proof SHA `0f99f55a…8bb2d` and complete normalized proof-mode
  non-time SHA `8cd3eda4…c2da7` are byte-identical to the accepted promoted-
  Vivify witness. Result SHA is `44cd9580…c4d`; all proof, decompressed input,
  checker output, and scratch are deleted with zero residue. Full Release is
  73/74 with every executable/correctness/proof/boundary/cleanup test green and
  only the known documentation-size policy debt failing. BVE promotion now
  awaits only Claude Code's authoritative terminal fuzz report.
  User-directed node018 BVE arm `1647573` is exact-state UNSAT in 371.711 s
  internal / 372.18 s wall, the best clean result on the current accepted
  Rank-27 trajectory. Its complete non-time SHA is the accepted
  `14f7da2d…6194`. Versus same-node promoted-Vivify `1647466`, total improves
  386.665→371.711 s (-14.954, ratio 0.961326), BVE 30.069→15.470 s (-48.55%),
  inprocessing 153.326→139.130 s, and outside time is flat
  (233.339→232.581 s); RSS rises only 4,784 KiB (0.76%). Versus historical
  clean `60fe8e…` job `1647447`, total improves 394.107→371.711 s (-22.396).
  The identical candidate's node017/node018 spread (419.772 vs 371.711 s) is
  environmental and non-scoring. The BVE repair accounts for essentially the
  full same-node gain, but Rank 27 remains 71.711 s above the 300-second
  milestone. Cleanup and certificate residue are zero.
  User-directed exact-binary node018 repeat `1647575` confirms that result:
  the candidate again returns the accepted complete non-time SHA
  `14f7da2d…6194` in 375.797 s internal / 376.27 s wall with BVE 15.500 s.
  It is 4.086 s (1.10%) slower than the best BVE observation `1647573`, but
  BVE differs by only 0.030 s and 3.416 s of the variation is outside
  inprocessing. Against the same-node promoted-Vivify baseline `1647466`, the
  repeat is still 10.868 s faster overall and 14.569 s faster in BVE; against
  historical clean `1647447`, it is 18.310 s faster. Thus `1647573` remains
  the best observed clean Rank-27 result at 371.711 s, while the repeat
  independently confirms the causal BVE throughput gain. Staged input,
  certificate, checker, scratch, and temporary residue are zero.
  At the user's request, a third exact-pin node018 control is submitted as job
  `1647654` from
  `benchmarks/runs/20260825-1147-tierd-rank27-bve-node018-third/`. It uses the
  byte-identical BVE Release pin `aeb14e2a...7f62`, immutable Rank-27 input,
  two CPUs, 16 GiB, and the 1,200-second solver contract. Compare only after
  accepted non-time SHA `14f7da2d...6194`, UNSAT exit 20, and zero residue;
  the direct prior observations are 371.711 s (`1647573`) and 375.797 s
  (`1647575`). This node-constrained repeat is causal evidence, not frozen-tier
  scoring.
  Job `1647654` completes exact-state UNSAT in 372.969 s internal / 373.45 s
  wall with BVE 15.472 s, inprocessing 139.291 s, outside-rule 233.678 s, and
  637,044 KiB peak RSS. Its complete normalized non-time SHA is the accepted
  `14f7da2d...6194`; cleanup and certificate residue are zero. It is 1.258 s
  (0.34%) slower than best job `1647573`, but BVE differs by only 0.002 s and
  1.097 s of the whole-run delta lies outside inprocessing. It is 2.828 s
  faster than repeat `1647575`. Across the three node018 BVE observations,
  median total is 372.969 s, range is 4.086 s, mean BVE is 15.481 s, and the
  BVE range is only 0.030 s. Thus `1647573` remains the single best at
  371.711 s, while three independent runs confirm the BVE-owned throughput
  reduction versus promoted Vivify's 30.069 s BVE owner.
  Wrap-up preparation is durable: the flat-slice lifetime/order invariant is
  recorded in `docs/implementation-notes.md`; the final three-page Andrew
  TeX/PDF is `report-to-andrew/2026-08-25-1215-rank27-bve-flat-slice/` (TeX
  SHA `b8e0c489...19a21`, PDF SHA `11d397ba...b25f`); and a
  fresh clean Release run is 73/74 with every executable/correctness/proof/
  model/trace/boundary/cleanup test green and only the pre-existing opt-in
  documentation-size policy failing. Claude Code retry at 11:18 EDT reached
  its account session limit without inspecting the campaign and reports reset
  at noon. Codex did not inspect fuzz logs or artifacts. Claude Code's
  authoritative terminal report now classifies exact-source ASan/UBSan native-
  LRAT fuzz job `1647571` PASS. Corpus is 27/27 and random fuzz is
  10,000/10,000 with zero failures: 5,097 independently checked SAT models,
  3,463 independently accepted UNSAT native-LRAT proofs, 1,440 bounded five-
  second nonterminations carrying no oracle claim, and zero model/proof
  rejection or Kissat differential disagreement. Required reach counts are
  BVE 5,959, BveEliminated 5,937, and Propagate 5,940, with 1,377,525 target
  applications. Claude reports zero sanitizer finding, crash, abort, invariant
  failure, certificate residue, or scratch residue. The 25 retained diagnostic
  timeout CNFs and seed ledger are inputs only, not failures or certificates.
  This closes the BVE fuzz gate; do not rerun or independently reinterpret the
  campaign. Full-profile follow-up
  exact-prefix array `1647576` prices an isolated all-binary watch-list fast
  path using the existing exact-negative `WatchHead.has_large` certificate.
  Every conflict-9450 counter is byte-identical (normalized SHA
  `3b87bb36…c5f`), including 5,795,248 decisions, 96,338,812 propagations, all
  rule ticks/effects, ordering, scheduling, learning, and pruning. Candidate /
  predecessor sampled cycles are 71.048/71.367 billion (-0.447%), but the
  intended ordinary-propagation owner is only 22.309/22.348 billion (-0.175%).
  This is below a material portable gain, so the isolated prototype is rejected
  as neutral and never enters main source, tracing, or B/C gates. It also
  demonstrates that the existing all-binary certificate does not cover enough
  hot work to explain the remaining propagation gap; do not reopen it without
  contradictory hardware evidence. Both staged CNFs and certificate residue
  are zero.
  Source comparison then retests Kissat's bulk propagation-statistics update
  against Tactix's source-level per-implication increment. Repository audit
  after completion found this was already closed by frozen job `1645516`, so
  array `1647578` is an unnecessary independent reconfirmation and must not be
  repeated. It again preserves normalized SHA `3b87bb36…c5f`, but candidate /
  predecessor total cycles are 71.554/71.368 billion (+0.261%) and ordinary
  propagation is 22.545/22.478 billion (+0.298%). The compiler therefore
  already removes any material machine-level shared-counter penalty from the
  predecessor. This isolated arm is rejected negative/neutral and never enters
  main source, tracing, or gates; do not reopen it without contradictory
  disassembly or hardware evidence. Cleanup and certificate residue are zero.
  A frozen-profile normalization audit then corrects the asserted remaining
  propagation gap without rerunning either solver. Tactix's
  `propagate_literal<false,false>` owner includes both search and probing, but
  the published 174.239 Kissat cycles/propagation divided only
  `kissat_search_propagate`'s 464.511B cycles by all 2.666B propagations,
  including 730.505M probing propagations. The owner-matched Kissat numerator
  is search 464.511B + probing 57.247B + warming 0.757B = 522.515B, or
  195.998 cycles/propagation. Accepted post-reserved-trail Tactix is 195.780,
  not 1.12362x worse. That later comparison is cross-run and function
  boundaries still differ, so it is localization-only rather than a claim
  that Tactix is faster; critically, the old 1.12362x worse-side claim is
  invalid and superseded. Current evidence establishes no residual portable
  propagation gap after reserved-trail removal. Resume source-backed residual
  Vivify/common-state localization instead of mining the environmentally
  inflated job-`1647574` total. Audit manifest is
  `20260825-1050-tierd-rank27-propagation-normalization-audit`.
  Residual Vivify is likewise not a demonstrated same-work loss: after the
  accepted arena-order repair, exact common-state job `1646181` spends 1.865 s
  in Vivify through conflict 9450, below current Kissat's frozen conservative
  1.96-second comparator through conflict 10000. The later 47.8/26.2-second
  full timers belong to different trajectories and cannot authorize another
  Vivify behavior change. The next source-backed broad-search candidate instead
  hoists invariant stable/focused mode dispatch out of the backtrack trail
  loop, matching Kissat's split loops. Exact-prefix array `1647580` preserves
  normalized SHA `3b87bb36…c5f` and lowers the intended backtrack owner
  4.064B→3.860B sampled cycles (-5.00%), but complete cycles regress
  71.526B→71.884B (+0.50%) and elapsed rises 79 ms. It therefore fails the
  predeclared whole-prefix acceptance contract and is rejected without main
  source, trace, or B/C promotion. Cleanup and certificate residue are zero.
  The next portable candidate uses DefaultPolicy's existing active-variable
  guard to call Kissat-shaped queue/heap pickers without the generic N1
  end-of-structure fallback; the policy-neutral total picker remains unchanged.
  Exact-prefix array `1647582` preserves normalized SHA `3b87bb36…c5f` while
  combined next-unassigned owners fall 1.458B→1.097B cycles (-24.73%), focused
  queue selection falls 1.306B→0.946B (-27.59%), complete cycles fall
  71.737B→71.420B (-0.442%), and elapsed falls 91 ms. This is a positive exact
  stage only; candidate-only event tracing against the frozen common-state
  trace is next, with no main-source or promotion claim yet. Cleanup and
  certificate residue are zero.
  Candidate-only trace job `1647584` produces exactly 64 untruncated rows with
  SHA `7a6216bb…7cc1`, byte-identical to the frozen common-state authority.
  Decision/rule/candidate order, exact tick/cost, scheduling, learning,
  pruning, and resulting state are therefore exact. Advance the isolated pin
  to a full Rank-27 node018 control against exact BVE baselines `1647573` and
  `1647575`; full B28+C5 remains blocked until the longer result is positive.
  Full node018 job `1647585` is now exact-state UNSAT with accepted complete
  non-time SHA `14f7da2d…6194`, zero staged-input/certificate residue, and
  373.696 s internal / 374.18 s wall. Against the immediately preceding exact
  BVE repeat `1647575`, total falls 375.797→373.696 s (-2.101, ratio 0.994410)
  entirely outside inprocessing (235.997→233.889 s); BVE and inprocessing are
  flat at 15.500→15.522 and 139.800→139.807 s. It is still 1.985 s slower than
  the single best `1647573` observation, so one noisy whole-run result is not
  a promotion claim. Together with the exact prefix owner reduction and exact
  trace, it is sufficient to authorize the predeclared full frozen B28+C5
  guard `20260825-1110-BC-active-decision-guard`, comparing exact BVE pin
  `aeb14e2a…7f62` with candidate `94784f16…8503` under two CPUs, 16 GiB, and
  1,200 s plus independent SAT-model/UNSAT-proof validation. Promotion remains
  blocked on that complete guard and independent fuzz authority. Full-width
  array `1647586` and after-any aggregate `1647652` are now submitted; neither
  submission nor partial task state carries a validation claim. The aggregate
  is now complete and passed: 66/66 valid results, with each arm independently
  checking 17 SAT models and 16 UNSAT proofs, no invalidity or solve loss,
  paired normalized non-time images exact on all 33 instances, and zero
  cleanup/certificate residue. Candidate/predecessor PAR-2 is
  1592.114/1599.173 s (ratio 0.995586), with equal 2.917 GiB maximum RSS.
  This closes the mandatory B28+C5 safety/performance guard. Active-decision
  source promotion remains deliberately ordered after the staged BVE wrap-up
  and requires Claude-owned fuzz authority; no second behavioral change is
  opened meanwhile.
  BVE is now closed and pushed at `a07bc93`. The isolated active-decision files
  are applied byte-for-byte to main over that clean baseline; rebuilt Release
  SHA is the already guarded `94784f16...8503`, and focused Release policy/N1/
  trace/reset validation passes 8/8. Claude Code now exclusively owns a new
  exact-source active-decision fuzz campaign from preparation through terminal
  PASS/FAIL; Codex will not inspect its fuzz logs or artifacts. Independent
  Rank-27 proof witness `1647655` is separately submitted with that exact pin,
  immutable input SHA `5435b1b2...bb4e9`, two CPUs, 16 GiB, 1,200-second solver
  and 4,800-second checker limits, and pinned `drat-trim`. Require UNSAT, a
  verified complete proof, accepted proof-mode non-time image, and atomic zero
  proof/input/checker/scratch residue before promotion. No collection,
  Substitute, or Congruence source work begins before this increment is
  reported, committed, and pushed.
  Proof witness `1647655` passes: exact candidate returns UNSAT in 522.462 s
  internal / 522.994 s wall with 624,272 KiB RSS; pinned `drat-trim` verifies
  the 1,514,395,111-byte proof in 128.730 s with `s VERIFIED`. Proof SHA
  `0f99f55a...8bb2d` and complete proof-mode normalized non-time SHA
  `8cd3eda4...c2da7` are byte-identical to the accepted BVE witness, preserving
  all proof effects and retained state. Proof, decompressed input, checker
  output, and scratch are deleted with zero residue. Active-decision promotion
  now awaits only Claude's terminal fuzz report and final report/wrap-up. A
  fresh full Release suite is 73/74: every functional, proof/model, trace, N1,
  proof-rule/strategy-boundary, cleanup, and reset test passes; only the known
  opt-in documentation-size policy debt fails.
  Claude Code's authoritative terminal report classifies exact-source ASan/
  UBSan native-LRAT fuzz job `1647657` PASS. Corpus replay is 27/27 with zero
  failures and zero semantic nonterminations. Random fuzz is 10,000/10,000
  with zero failures: 5,667 independently checked SAT models, 3,906 native-
  LRAT proofs accepted by pinned `lrat-check`, and zero Kissat differential
  disagreement across all 9,573 verdict cases. The remaining 427 cases hit the
  declared five-second quick bound and carry no oracle claim; their seed ledger
  and 25 capped diagnostic CNF bundles are inputs only, not failures or
  certificates. Required reach counts are Decide 1,280, SwitchMode 395, and
  Propagate 7,280, exercising both focused and stable active pickers. Claude
  reports zero sanitizer finding, crash, invariant assertion, model/proof
  rejection, certificate residue, or scratch residue. This closes the active-
  decision fuzz gate; do not rerun or independently reinterpret the campaign.
  Final three-page Andrew handoff is
  `report-to-andrew/2026-08-25-1300-rank27-active-decision/`; TeX SHA
  `b464f052...7d90`, PDF SHA `cb6a7961...3845`, with no final-pass LaTeX box
  warning. The isolated source diff over pushed BVE base is
  `a5662046...4ad0`, and the validated cumulative Release binary remains
  `94784f16...8503`.
  Active-decision is now committed and pushed separately at `508342d`, with
  main clean and synchronized. The next collection/rewatching increment freezes
  that accepted Release (`94784f16...8503`) and current instrumented Kissat
  comparator commit `8af8e56` / binary `b6a9b1d0...644d5` in
  `benchmarks/runs/20260825-1315-rank27-collection-rewatch-baseline/`. The
  immutable Rank-27 input, exact Tactix conflict-9450 image
  `3b87bb36...92c5f`, and 64-row trace `7a6216bb...7cc1` remain the common-state
  authorities; Kissat's conflict-10000 result is a conservative localization
  envelope, not a scoring reference. Before any fusion prototype, trace arena
  scanning/copying, watch compaction, metadata reconstruction, large-reference
  forwarding, sparse collection, rewatching, and dense reconstruction,
  normalized by calls, literal lists, entries, words, clauses, and literals
  processed. No Substitute or residual Congruence source work is mixed in.
  Observer-only conflict-9450 collection population job `1647658` from
  `20260825-1320-rank27-collection-substage-trace` completes 0:0 on node017 in
  19 seconds with two CPUs. Removing timing and the observer-only row gives the
  accepted exact common-prefix SHA `3b87bb36...92c5f`; perf has 3,636 samples,
  about 67.990B cycles and zero lost samples, and staged-input/certificate
  residue is zero. Across 36 collection calls, 31,537,716 literal lists carry
  55,263,788 binary plus 42,098,107 large entries. Stable compaction retains
  every binary and 5,759,788 large entries while dropping 36,338,319 large
  entries; the separate forwarding pass then revisits all 5,759,788 survivors.
  Its isolated compact and forward owners cost 1.724B and 0.367B cycles. Arena
  collection scans 42,102,056 records / 344,622,498 words and retains
  20,906,868 / 170,998,079 semantic words; rewatching covers 18,026,974 clauses
  / 57,342,390 literals and dense reconstruction emits 56,958,251 occurrences.
  This exact observation authorizes corresponding current-Kissat population
  tracing and a bounded simultaneous fused traversal prototype, not promotion.
  Current-Kissat observer job `1647659` is submitted from the clean detached
  `8af8e56` source through the conservative conflict-10000 envelope with two
  CPUs, 16 GiB, and a 300-second bound. Its isolated metrics-only pin
  `345a6630...1acc` counts sparse collection calls, watch-list input/survivors,
  arena clauses/words/literals, and rewatch clauses/literals. It must preserve
  all pre-existing job-`1647653` semantic metrics after removing observer rows,
  have zero lost samples and residue, and remains non-scoring because its 550
  extra conflicts are not the exact Tactix common-state endpoint.
  Job `1647659` completes 0:0 on node017 in 16 seconds at exactly 10,000
  conflicts. All 270 integer metrics common to clean current Kissat and frozen
  job `1647653` match exactly; the older metrics overlay alone prints an
  unrelated zero `definition_ticks` row. perf has 2,890 samples / about
  54.006B cycles and zero lost samples, and residue is zero. Its seven sparse
  collections visit 6,251,150 literal lists with 10,818,424 binary + 8,244,150
  large entries, retaining 10,818,314 + 5,759,788; they scan 1,311,434 tail
  clauses / 2,687,531 wards / 4,301,466 literals and rewatch 1,241,263 clauses
  / 3,992,216 literals. Entries per call are nearly identical to Tactix
  (2,704,497 versus 2,723,225, ratio 0.9931), but Tactix calls collection 36
  versus 7 and scans 5.1075x as many aggregate watch entries. Both retain
  exactly 5,759,788 large entries in total. Source/application accounting
  decomposes Tactix's 36 as seven sparse-root, sixteen Vivify, nine Substitute,
  three Congruence, and one whole compact call; Kissat defers/reuses collection
  across those rule boundaries. Tactix also performs a live-size prepass and a
  second whole-arena scan/copy even for sparse-tail collection, whereas Kissat
  preserves the prefix and sweeps only the tail. Invocation frequency and the
  whole-arena sparse path are therefore larger demonstrated collection owners
  than the isolated second forwarding traversal. Keep the predeclared bounded
  fused arm exact and isolated, but do not mistake it for collection closure.
  Bounded fused compact-and-forward prefix stage `1647660` is submitted from
  `20260825-1322-rank27-collection-fused-stage` as a sequential `0-1%1` array,
  with two CPUs and 16 GiB per arm. The predecessor is exact observer pin
  `0a55e600...e412`; candidate `056adb35...7405` maps each retained large Ref
  while stable compaction copies the two-word entry. A focused mixed-entry unit
  image and six boundary/scheduling/reset tests pass 7/7. Require both arms'
  exact accepted conflict-9450 image, identical collection populations, zero
  lost samples/residue, and a material compact-plus-forward and complete-prefix
  reduction. This isolated source is not in main and does not alter the larger
  eager-call/whole-arena findings.
  Both sequential arms `1647660_0/1` complete exact on node017. Their complete
  normalized images equal accepted SHA `3b87bb36...92c5f`, collection population
  rows are byte-identical, both perf profiles have zero lost samples, and
  residue is zero. Predecessor compact+forward costs 1.8025B+0.3343B=2.1368B
  cycles; fused candidate costs 2.1341B, only -0.1245%, while complete prefix
  cycles regress 67.405B→67.691B (+0.4236%) and solve time 17.551→17.657 s.
  Reject this bounded fusion as neutral/noisy without touching main or invoking
  Claude fuzz. The second pass sees only 5,759,788 survivors versus 97,361,895
  entries handled by compaction; eager call frequency and the whole-arena
  sparse path remain the first material demonstrated collection owners.
  A follow-on isolated arena-scan hypothesis combined the live-size prepass
  with the existing relocation-order traversal, but local Release
  `solve_reset` then segfaulted once after six other focused tests passed.
  Main source was never changed. Per §8.2 the candidate was immediately
  quarantined and handed to Claude Code with edit permission and a bounded
  reproduce/repair/test scope. Three complete Claude invocations ended without
  a verdict at the same external API/DNS `ENOTFOUND`; Codex did not inspect or
  modify Claude's intermediate workspace. Reject and quarantine this candidate
  rather than bypass the mandatory crash workflow. Collection remains open on
  the documented eager-call/whole-arena owners; safe read-only localization of
  the next ordered gap may continue while external Claude connectivity is down.
  Substitute direct-localization stage `1647662` completes both sequential
  two-CPU arms 0:0 on node017 with zero lost samples and zero residue. Tactix
  preserves the exact accepted conflict-9450 image `3b87bb36...92c5f` and
  takes 1,766 ms across 13 canonical SCC rounds in nine applications: SCC 601,
  mark 3, and rewrite 1,159 ms. Current Kissat completes ten rounds in nine
  calls: SCC 570.808 ms, non-SCC round work 237.056 ms, rewatch 122.520 ms,
  and total 931.761 ms. Its completed Substitute work is authoritative even
  though the built-in limiter performs one later stable propagation beyond the
  frozen endpoint; all other 129 common metrics and the formula endpoint are
  exact. Exact Kissat work counts are frozen in the manifest. SCC traversal is
  not the owner. Tactix's rewrite/maintenance path explains about 798 ms of the
  834 ms gap: its 250 rewrite/session rows total snapshot 221, binary 265,
  large 190, post-large 34, and collection 419 ms. The first bounded portable
  candidate is a canonical-call import-order image: whole-prefix perf assigns
  792.9M cycles (1.18%) to repeated `kernel_import_lit_order` indirections, and
  source shows repeated calls while snapshotting/rebuilding binary images.
  Preserve Sweep/session behavior and all ordering exactly; test this isolated
  representation repair before attempting a larger streaming rewrite.
  Exact-prefix import-order-image stage `1647664` is submitted sequentially
  from `20260825-1350-rank27-substitute-import-order-stage`. Frozen predecessor
  `6f7e44e...902d` and candidate `b926d0dc...2861` use identical Release
  conflict-stop/observer configuration, two CPUs, 16 GiB, and conflict 9450.
  The candidate resolves exact permanent import rank once per current variable
  for canonical binary snapshot/publication only; sparse Sweep sessions remain
  unchanged. Focused proof/rule/schedule/reset tests pass 12/12 in both Release
  configurations. Require exact normalized state/work, zero lost samples and
  residue, material Substitute/import-order improvement, and no whole-prefix
  regression before any trace or promotion.
  Both sequential `1647664` arms complete 0:0 on node017 with exact accepted
  normalized SHA `3b87bb36...92c5f`, identical round/image/tick/effect
  populations, zero lost samples, and zero residue. The intended owner improves:
  snapshot 201→148 ms (-26.37%), `kernel_import_lit_order` 528.8M→263.9M
  sampled cycles (-50.10%), and canonical Substitute 1,610→1,574 ms (-2.24%).
  The short complete prefix drifts the other way, 62.447B→63.064B cycles
  (+0.99%) and 16.257→16.427 s; approximately 0.48B of the increase is in
  unchanged propagation. This is neither promotion nor rejection under the
  user-required longer-horizon contract. Require byte-identical event trace,
  then price the frozen pair over full Rank 27 and predeclared mechanism-
  relevant B+C instances. Only a longer positive result may advance to the
  complete checked B28+C5 gate.
  Candidate-only event-trace job `1647666` is submitted from
  `20260825-1355-rank27-substitute-import-order-trace` with two CPUs and the
  established conflict-9449/stop-9450 observer contract. It compares exactly
  64 rows against frozen SHA `7a6216bb...7cc1` without rerunning the accepted
  predecessor. Event-trace Release tests pass 4/4; longer pricing remains
  blocked until the trace is byte-identical and residue-free.
  Job `1647666` completes 0:0 on node017 with exactly 64 untruncated rows and
  SHA `7a6216bb...7cc1`, byte-identical to the frozen authority. Exact rule/
  candidate order, scheduling, tick/cost, learning, pruning, and resulting
  state are preserved; staged-input and certificate residue are zero. This
  authorizes only the predeclared longer Rank-27 plus mechanism-relevant B+C
  pricing stage, not the full gate or promotion.
  Longer evaluation is now frozen and submitted. Same-allocation full Rank-27
  job `1647667` runs accepted Release `94784f16...8503` then candidate
  `bafb4522...639a` sequentially on one node with two CPUs, 16 GiB, and 1,200 s
  per arm. Checked B+C mechanism screen array `1647668` plus after-any aggregate
  `1647675` pairs four Tier B (FmlaEquivChain, PRP, DivS, b22) and two Tier C
  (500-gate circuit, T105) selected before submission from accepted guard
  `1647586` as the highest Substitute time/share cases. All 12 tasks use two
  CPUs, 16 GiB, 1,200 s, independent SAT-model/UNSAT-proof validation, atomic
  cleanup, and zero-retention policy. Neither submission nor partial state is
  a result; complete B28+C5, Claude fuzz, report, or source promotion remains
  unauthorized pending both terminal classifications.
  Initial full-pair allocation `1647667` was canceled by the submitter after 43
  seconds, before either solver emitted output, because it began on node017
  concurrently with the six-wide mechanism screen and would expose the first
  arm to a transient load confound. EXIT removed its staged CNF; only three
  preserved zero-byte placeholders and its scheduler log remain, with no
  verdict/certificate/score authority. The unchanged pair is resubmitted as
  job `1647680` with `afterok:1647675`, so it starts only after the checked B+C
  aggregate succeeds. Do not use `1647667` for any comparison.
  Mechanism screen `1647668`/`1647675` passes correctness and cleanup 12/12:
  each arm independently validates three SAT models and three UNSAT proofs,
  every paired non-time statistic image is byte-identical, and solve loss/
  invalidity/residue are zero. Candidate/predecessor PAR-2 is 691.931/734.113
  (0.942540) and summed Substitute is 48.731/51.794 s. Performance attribution
  remains open: T105 alone contributes 41.330 of the 42.182-second wall gain
  and 3.021 of the 3.063-second Substitute gain, while all its major unrelated
  rules scale faster too; the other five instances sum to Substitute
  22.292→22.250 s (-0.19%). Treat the screen as exact, correctness-clean longer
  coverage with environmentally dominated positive timing, not a causal win.
  Dependency-released same-allocation Rank-27 job `1647680` is now running on
  node017; use it as the causal long control before deciding whether one
  sequential T105 pair is required.
  Same-allocation Rank-27 pair `1647680` completes both arms UNSAT exit 20 on
  node017 with exact non-time SHA `14f7da2d...6194` and zero residue. The local
  vector candidate lowers Substitute only 23.711→23.554 s (-0.157, -0.66%) but
  regresses complete internal time 453.199→461.583 s (+8.384, +1.85%):
  inprocessing rises 0.444 s, unchanged search/scaffolding rises 7.940 s, and
  peak RSS rises 637,064→665,952 KiB. Reject this per-call allocation/fill
  implementation; no full gate or fuzz is authorized. The demonstrated helper
  owner remains open to one distinct general representation repair: maintain
  current-internal first-import rank in Kernel across growth/record/compaction,
  making `kernel_import_var_order` a direct read globally with no Substitute
  allocation. Require fresh exact prefix/trace and longer controls before any
  promotion.
  The distinct general candidate is frozen in
  `20260825-1430-rank27-import-rank-inverse-stage`: external rank remains the
  permanent proof/reconstruction authority, while `internal_import_rank` is
  initialized at growth, set on first import, and remapped at compaction.
  `kernel_import_var_order` becomes one direct current-identity read for
  imported variables and retains the exact unrecorded stable-external fallback.
  Release and conflict-stop builds pass 14/14 focused tests each, including a
  new pre/post-compaction rank-image assertion. Sequential exact-prefix job
  `1647683` compares accepted `6f7e44e...902d` with candidate
  `b8829dc7...1e03`, two CPUs, 16 GiB, conflict 9450, identical Substitute
  timers, and perf. Require exact state/work, zero residue, material helper/
  Substitute improvement, no whole-prefix regression, and no large RSS rise.
  Both sequential `1647683` arms complete 0:0 on node017 with exact accepted
  normalized SHA `3b87bb36...92c5f`, identical 13 canonical rounds, 250 total
  rewrite/session rows, 335,924 images, ticks/effects/schedules, zero lost
  samples, and zero residue. The intended helper falls 448.8M→302.8M sampled
  cycles (-32.53%), snapshot 199→186 ms, and Substitute 1,623→1,599 ms. Unlike
  the rejected local vector, complete cycles also fall 63.189B→62.206B
  (-1.55%) and elapsed 16.456→16.208 s. This is a positive exact stage only;
  candidate-only comparison against frozen 64-row trace `7a6216bb...7cc1` is
  next, with no long-run/full-gate/fuzz/promotion claim yet.
  Candidate-only event-trace job `1647685` is submitted from
  `20260825-1433-rank27-import-rank-inverse-trace`, two CPUs, 16 GiB, using the
  established conflict-9449/stop-9450 contract. It must produce exactly 64
  rows at frozen SHA `7a6216bb...7cc1` without rerunning the accepted trace;
  event-trace Release tests pass 5/5. Longer evaluation remains blocked until
  exact trace and zero residue are terminal.
  Job `1647685` completes 0:0 on node017 with exactly 64 untruncated rows at
  SHA `7a6216bb...7cc1`, byte-identical to the frozen authority. Exact rule/
  candidate order, scheduling, tick/cost, learning, pruning, and state are
  preserved; residue is zero. Advance only to the same frozen six-instance
  checked B+C mechanism screen and a dependency-serialized same-allocation
  full Rank-27 pair, not the complete gate or fuzz.
  The same predeclared six-instance checked B+C screen is submitted for the
  general inverse as array `1647686` plus after-any aggregate `1647693`: 12
  two-CPU tasks, 16 GiB/1,200 s, independent model/proof validation and atomic
  zero-retention cleanup. Same-allocation full Rank-27 pair `1647694` is held
  on `afterok:1647693`; it runs accepted Release `94784f16...8503` then exact
  candidate `c7c46bfa...57d4` on one node with two CPUs and 1,200 s per arm.
  Neither submission nor partial state is a result; full B28+C5/fuzz/report/
  promotion remain unauthorized pending both terminal classifications.
  Screen `1647686`/`1647693` completes correctness-clean 12/12: each arm has
  three checked SAT models and three checked UNSAT proofs, every paired
  non-time statistic image is exact, and invalidity/solve-loss/cleanup/residue
  are zero. Timing is environmentally inconclusive: candidate/predecessor
  PAR-2 is 725.029/716.779 (1.011510) and summed Substitute 51.306/49.483 s,
  but DivS swings 33.496 s toward candidate while T105 swings 38.637 s toward
  predecessor, with unrelated rule times scaling in the same directions. The
  other four instances sum to only +73 ms Substitute. Peak RSS is
  2.9305/2.9166 GiB, consistent with the four-byte per-current-var inverse.
  Treat this as exact high-exposure correctness coverage, neither performance
  win nor candidate-specific regression. Dependency-released same-allocation
  Rank-27 job `1647694` is now running on node017 as the causal long control.
  Same-allocation `1647694` completes both arms UNSAT exit 20 on node017 with
  exact non-time SHA `14f7da2d...6194` and zero residue. Candidate improves
  internal/wall 458.387/458.89→449.707/450.21 s (-8.680, -1.89%): inprocessing
  155.770→154.830 s, outside-inprocessing 302.617→294.877 s, and Substitute
  23.576→23.504 s. Rank-27 RSS rises 637,064→688,080 KiB, anomalously above
  the designed 1.7 MiB image; the larger six-instance screen's measured maximum
  rises only 14.6 MiB, exactly the four-byte-per-current-var cost, and both are
  far below 16 GiB. The exact prefix/helper win, byte-identical trace,
  correctness-clean B+C screen, and positive causal long control authorize the
  complete checked B28+C5 gate plus Claude-owned exact-source fuzz. No source
  promotion or checked proof witness is claimed yet.
  Complete full-width B28+C5 array `1647702` and aggregate `1647768` are now
  terminal PASS under the frozen 2-CPU/16-GiB/1200-s contract: 66/66 valid,
  with 17 independently checked SAT models and 16 independently checked UNSAT
  proofs per arm, no missing/malformed/invalid result, no solve loss, and zero
  cleanup failure or certificate residue. Candidate/shipped PAR-2 is
  1550.813/1537.768 s = 1.008483, inside the predeclared 1.10 guard; peak RSS
  is 2.9317/2.9166 GiB. This closes the mandatory complete B+C guard for source
  diff `681d17c9...ee1b6`. Claude Code is re-invoked after its earlier quota-only
  exit to own the still-required exact-source ASan/UBSan native-LRAT fuzz from
  preparation through terminal verdict; no fuzz PASS/FAIL was produced by the
  quota exit, and Codex does not inspect fuzz artifacts.
  Independently proof-producing immutable Rank-27 witness `1647775` is also
  submitted from the same candidate pin with exactly 2 CPUs, 16 GiB, 1200 s
  solver / 4800 s checker limits, pinned `drat-trim`, and atomic certificate
  cleanup. It must return checked UNSAT with the accepted normalized proof-mode
  state and zero residue; submission is not yet a result and does not substitute
  for Claude's fuzz verdict.
  A fresh exact-source full Release run is 73/74: every functional, solver,
  proof/model, trace, N1, rule/policy boundary, solve-reset, cleanup, campaign,
  and hygiene test passes. Only the pre-existing opt-in `doc_size` policy debt
  fails on the already-recorded PROGRESS/implementation-notes budgets; this is
  not a solver, proof, or candidate regression.
  Rank-27 proof witness `1647775` is now terminal valid UNSAT on node017:
  380.749 s wall, 669,880 KiB peak RSS, solver exit 20. Pinned `drat-trim`
  verifies the complete 1,514,395,111-byte proof in 112.291 s with exit 0 and
  `s VERIFIED`. Proof SHA `0f99f55a...8bb2d` and normalized proof-mode non-time
  SHA `8cd3eda4...c2da7` are byte-identical to the accepted active-decision
  witness. The result publishes only after proof/input/checker/scratch cleanup;
  artifact and derived-artifact deletion are true, cleanup is no longer
  pending, and certificate residue is zero. The independent proof obligation
  is closed; promotion still awaits Claude's terminal fuzz verdict and final
  Andrew report/commit.
  User-requested diagnostic `1647782` now controls the anomalous independent
  Rank-27 solver timings (proof-off `1647694` 450.21 s versus proof-on witness
  `1647775` 380.749 s). One 2-CPU/16-GiB allocation runs the exact same
  `c7c46bfa...057d4` Release pin and staged immutable input proof-on first, then
  proof-off, with the checker only after both solver phases. This deliberately
  gives proof-off any warm/page-cache advantage. Both arms must be exact-state
  UNSAT; the proof must reproduce the accepted SHA, pass pinned `drat-trim`,
  and leave zero residue. Submission is not a result and changes no behavior.
  Controlled pair `1647782` completes both solver phases and the checker on
  node017; its scheduler `FAILED 1:0` is only a post-validation wrapper
  classification error because anchored `rg` did not accept `drat-trim`'s
  carriage-return-formatted `s VERIFIED` line. Proof-on first is UNSAT exit 20
  at 405.375 s internal / 405.86 s wall and 669,180 KiB; proof-off second is
  UNSAT exit 20 at 376.843/377.31 s and 640,736 KiB. Thus same-allocation proof
  generation costs 28.532/28.55 s (+7.57%) even though the later proof-off arm
  receives the warm-state advantage. Checking adds 106.37 s, so the complete
  proof pipeline is 512.23 s. Proof-off and proof-on reproduce their accepted
  non-time SHAs `14f7da2d...6194` and `8cd3eda4...c2da7`; all common logical
  state is exact, with only the expected proof-sink-dependent 432 versus 0
  Sweep general-pair emission count. The 1,514,395,111-byte proof reproduces
  accepted SHA `0f99f55a...8bb2d` and pinned `drat-trim` exits 0 VERIFIED.
  EXIT cleanup leaves zero certificate/input/checker residue. This establishes
  that the earlier independent 380.749-versus-450.21 inversion was cross-run
  environmental variation, not a proof-mode speedup; no rerun or solver repair
  is required.
  Claude Code's authoritative terminal report now classifies exact-source
  ASan/UBSan native-LRAT fuzz PASS. Prep `1647778`, main leg `1647779`, and
  compaction-reach phase `1647781` preserve source-diff SHA
  `681d17c9...ee1b6`; attempts `1647776`/`1647777` were pre-submission stale-
  pin/probe failures and `1647780` was an unhandled wrapper timeout after 14
  clean cases, not solver failures. Corpus is 27/27 and random fuzz is
  10,000/10,000 with zero failures: all 5,668 SAT models and 3,906 UNSAT native-
  LRAT proofs validate independently, and pinned Kissat agrees on every one of
  the 9,574 verdict cases. The 426 bounded five-second semantic
  nonterminations carry no oracle claim. Dedicated reach validation adds 8 SAT
  and 10 UNSAT checked cases and reaches VariableCompaction in 17/17 passes;
  Substitute reaches 2,252 main-leg plus 18 reach cases. Claude reports no
  sanitizer finding, crash, invariant abort, rejected model/proof, differential
  disagreement, repair trigger, or certificate/scratch residue. Codex accepts
  this report without inspecting fuzz artifacts. This closes the final
  correctness gate; only the user-requested proof-mode timing control and final
  report/commit remain before promotion.
  The proof-mode control is now classified above and the final three-page
  Andrew handoff is `report-to-andrew/2026-08-25-2100-rank27-import-rank-inverse/`:
  TeX SHA `41628697...035a`, PDF SHA `83f26ba2...5fd`, 160,789 bytes. It compiles
  in two LaTeX passes with no underfull/overfull box warning. Final source-diff
  SHA remains `681d17c9...ee1b6` and clean Release binary SHA remains
  `c7c46bfa...057d4`. Exact prefix/trace, full Rank 27, complete checked B28+C5,
  independently verified Rank-27 proof, full functional Release suite, Claude-
  owned fuzz PASS, zero certificate residue, and the requested mode-control
  diagnosis are all closed. The isolated Kernel representation repair lands at
  `dc77614`; collection/rewatching begins only from that accepted baseline.
  Read-only source/profile review names whole-arena collection as the next
  possible broad-search representation owner after active decision: exact
  conflict-9450 Tactix assigns 4.91% of sampled cycles to
  `kernel_collect_garbage`, while the already accepted full current-Kissat
  profile distributes markedly less across compact/collect/rewatch functions.
  Because the latter full trajectories differ, this is localization only.
  Current-instrumented-Kissat job `1647653` is submitted at conflict 10000
  under two CPUs/16 GiB to measure the conservative common-state envelope and
  confirm both solvers perform the same compact phase; it does not rerun a
  frozen scoring reference and cannot authorize a behavioral change alone.
  It completes intentional UNKNOWN at exactly 10000 conflicts with zero lost
  samples and residue. The initial same-compact premise is corrected: Kissat
  performs seven sparse collections and two literal compactions, versus the
  shorter Tactix prefix's one printed compact pass. Even with 550 extra Kissat
  conflicts, explicit collection/flush/rewatch owners total 4.521B sampled
  cycles; the corresponding shorter-prefix Tactix family totals 7.451B
  (1.648x), excluding generic search-time `move_best_watch_to_front` from the
  Tactix sum. Function boundaries differ and the Kissat metrics overlay carries
  unrelated observer overhead, so this is credible localization, not a repair
  claim. After the two validated increments land, trace exact collection
  substages and source operations before pricing any exact representation arm.
  Existing exact-profile disassembly now places about 60% of
  `kernel_collect_garbage` local samples in its per-literal watch maintenance:
  `WatchLists::compact` stably retains entries and recomputes head metadata,
  then `map_large_refs` walks every retained entry again solely to forward
  large-clause Refs. The first bounded successor hypothesis is one stable fused
  compact-and-forward primitive preserving blockers, entry order, binary
  counts/flags, sparse-prefix semantics, and forwarding. This remains queued
  behind the BVE and active-decision wraps; no source prototype exists yet.
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
