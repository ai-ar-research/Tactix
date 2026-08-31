# Progress

`DEVELOPMENT_PLAN.md` owns scope and evaluation policy; `docs/calculus.md`
owns proof semantics; `docs/implementation-notes.md` owns engineering
invariants; and `docs/scheduling-parity-ledger.md` owns N4. Frozen campaign
manifests and dated reports own experiment detail. This file is the compact
resume surface.

## Current resume anchor

- Checkpoint `79dc3bfacb2715d1864cf8d9c3e55aec2cee87f3` evaluates a Release
  binary built on `1c719fc8...`; pinned Kissat is
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
- Homogeneous campaign `20260831-3630-tierd-ranks1-40-transitive-unit`, job
  `1657564`, is complete: 40/40 expected verdicts, all 22 SAT models and all 18
  UNSAT proofs verified, with zero residue. Andrew-facing catch-up report:
  `report-to-andrew/2026-08-31-0140-transitive-unit-and-rank1-40-revisit/`.

## Latest candidate performance

- Release SHA `5743b0a...03698`, homogeneous job `1657639`, provisionally solves
  40/40 in 10,290.269 s: 0.973186x preceding, 0.916900x accepted, and 1.340710x
  Kissat; geometric-mean ratios are 0.973948x, 0.930427x, and 1.330300x.
- SAT/UNSAT totals are 4,520.427/5,769.842 s. All 22 SAT models and 16/18 UNSAT
  proofs are independently verified at the snapshot; two checks continue
  nonblocking, with no invalidity or residue. Full table/revisit: campaign
  `3760` `comparison-live.md` and `2026-08-31-fixed-active-rank1-40-codebase-revisit.md`.
- Jobs `1657634`/`1657636` prove Rank 17's old 944,336 active count included
  616,255 retained roots; the repair changes only that field to 328,081 at
  first BVE entry. Rank 17 improves 449.150→314.130 s, Rank 19 452.403→166.486,
  Rank 31 628.742→461.093, and Rank 34 807.219→692.338.
- Rank 33 regresses 395.771→753.767 s; retain the general repair and localize
  its downstream trajectory. Rank 36 remains closed at 385.209 s versus
  Kissat's 368.510 s. Peak RSS remains Rank 10 at 16,987,860 KiB.

## Rank-32 causal closure

- Complete first pre-Congruence formula comparison proved both solvers had
  585,717 identical live large clauses. The first semantic mismatch was a
  Tactix-only XOR degeneracy equivalence `174966 <-> 174968`.
- Gate 397861 was XOR output 174966 over inputs 174968 and 174970. Tactix
  indexed the original clause shape even though 174970 was root-false; Kissat
  simplified before candidate admission and treated the encoding as
  effectively binary. The root-effective index repair removes that mismatch.
- After that repair, the first early-Substitute and early-Backbone binary
  states are exact. At first pre-Sweep, both arms have 756,245 binary copies
  with canonical SHA `358a6bd5...483c4`, plus 580,534 irredundant and 3,436
  redundant large clauses with canonical SHA `7e0dea9e...c681`.
- Sweep seed tracing then found identical first-round seed/environment order,
  but Tactix stopped after 588 seeds at 1,000,001 ticks while Kissat was exactly
  at 1,000,000 and admitted seed 34979. Per-seed behavior differed from the
  first seed 881.
- Selected seed 881 imported the same 256 variables, 235 clauses in the same
  order, and 142 binary attempts. The initial solve cost 312 ticks on both
  sides but returned different SAT models. Source audit found Tactix began the
  fresh Kitten at seed 1 while Kissat begins at seed 0.
- After the lifecycle repair, campaign
  `20260830-1110-tierd-rank32-sweep-seed0-validation`, job `1656759`, matches
  Kissat byte-for-byte on environment order, every query, every model, and
  seed-881 effort (5,745 ticks). Tactix processes the same 589 initial seeds.
