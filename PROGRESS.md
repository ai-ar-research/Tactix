# Progress

`DEVELOPMENT_PLAN.md` owns scope, architecture, gates, and evaluation policy;
`docs/calculus.md` owns proof-rule semantics; `docs/implementation-notes.md`
owns engineering invariants; and `docs/scheduling-parity-ledger.md` owns N4.
Frozen manifests and reports own experimental detail. Git preserves the former
session narrative; this file is only the current resume surface.

## Resume anchor

- Work in `/home/hwu/SAT-solving` on branch `main`. Accepted solver source is
  commit `3c3164d`, based on accepted predecessor `a8b41c6`; the following
  wrap-up commit changes documentation only. The accepted Release binary
  SHA-256 is
  `f2eecac104f6b4247197bbfbe5ffe6542b21c3735a30ca5b49e27574f05c4dcf`.
  Evidence: `benchmarks/reports/2026-08-27-rank4-factor-fresh-admission.md` and
  `report-to-andrew/2026-08-27-0557-rank4-factor-fresh-admission/report.pdf`.
- The complete promotion chain is terminal PASS: exact prefix array `1650366`;
  three paired Rank-27 timing waves in array `1650368`; full-width B28+C5
  array/aggregate `1650774`/`1650840`; and Claude-owned fuzz campaign
  `20260827-0040-sweep-phase-word-fuzz` with prep `1651241`, generated legs
  `1651243`/`1651244`/`1651247`, and reach `1651246`. The gate checked 17 SAT
  models and 16 UNSAT proofs per arm with no solve loss; Claude reported
  81/81 corpus and 30,000/30,000 generated cases, 13,914 checked SAT models,
  10,955 accepted native LRAT proofs, zero failures, and zero certificate or
  scratch residue.
- Decision-grade Rank-27 candidate/predecessor internal seconds are
  358.542/358.587, 357.916/358.759, and 357.941/359.460. Median paired ratio is
  0.997650, range 0.995774--0.999875; both arms have under-0.25% absolute
  spread and every candidate run is below 400 seconds. Full semantic SHA is
  `10372d94d2cb1ae86893408435e0c3ffa2338053b11b7012c2107a3bebf1ffd0`.
  Per project-owner direction, Rank 27 is now parked rather than treated as the
  next witness.
- **Predecessor measurement:** frozen campaign
  `benchmarks/runs/20260826-2340-tierd-ranks1-40-current-screen/` re-evaluates
  immutable ranks 1--40 with accepted Tactix `92d9bc1`/binary `a76182a0...4d95`
  and accepted current instrumented Kissat `8af8e56`/binary
  `b6a9b1d0...644d5`. Array `1651248` has 80 independent one-solver tasks;
  classifier `1651287` is terminal. All 80 results are classified with zero
  cleanup or certificate residue. Five `std::bad_alloc`/resource exits occurred
  under the rejected address-space cap (Tactix ranks 5, 10, 13, and 14; Kissat
  rank 10), six Tactix runs reached the 1,200-second solver limit, and three
  otherwise solved UNSAT arms reached the independent checker's 4,800-second
  limit. Every solver job used `-c 12`,
  exactly `--mem=48000M`, the now-rejected 16 GiB `RLIMIT_AS`, 1,200 seconds,
  proof production, and independent proof/model checking; there is no throttle
  or node constraint. Frozen scoring references are not rerun. Preserve and
  classify this campaign as dual-limit diagnostic evidence, but do not use its
  apparent early OOMs to select the next target.
- **Terminal replacement screen:** frozen campaign
  `benchmarks/runs/20260827-0100-tierd-ranks1-40-slurm-memory-screen/`, array
  `1651430`, and after-any classifier `1651464` rerun all 80 arms with the same
  accepted solvers, immutable inputs, and proof/model checks. Every independent
  task uses six physical/twelve logical CPUs, exactly `--mem=48000M`, 1,200
  seconds, no throttle or node constraint, and SLURM task-cgroup memory
  enforcement only; there is no `RLIMIT_AS`. All 80 results are classified:
  Tactix/Kissat have 33/38 valid solves, six Tactix solve timeouts, and three
  independently identified checker timeouts, with no correctness or
  infrastructure failure and zero cleanup/certificate residue. The campaign
  is globally non-scorable because of those invalid rows. Its validated UNSAT
  pairs are diagnostic only; Rank 4 is 14.690/7.728 seconds and Rank 32 is
  416.933/293.092 seconds. Summary SHA-256 is
  `6973ac74261e08ea8677ad64ffaa277f38a9a47af86c668851a933ddd29ff396`.
  Frozen scoring references were
  not rerun.
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
- **Next action:** freeze solver commit `3c3164d` and the unchanged current-
  Kissat comparator as the Rank-32 pins. Establish three paired node018 timing
  waves with independent one-solver tasks at `-c 12 --mem=48000M`, proof/model
  validation, no `RLIMIT_AS`, and under-2% within-arm spread. Then localize the
  earliest common-state worse-side divergence before changing behavior. The
  replacement screen's 416.933/293.092-second pair selects Rank 32 but is not
  itself decision-grade evidence.
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
- Collection is a secondary lead, not the main bottleneck: exact prefix work
  attributed about 4.91% to whole collection, and the rejected forwarding
  traversal only 0.33%. Current Tactix invokes collection more often than the
  accepted current-Kissat envelope, but fused-forward, arena-prepass, and
  generic key-layout prototypes are closed negative. Reopen only on a new
  exact call-reason owner with positive whole-run evidence.
- Residual BVE has no current comparable-state worse-side owner; Congruence's
  three-inline and two-inline table layouts and Substitute call-level deferral
  are rejected. Equal-score Rank-27 heap choices are neutral. Do not reopen any
  of these without contradictory exact source-backed evidence.

## Recent changes

- 2026-08-27 — Rank 4's fresh-polarity Factor admission repair restores exact
  order/state/340,150-tick work, cuts median time 15.340 to 10.733 seconds, and
  passes the full B28+C5 gate plus Claude-owned 30k fuzz.
- 2026-08-27 — The rejected dual-limit ranks 1--40 predecessor is terminal:
  80/80 classified, zero residue, and non-scorable; its address-space exits do
  not rank solver correctness or performance.
- 2026-08-27 — Sweep phase-word expansion passes exact prefix, three-wave
  Rank-27 timing, complete B28+C5, and Claude-owned 30k fuzz; Rank 27 is parked.
- 2026-08-26 — Immutable fuzz snapshots and 50-physical-core generated-leg
  policy are active; checker-root and read-only-snapshot cleanup paths are fixed.
- 2026-08-26 — Timing policy uses `-c 12`, exactly 48,000 MiB, and three paired
  waves; whole-node exclusivity is prohibited.
<!-- track-sched:begin -->
N4 verification remains complete at the accepted parity pin. Living mechanism
dispositions and reopening conditions are in `docs/scheduling-parity-ledger.md`.
The accepted Sweep and Rank-10 layout repairs change no schedule, rule,
candidate, tick, or policy state. The accepted Rank-4 repair changes only the
portable per-polarity admission lifecycle proven above and leaves the rule
instance surface unchanged. Rank 32 is the active selected witness.
<!-- track-sched:end -->

## Gates

- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; blocks Phase 4.5.
- [ ] T5: exceed frozen VBS or ship the best verified effort.
