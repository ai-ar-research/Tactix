# Progress

`DEVELOPMENT_PLAN.md` owns scope and evaluation policy; `docs/calculus.md`
owns proof semantics; `docs/implementation-notes.md` owns engineering
invariants; and `docs/scheduling-parity-ledger.md` owns N4. Frozen campaign
manifests and dated reports own experiment detail. This file is the compact
resume surface.

## Current resume anchor

- Checkpoint `daa4e129b45e6ee81610fe1ca5b5dcad195e5352` is the last local
  commit; pinned Kissat is
  `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.
- Keep every current general repair. Latest closure: live active state excludes
  retained semantic level-zero fixed identities but includes non-root search
  assignments, matching Kissat flags. TransitiveReduce, Congruence,
  random-index, BVE, reconstruction, and creation-order repairs remain active.
- No retained repair has a formula, rank, benchmark, identity, size, density,
  admission, effort, classification, or strategy gate. Preserve N1--N4.
- Production uses Kissat creation order unconditionally. CaDiCaL arena order
  remains only the explicit `TACTIX_ARENA_ORDER=1` diagnostic arm.
- Production BVE/Factor/forward-subsume/Vivify remain uniformly enabled at
  100/50/100/100 permille. Removed Dose5, Factor5, NoBve, NoBveLight, and
  StartupFactorOnly profiles stay removed.
- Homogeneous campaign `20260831-3760-tierd-ranks1-40-fixed-active`, job
  `1657639`, is complete: 40/40 expected verdicts, all 22 SAT models and all 18
  UNSAT proofs verified, with zero residue. Rank 36 remains closed.
- Current increment contains general BVE lifecycle and Factor candidate-filter
  repairs plus bounded observers. Valid paired job `1657793` proves the selected
  ternary survives BVE, forward processing, Eliminate, search, arena relocation,
  and conflict-19,378 Probe entry. Full trace:
  `2026-08-31-rank33-factor-lifecycle-handoff.md`.

## Latest candidate performance

- Release SHA `5743b0a...03698`, homogeneous job `1657639`, provisionally solves
  40/40 in 10,290.269 s: 0.973186x preceding, 0.916900x accepted, and 1.340710x
  Kissat; geometric-mean ratios are 0.973948x, 0.930427x, and 1.330300x.
- SAT/UNSAT totals are 4,520.427/5,769.842 s. All 22 SAT models and all 18 UNSAT
  proofs are independently verified, with no invalidity or residue. Full table/revisit: campaign
  `3760` `comparison-live.md` and `2026-08-31-fixed-active-rank1-40-codebase-revisit.md`.
- Jobs `1657634`/`1657636` prove Rank 17's old 944,336 active count included
  616,255 retained roots; the repair changes only that field to 328,081 at
  first BVE entry. Rank 17 improves 449.150→314.130 s, Rank 19 452.403→166.486,
  Rank 31 628.742→461.093, and Rank 34 807.219→692.338.
- Rank 33 regresses 395.771→753.767 s; retain the general repair and localize
  its downstream trajectory. Rank 36 remains closed at 385.209 s versus
  Kissat's 368.510 s. Peak RSS remains Rank 10 at 16,987,860 KiB.

## Rank-33 current localization

- At BVE conflict 15,347 both solvers generate ternary
  `(-237373,-237365,-145)` from the same 2x8 transaction. Valid jobs `4140` and
  `4170` prove both forward passes retain it; invalid observer jobs are
  classified clean and make no claim.
- Job `4210` follows the same clause through conflict-17,312 relocation and to
  Probe entry with exact decisions, heap prefix, formula counts, pose, and
  metadata. Job `4220` proves every Probe subrule retains it and confines the
  score 2-versus-1 split to Factor's dense candidate construction.
- Source comparison found Tactix omitted Kissat's separate final survivor
  recheck against the last `factorcandrounds` count rebuild. The uniform recheck
  and a three-layer peel regression are implemented. Causal job `1657806`
  (`4240`) now matches `-237365` active=1, marked=1, value=0, degree=1 in both
  solvers. Job `4230` is invalid/clean and contributes no paired claim.
- Proceed to Release, Rank-33, and Tier-B+C validation. Do not restore a profile
  or add a formula/rank selector. Release passes 76/76; Rank-33 job `1657808`
  is valid and clean at 468.533 s, with its 2.137 GB DRAT independently verified.
  Fixed
  B28+C5 job `1657809` and homogeneous Rank-1--40 job `1657810` are running
  from the same immutable Release pin. B28+C5 has all 66 expected solver
  verdicts and checked artifacts at 2,010.962/2,444.835 s candidate/shipped
  (0.822535x PAR-2): all 66 rows are valid and clean, so the gate passes.
  Rank 1--40 has all 40 expected solver verdicts in
  10,848.684 s: 1.054266x preceding, 0.966657x accepted, and 1.413466x Kissat;
  all 22 SAT models and 18 UNSAT proofs verify, and all 40 rows are clean.

## Rank-25 current localization

- Rank 25 regresses 270.955→760.379 s with exact search/BVE work but BVE time 13,803→509,433 ms over the same 3,296,386 applications: an implementation-unit-cost regression.
- Matched campaign `20260831-4290-tierd-rank25-bve-unit-cost`, job `1657922`, is valid/clean: previous 263.337 s / 13,779 ms BVE versus current 755.590 s / 507,738 ms, with exact work and verified models.
- Streaming cleanup passes three waves at median 0.348100x current; Rank33 and B+C are checked/clean (B+C 0.996294x shipped), while homogeneous `1658000` at 10,266.505 s has 6 proofs continuing asynchronously.

## Rank-38 current localization

- Homogeneous Rank 38 is a stable search-work loss: candidate 790.264 s versus
  pinned Kissat 366.258 s, with 35,907,678/47,372,472 conflicts/decisions
  versus 18,430,710/24,657,248.  Its accepted/current times are essentially
  unchanged, so it is the next general search-trajectory owner rather than a
  volatile downstream rank.
- Fresh pinned trace binaries and the fixed Rank-38 input match exactly through
  conflict 50,000: all 458,818 common portable decisions, conflicts, analysis
  boundaries, and learned-clause stages agree.  Candidate job `1658059`
  (`20260831-4361-tierd-rank38-trajectory-prefix`) is valid and untruncated;
  Tactix's eight-row suffix is solely its stop landing at conflict 50,001.
- Predecessor job `1658057` is observer-invalid, not a solver divergence.  A
  Tactix-only generic 1,024-call post-Reorder detail budget suppressed compact
  `analysis_begin`/`analysis_end` rows after conflict 10,043 while Kissat kept
  them.  Trajectory mode now retains every portable analysis boundary, with a
  regression that exhausts the old budget.  The repair is diagnostic-only and
  N4 read-only.
- Learned-only campaign `20260831-4362-tierd-rank38-learned-prefix1m`, job
  `1658061`, first differed at conflict 136,869.  Narrow mode-clock traces
  localized the cause to one extra Tactix search tick at conflict 128,927:
  scan `+367` had identical 1,501-entry watch order, clause poses, `searched`
  cursors, and values, but Tactix charged 160 relocations while Kissat charged
  159.  Both removed 160 large entries; the remaining Kissat removal was the
  garbage 21-literal clause later identified as Tactix clause ID 147584.
- Kissat retires that clause at conflict 128,927 through
  `eagerly_subsume_last_learned`.  Tactix had inserted the exact clause into
  its recent-learned register at conflict 128,925, then cleared the register
  during Vivify's Tactix-only reclamation collection.  Pinned Kissat does not
  collect at Vivify exit, so its register survives.  The general repair adds
  an explicit collector mode that forwards live recent-learned refs (and
  invalidates garbage refs) only for implementation-only Vivify reclamation;
  genuine Kissat-equivalent collections retain their unconditional reset.
  This is rule/mechanism dependent only and adds no formula or workload gate.
- Focused Analyze/EventTrace tests pass.  After repair, clause ID 147584 is
  preserved and eagerly deleted at conflict 128,927; scan `+367` is exact at
  159 relocations and 429 ticks.  A fresh local learned-only run matches all
  996,949 pinned-Kissat final learned records exactly through conflict
  1,000,000.  Release 76/76 and Trace 75/75 pass.  Rank-33 campaign
  `20260831-4370-tierd-rank33-vivify-eager-register`, job `1658063`, returns
  expected UNSAT provisionally in 466.840 s (prior valid 467.925 s); its proof
  check continues nonblocking.  Full fixed B28+C5 campaign
  `20260831-4380-BC-vivify-eager-register`, job `1658064`, is submitted with
  shipped/candidate SHA pins `c18133ce...cab00` / `9b53e154...52ab6`.
  All 66 provisional verdicts are expected: candidate 2,007.209 s versus
  shipped 2,000.647 s (1.003280x), inside the 1.10 guard; final validation
  continues.  Uniform Rank-1--40 campaign
  `20260831-4390-tierd-ranks1-40-vivify-eager-register` first had an
  infrastructure-only job `1658130`: all tasks stopped before solver invocation
  because the fresh runner directories were absent; its logs are retained.
  After creating those empty directories, job `1658170` is the sole solver
  execution from the same candidate pin, with no comparator rerun or selector;
  proof checks may finish asynchronously.  Its repaired Rank 38 is expected
  UNSAT in 388.117 s / 18,197,625 conflicts, versus the preceding 790.264 s /
  35,907,678 and pinned Kissat's 366.258 s / 18,430,710: search work converges,
  leaving a roughly 6% implementation-cost gap on this witness.  The full
  solver phase is 40/40 expected in 9,758.178 s (SAT 4,473.787; UNSAT
  5,284.391): 0.950487x the preceding candidate, 0.869489x accepted Tactix,
  and 1.271385x pinned Kissat.  Model/proof checks and zero-residue
  classification continue asynchronously.

## Next attribution gate

- After the active validation and checkpoint commit, first classify the largest
  remaining gaps as search-work, matched-work runtime/RSS, or mixed.  Inventory
  implementation-only arena collections and their bytes/time/state effects as
  read-only evidence, not as a presupposed production rewrite.
- Preserve independent immutable proof identities, N1--N4, and unconditional
  Kissat creation order.  Select physical representation convergence only if
  attribution shows it dominates; otherwise resume the leading general search
  mechanism.  Any stage needs focused differential tests and isolated evidence
  before cumulative B+C and homogeneous validation.
- Initial immutable-authority attribution favors search localization: the
  remaining aggregate gap is 2,082.942 s; Ranks 27/31/34 contribute 1,202.346 s
  with 3.067x/2.201x/1.757x conflicts, and adding Ranks 22/37/21 reaches
  1,867.295 s (89.65% of the gap) with material work divergence.  Keep bounded
  representation measurements on matched-work witnesses, but do not promote a
  broad arena refactor ahead of the leading stable search discrepancy.

## Tests and correctness

- Full trace SubSolver oracle passes 100,000 randomized formulas; the complete
  75/75 Trace and 76/76 Release suites pass. Configuration
  wiring has zero failures; campaign pins are hash-checked per task.
- Two observer campaigns were correctly disqualified after their SAT checker
  bundle omitted `modelcheck.py`. Tier-D preparation now pins that runtime
  module with `checkmodel` and fails closed; positive and negative regressions
  cover the recurrence.
- Proof checks are nonblocking for timing but remain authoritative and preserved.
- Fresh campaigns must be all-valid before scoring. Solver execution is
  Slurm-only, one solver per task, immutable pins, no node constraints, with
  model/proof validation and zero-residue cleanup.

## Open work

1. Historical Rank-33 prefix, heap, watch, and early BVE localization is closed
   and retained in its dated reports; the current Rank-33 resume state is the
   compact section above. The
   non-reference `bve_min_bound` selective admission cache is removed;
   `bve_retry` now mirrors Kissat's eliminate flag, including blanket re-arm
   only when the bound advances. The first changed-arm validation (job
   `1656889`) correctly showed that cache removal alone did not change this
   boundary, excluding it as the cause. Kissat clears still-set flags after an
   incomplete elimination round and does not advance the bound.
   Tactix now does the same, with a focused regression fixture. Focused BVE,
   slot-pose, event-trace, and overlay tests pass. The changed-arm successor,
   `20260830-2440-tierd-rank33-bve-incomplete-validation` (job `1656890`), is
   valid but also non-closing: its 84-record trace has the exact same
   `be4763db...` digest as job `1656889`, including the target heap trajectory.
   Both exact lifecycle repairs remain; the next bounded observer summarizes
   every BVE schedule without another expensive full lifecycle trace.
   Its focused tests and both instrumented builds pass. Two-arm campaign
   `20260830-2450-tierd-rank33-bve-schedule-sets` (job `1656891`) is valid in
   both arms. The first schedule is the exact same 429,743-variable external
   set in both solvers (all three fingerprints match at bound 0). This corrects
   the earlier selected-event interpretation and excludes retry-set admission
   as the first-round cause. The first two observed pops match; the next heap
   transition differs. Add ordered topology and score-multiset fingerprints to
   distinguish initial heap construction from dynamic update mechanics.
   Focused tests and both trace builds pass; campaign
   `20260830-2460-tierd-rank33-bve-topology` (job `1656893`) is valid in both
   arms. At the first round, insertion hash `eaf80835b69cd9e3`, topology hash
   `1ad753a321a3f75d`, both score fingerprints, and all set fingerprints match
   exactly. Thus compact order, initial scores, and heap construction are
   excluded. The owner is a dynamic elimination/update difference after the
   initial common pops. Locate its first pop-stream mismatch with cumulative
   block hashes, then open only that ordinal block. Focused tests and both
   trace builds pass; two-arm campaign
   `20260830-2470-tierd-rank33-bve-pop-blocks` (job `1656895`) is valid in both
   arms. Cumulative external-ID hashes are exact through pop 131,072
   (`5811a78766ecde2c`) and differ by pop 135,168, with equal heap size. Open
   only ordinals 131,073--135,168 to identify the first different root.
   Two-arm campaign `20260830-2480-tierd-rank33-bve-pop-window` (job `1656897`)
   is valid in both arms. Pops match through ordinal 132,360: both pop 91,836
   at score 3,999,990.5 and expose 91,843 as root. The next pop differs:
   Tactix promotes 91,837 at score 3,999,991 while Kissat pops 91,843 at
   3,999,990.5. Candidate 91,836's elimination/update path owns the first
   divergence. Two-arm campaign
   `20260830-2490-tierd-rank33-bve-candidate91836` (job `1656899`) reached the
   target in both arms, but its Kissat arm hit the 100,000-row observer cap
   before the stop boundary and is invalid overall. Its completed target
   prefix exposes the mechanism: Kissat marks three duplicate-preserving
   opposite-side binary watches as AND/OR gate members; Tactix marks only two
   because its sparse lookup retains one index per partner. This changes the
   duplicate resolvent and 91,837's score. Tactix now marks every matching
   binary copy, with a direct duplicate-gate regression test. Focused tests
   pass. Changed-arm campaign
   `20260830-2500-tierd-rank33-bve-duplicate-gate-validation` (job `1656901`)
   is valid and closes the boundary: gate counts are 1/3, all four resolvent
   hashes and their order match, 91,837 finishes at 3,999,989, and the next
   pops are 91,843 then 91,845. The implementation uses one linear side scan,
   adds no admission/effort/shape selector, and preserves N1--N4.
   Full Release validation passes 76/76. Uniform Rank 1--40 campaign
   `20260830-2510-tierd-ranks1-40-bve-duplicate-gate` is job `1656902`; it
   pins Release SHA `6ef2c6d5...da9bc7a2`, uses the unchanged 40-rank matrix,
   and reruns no comparator. Solver verdict/timing is provisional and
   nonblocking; proof checkers remain alive to finish independently.
   While that immutable pin runs, the complete pinned-`ands.c` follow-through
   found a complementary edge: one unrelated duplicate stays antecedent and
   later copies become gate members after Kissat consumes the literal mark.
   The sparse index now retains the first physical copy and reproduces both
   required- and unrelated-partner duplicate semantics in one scan. A focused
   content-sensitive BVE test passes. This second fix is deliberately absent
   from job `1656902` and awaits its own release/cumulative validation.
   The follow-through also found that pinned Kissat accepts a physical large
   AND base shortened by root assignments to pivot plus one input when the
   other side has at least two distinct binary partners. Tactix's equivalence
   detector cannot cover that physical-large case. AND detection now carries
   Kissat's distinct-partner precondition and accepts the shortened base; a
   focused resolvent test and the full 76/76 Release gate pass. The cumulative
   follow-up Release SHA is `0ea7b7ca...a529994` (diff `9287df9d...fcbc33c`).
   Job `1656902` is solver-complete: all 40 provisional verdicts are expected,
   total 10,524.426 s. Versus job `1656827`, total is 1.02909x and geomean
   1.06488x; versus accepted Tactix, total is 0.93776x; versus pinned Kissat,
   total is 1.37122x. Rank 33 improves 776.358→469.639 s and Rank 36 stays
   stable at 384.756 s; Rank 19 regresses 39.330→261.796 s through a much
   longer search trajectory but remains faster than Kissat's 739.902 s.
   Eleven proof checkers were still running at this snapshot and remain alive.
   Uniform cumulative follow-up campaign
   `20260830-2520-tierd-ranks1-40-bve-gate-complete` is job `1656942`. It
   pins Release SHA `0ea7b7ca...a529994`, uses the identical matrix, compares
   reporting-only to job `1656902`, and contains no selector or comparator
   rerun. Provisional timing remains independent of proof-check completion.
   Validation correction: jobs `1656902` and `1656942` omitted the adjacent
   `pins/modelcheck.py` module required by the pinned `checkmodel` wrapper.
   Their 22 SAT checks therefore fail infrastructurally; exact stdout/models
   were deleted with task scratch. Both campaigns are observer-only, are not
   acceptance candidates, and their records will not be overwritten. UNSAT
   proof checks remain valid and continue. The next cumulative campaign must
   pin `modelcheck.py` SHA `f70f0eef...1676cf` before submission and is the
   only candidate eligible for all-valid acceptance.
   Job `1656942` is solver-complete with 40/40 expected verdicts but remains
   observer-invalid for the SAT packaging issue. Total is 10,553.166 s:
   1.00273x job `1656902`, 0.94033x accepted Tactix, and 1.37496x Kissat;
   geomean is 0.98045x, 0.94173x, and 1.34647x respectively. Ranks 19, 31,
   32, and 35 improve materially; Rank 37 regresses 420.727→747.713 s and
   Rank 33 regresses 469.639→639.537 s, while Rank 36 stays at 384.237 s.
   Acceptance-eligible campaign
   `20260830-2530-tierd-ranks1-40-bve-physical-definition` is job `1656982`.
   It pins Release SHA `d304e8e0...a691f0e5` plus `drat-trim`, `checkmodel`,
   and `modelcheck.py`; the runner hash-checks all four before every task. It
   uses the identical uniform matrix, no selector, and no comparator rerun.
   Tracked Tier-D tooling now pins `checkmodel` and `modelcheck.py` as one
   checked bundle and fails closed when the runtime module is absent. The
   direct pinning regression and campaign-certificate-cleanup tests pass.
   Superseded observer jobs `1656902` and `1656942` retained 11/18 and 9/18
   verified UNSAT proofs respectively. Their remaining proof-only tasks were
   then canceled: missing SAT models already made acceptance impossible, and
   those tasks were holding seven replacement-campaign ranks pending. Job
   `1656982` was untouched; all of its remaining ranks started immediately.
   A subsequent `definition.c` audit found that Kissat removes only the pivot
   when exporting a physical large occurrence to Kitten; root-false literals
   remain subsolver variables. Tactix incorrectly reused resolution's stripped
   image and could prove a definition absent in Kissat. Definition import now
   reads the physical clause without allocating a duplicate image, and LRAT
   lifting starts from that same physical original. A fixture distinguishes a
   SAT physical environment from its artificial UNSAT stripped image; focused
   and full 76/76 Release tests pass. Release SHA is `d304e8e0...a691f0e5`.
2. Rank 34 causal report is
   `benchmarks/reports/2026-08-30-rank34-sat-divergence.md`; final revisit is
   `benchmarks/reports/2026-08-31-transitive-unit-rank1-40-codebase-revisit.md`.
   Jobs `1657550`/`1657552` localize the second conflict-7,705 TransitiveReduce
   pass and its only excess deletions. Job `1657554` proves Tactix deferred two
   failed-source units that Kissat commits immediately. The unconditional rule
   repair retains prior tombstones, emits native-LRAT BFS chains, and reaches
   root/H per source. Job `1657559` makes all four binary snapshots exact and
   closes the two units and end counts. Checked job `1657561` improves Rank 34
   1,014.550→804.709 s. Uniform job `1657564` solves 40/40 but regresses total
   9,305.953→10,573.791 s; retain fixes and localize downstream trajectory.
3. Required Claude-owned fuzz remains blocked by private-workspace access.
   Do not inspect or replace Claude fuzz artifacts. Ask the user for explicit
   authorization on return, then run exactly one consolidation fuzz campaign.
4. Andrew report `2026-08-31-0140-transitive-unit-and-rank1-40-revisit`
   records the Rank-34 closure and homogeneous validation. Required gates pass;
   record remote publication only after independently verifying it.
5. Historical Rank-20 random-index and Rank-34 TransitiveReduce closures are
   retained; their dated reports own the full causal evidence and validation.

## Architectural contract

<!-- track-sched:begin -->
- Kissat creation order is the unconditional production default.
  `TACTIX_ARENA_ORDER=1` is an explicit CaDiCaL diagnostic arm only.
- Unsupported formula-dependent profiles stay removed. No formula, size,
  density, benchmark, identity, or rank gate may replace them.
- A sound general repair is retained through downstream regression while the
  discrepancy is localized. Do not mask it with a shape profile.
- Preserve N1 soundness, N2 proof accountability, N3 resource discipline, and
  N4 reference scheduling parity.
<!-- track-sched:end -->

## Immutable authorities and gates

- Accepted source watermark: `bad5cbf7548c6abf1181d36150206ac6f52c316f`.
- Accepted binary SHA: `a3f26ae0...f6859`.
- Fresh accepted/Kissat authority:
  `20260829-1930-tierd-ranks1-40-accepted-fresh`, 80/80 valid.
- [x] T1: MiniSat correctness/performance bar.
- [x] T2: Glucose/API bar.
- [x] T3: matched Tier B/C Kissat coverage and at most 1.10x PAR-2.
- [ ] T4-K: Kissat coverage and at most 1.10x on SC24, SC25, SC26, and union.
- [ ] T4-VBS: exact status-sensitive frozen-VBS parity; then T5 best effort.