- Carry-forward campaign `20260830-1120-tierd-rank32-seed0-target-boundary`,
  job `1656760`, ends at the diagnostic 120-second ceiling before its requested
  conflict-9,464 endpoint, so the whole observer is not acceptance-valid. Its
  selected boundary is complete: all 583,970 large clauses, both binary
  snapshots (756,245 and 755,972 copies), all 1,681 seeds, environment sizes,
  and per-seed tick/solve deltas match Kissat. Both find 159 equivalences.
  Three event rows reverse endpoint print order only; post-Sweep clauses match.
- The causal discrepancy is closed at common input, mechanism trajectory, and
  output state. Detailed chronology is in
  `benchmarks/reports/2026-08-30-codebase-revisit-and-otfs-candidate.md`.

## Tests and correctness

- Full trace SubSolver oracle passes 100,000 randomized formulas; focused event
  trace/overlay checks and the complete 76/76 Release suite pass. Configuration
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

1. Historical Rank-33 localization after the exact Kitten seed repair:
   repair rather than reverting it or adding a shape gate. Two-arm Rank-33
   conflict-10k prefix campaign `20260830-2320-tierd-rank33-prefix10k`, job
   `1656867`, reached conflicts 2,012/2,003 under its 300-second ceiling. After
   masking one missing Kissat observer row at conflict 1,171, every portable
   event in the overlap is exact. Startup, preprocessing, and early search are
   excluded. Two-arm continuation `20260830-2330-tierd-rank33-post2003`, job
   `1656869`, reached conflicts 3,809/3,071. After masking one observer-only
   row at 2,106, the whole portable overlap is still exact with no macro-state
   divergence. Next use learned-only tracing from conflict 3,000, then reopen a
   narrow full-detail window at the first learned mismatch. That learned-only
   cut, campaign `20260830-2340-tierd-rank33-learned250k`, job `1656871`, is
   valid through conflict 250,000. Learned streams are exact through conflict
   15,347. At 15,348 Tactix has two fewer decisions/levels and learns a
   different size-22 clause. Full-detail conflicts 15,347--15,349 campaign
   `20260830-2350-tierd-rank33-window15347`, job `1656873`, is valid. The
   first 14 post-conflict decisions match; recorded decision 5,646,489 picks
   variable 19,823 in Tactix versus -12,222 in Kissat. Heap cut
   `20260830-2360-tierd-rank33-heap-pop5646489`, job `1656875`, proves the
   score of 19,823 is exact: Kissat skips it because it is assigned, while
   Tactix does not. Propagation cut `20260830-2370-tierd-rank33-imply19825`,
   job `1656877`, traces the assignment to level 11 after common decision
   -25,805. Propagation order already differs immediately after the first
   common post-conflict decision, although conflict 15,347 and its learned
   clause are exact. Portable non-root assignment hashes in campaign
   `20260830-2390-tierd-rank33-assignment-hash-nonroot`, job `1656881`, are
   exact at analysis end (15,052 assignments, hash `05910eef13c8073e`) and
   immediately after common decision +9,908. Its propagation produces 21
   assignments in each solver but different sets: Tactix alone has 10,166 and
   Kissat alone has 9,912. Next compare exactly the ordered watch list scanned
   for +9,908 and those two binary clauses; Reorder and later trail retention
   are excluded at this boundary. That watch cut, campaign
   `20260830-2400-tierd-rank33-watch9908` (job `1656883`), finds 19 binary
   watches in both solvers. Every common binary retains the same order, but
   Tactix has `(-9908,10166)` where Kissat has `(-9908,9912)`; the large-watch
   sets also differ. The lifecycle cut proves both binaries initially exist in
   Tactix and that BVE deletes `(-9908,9912)` at conflict 15,347. Kissat's
   lifecycle arm records both initial additions but times out before the BVE
   phase because that observer is too expensive, so it is not accepted as a
   completed diagnostic. A diagnostic-only narrow BVE heap observer passes
   focused tests. Campaign `20260830-2420-tierd-rank33-bve-heap9912-10166`
   (job `1656887`) is valid in both arms and narrows the owner: target candidate
   scores and their initial positions match, then the selected-variable heap
   trajectories diverge and Tactix eliminates 9,912 while Kissat eliminates
   10,166. The then-reported 310,880 Kissat “schedule size” was only the heap
   size at the last selected-variable insertion event, not the completed
   schedule. Job `1656891` corrects that interpretation below. The
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
5. SAT Rank 20 historical mechanism closure: Tactix was 594.433 s versus
   Kissat 140.142 s. Full-row pre-Factor job `1657022` was observer-invalid:
   Tactix hit 600 s exporting 13.5M rows; job `1657024` also hit 600 s by
   rescanning at every boundary. Job `1657026` exposed a pre-/post-admission
   boundary mismatch and its Tactix task was canceled. Tactix-only job
   `1657028` completed the exact pre-admission boundary: both solvers have
   1,011,733 irredundant large clauses, 12,553,619 binary copies, identical
   duplicate-sensitive clause/binary hashes, 1,591,244 unassigned variables,
   and 3,888,736 propagations. Kissat's lower root counter is compaction of
   parse-fixed variables, not a semantic mismatch. Both decline startup Factor
   under equivalent `factordelay`. Job `1657029` was canceled when the old
   trace cap was proven to suppress its terminal event. The shared observer
   now has a tested target-only fast path and terminal-cap bypass; two-task
   replacement `1657031` then hit 1,200-second trace-overhead caps without a
   boundary row. Job `1657033` found Factor offers exact at conflicts 0, 4,547,
   and 14,005, then Tactix/Kissat at 28,690/28,694 and 48,921/48,925. Tactix
   first admits at 48,921, after the four-conflict trajectory split. Kissat's
   generic Probe journal incorrectly used probing rather than Factor ticks;
   one-task correction `1657035` confirms 197,079,650 Kissat ticks versus
   197,034,943 Tactix ticks (0.023% difference). Compare the still-common
   conflict-14,005 pre-Factor state after Probe phase 2 next. Two-task job
   `1657036` also hit full-trace overhead caps. Replacement `1657038` keeps
   tracing unopened until that exact boundary, then reuses the same one-row
   fingerprint. It completed in 34/55 seconds and proves the state already
   differs: Tactix/Kissat decisions are 318,296/284,845, propagations are
   215.7M/233.1M, and formula counts, hashes, and top-32 order disagree.
   Factor is downstream. Narrow once to the preceding common offer at conflict
   4,547 with two-task late-open job `1657040`, then inspect the owning
   inprocessing/search mechanism. It also differs before Factor: decisions are
   233,628/210,133, propagations 169.4M/194.0M, and formula hashes disagree.
   Two-task job `1657042` compares conflict-4,547 Probe entry before
   Congruence. It proves the irredundant formula is still exact but search has
   already accumulated 89 extra Tactix learned clauses and one extra binary.
   Job `1657044` proves the trajectory exact through conflict 50. Jobs
   `1657046`/`1657048` then localize the first split to conflict 502, decision
   62,811: RNG word and slot are exact, but slot 490,347 maps to Tactix
   external 573,525 versus Kissat 2,745. Repair the general compact random-index
   order (import order, not activation order); checkpoint `1657049` now maps
   the exact shared slot to 2,745. Job `1657050` is 123.02 s versus 594.433 s
   and Kissat 140.142 s, with exact Kissat conflicts/decisions/Factor/BVE.
   Uniform job `1657091` is the preceding 40/40 authority at 9,305.953 s. Later Rank-34 jobs close Eliminate formula/watch order, all 571 BVE removal ordinals, and the second TransitiveReduce pass, where immediate failed-source units retain two binaries Tactix previously deleted.
   Checked job `1657561` is SAT in 804.709 s; uniform job `1657564` solves 40/40 in 10,573.791 s. The 13.6% total regression is downstream trajectory evidence, not grounds to revert general parity or add a selector. Release 76/76, Trace 75/75, native LRAT, and focused ASan/UBSan pass; N1--N4 remain intact.

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
