# Sweep pose repair: Claude-owned consolidation fuzz campaign (terminal report)

Campaign `benchmarks/runs/20260907-sweep-pose-fuzz` (plan section 8.2, one
campaign, no replication). Authoritative record: its `manifest.json`; every
job, pin, log, and audit named below lives under that directory unless a path
says otherwise.

## Verdict

**FAILURE for the submitted candidate, PASS for the repaired candidate.**

- The submitted candidate (`96e894c` + `20260907-5302-sweep-pose-repair/
  candidate.patch`, Release SHA `f180cbc7…`, pins `pins/`) failed the campaign
  at its preparation probe: three debug-assertion crash classes on the dense
  mixed-occurrence image under free N1 schedules. All three reproduce
  byte-for-byte on the plain base `96e894c`, so the 5302 pose patch did not
  introduce them; they are regressions from `02ab6f1` and `1148168`, after the
  last passing campaign (`20260901-4631` at `f51a988`).
- The bounded repair (below) removes every class. The complete campaign on
  the repaired pins `pins-r2` is clean: six generated legs plus the reach leg,
  0 failures of any class, every coverage floor and reachability requirement
  met, zero residue.
- Accepted source: `candidate-r2.patch` on `96e894c` (the live worktree's
  `src/` and `tests/` at the time of this report, verified identical to
  `build/pin-sweep-pose-fuzz-r2-src`). Release SHA
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`. The
  repair relative to the r0 candidate source is `repair.patch`.
- Not done here, by instruction: PROGRESS, the Andrew report, commits, pushes,
  and performance work. The B28+C5 checked gate below was measured on the r0
  Release binary; the repaired Release binary is byte-different, so the
  performance owner decides whether the 8-instance byte-identity evidence
  suffices or the gate is re-run on `pins-r2/tactix-release`.

## B28 + C5 checked gate (campaign 5304, job 1668339)

Audit `logs/gate-1668339-audit.txt`: 66/66 array tasks COMPLETED 0:0
(node017/node018), 66/66 records valid: expected verdicts and exit codes, pin
hashes matched, 34 SAT models verified by `checkmodel`, 32 UNSAT DRAT proofs
verified by drat-trim, scratch deleted on every task. Certificates are
byte-identical between arms on 19/33 ranks and differ on ranks 1, 4, 5, 9, 11,
14, 15, 16, 19, 20, 24, 30, 31, 33.

The T105 claim is corrected: both arms wrote 695,190,050-byte DRAT
certificates with **different** SHA-256 (shipped `95436183d5f3…`, candidate
`1de75ce8048c…`), both verified (drat-trim 3,332.0 s and 3,324.9 s). Equal size
with different content is what a literal-order-only change produces in binary
DRAT; the arms' non-timing statistics differ only in Sweep's own counters. Not
a failure. Total solve wall 1,095.664 s (shipped) versus 1,109.774 s
(candidate) in this single wave, not a timing claim.

## Identity and pins

| Item | Value |
|---|---|
| r0 snapshot | `build/pin-sweep-pose-fuzz-src` = git archive `96e894c` + 5302 `candidate.patch`; fresh Release build reproduced `f180cbc7…` byte-for-byte |
| r0 pins (frozen) | `pins/`: release `f180cbc7…`, debug ASan/UBSan `-O1 -g` `f8b765ed…`, shadow (proof-population oracle) `1a48ef0a…` |
| r2 snapshot | `build/pin-sweep-pose-fuzz-r2-src` = git archive `96e894c` + `candidate-r2.patch` (`src/`, `tests/`) + the untracked files it references + corpus 028-030 |
| r2 pins (accepted) | `pins-r2/`: release `de3d9332…`, debug `c7c536fe…`, shadow `f3354faf…` (`pins-r2.sha256`) |
| Harness | live `tools/fuzz` files unchanged for the whole campaign (`fuzz.py e9639c52…`, `run.sh 3e736db1…`, `slurm.sh b08aa2bb…`, `modelcheck.py f70f0eef…`, `generators.py 489bfdab…`, identical to campaign 4631, so its selftest applies); leg snapshot aggregate `33fdec52…` (30 corpus entries); reach snapshots `harness-r3` `4b7a8e4b…` |
| Oracles | kissat `3aa8e41f…`, drat-trim `f2cb7d45…`, lrat-check `0c9a87d5…`; independent original-CNF model check on every SAT; drat-trim (DRAT legs) or lrat-check (native LRAT legs) on every UNSAT; kissat differential on every verdict; ASan+UBSan `halt_on_error`, debug assertions, crash/io consistency; proof-population oracle on the shadow leg |

## Preparation probe on the r0 pins (job 1668415, 25 physical cores)

- ctest on the ASan build: 71 passed. `fuzz_slurm_snapshot` failed only in
  its `finally` cleanup (`shutil.rmtree` ENOTEMPTY on the network filesystem
  after every assertion passed; the empty residue directory was removed).
  `doc_size` failed on the base commit's documentation budgets (not a solver
  test). `demo3_scripted_default` (an examples API demo) exceeded the 2,400 s
  budget under ASan. `micro_proofs` and `sweep_proofs` skipped because the
  git-archive snapshot carries no checker binaries; job 1668428 replayed both
  on the same ASan build with the live checkers: ok, 44 Sweep derivations
  verified by drat-trim plus lrat-check.
- Corpus replays (27 entries then): the two DefaultPolicy shapes clean;
  random-lrat 7 failures, subst-stress 11, sweep-stress 2, shadow-sweep 2,
  every failure exit -6 (SIGABRT, a debug assertion), plus 026 non-terminating
  under the two Sweep-stress shapes.
- Reach probe: default and random universes clean (16 and 12 gdb cases, all
  oracle-clean); the sweep-stress universe aborted when `check-one` crashed on
  seed 980019 (`--lrat`).

## Diagnosis and attribution

Job 1668429 reproduced every failing composition with stderr and backtraces
(`diag/`, `logs/diag-1668429.log`). Three classes, all debug assertions on the
Eliminate-style dense image (`OccIndex::ordered`, `track_order`) that the
random and stress policies attach for the whole walk on odd seeds so BVE can
run at any step:

- **A** `kernel_replace_large_clause_in_place`: `large_index < occ.size() &&
  occ[large_index] == ref`, inside `substitute_apply_sweep_equivalences`
  (corpus 002/019 under the random and Sweep-stress shapes).
- **B** `kernel_check_invariants`: `physical_binary == ordered_binary`, "dense
  ordered stream lost a physical binary copy", after a canonical Substitute
  step (corpus 008/015 under Substitute stress; reach seed 980019 under Sweep
  stress with `--lrat`, which selects the session image path).
- **C** `kernel_check_invariants`: `physical_large == ordered_large`, after a
  Vivify step (corpus 020 under Sweep stress).

Job 1668430 ran the same compositions on a Debug ASan build of the plain base
`96e894c`: identical assertions, refs, and ids. Bisection over
`f51a988..96e894c` (18 Debug builds, jobs 1668431-1668448, reproductions
`rp002`, `su008drat`, `ss019`, `ss020`, `reach`):

| Range | Result |
|---|---|
| `f51a988` (last passing campaign) | B/C/A cases pass; `ss020` hits an older BVE assertion `garbage == garbage_prefix.size()` on this never-before-run schedule (not reached on the current source, see below) |
| `02ab6f1` "Converge Rank 27 lifecycle mechanisms" (first commit after `f51a988`) | classes B and C appear; the commit also added the two invariants |
| `1148168` "find a removed occurrence in one pass over the mixed stream" | class A appears; the commit added the correspondence assertion |

Root causes, read at the source (`logs/trace-*.log`, TACTIX_TRACE step traces):

- B: canonical Substitute rewrites the stable binary watches in place and
  appends the retired endpoints' mirrors to their representatives
  (`substitute.c:294-363`), and `bulk_clear_dense` kept retired literals'
  binary entries; none of that goes through the kernel add/delete primitives
  that maintain `ordered`, so the attached image kept stale binary entries
  that BVE would read as phantom clauses. Kissat never substitutes under a
  dense image (substitute runs in its sparse probing phase).
- C: `promote_irredundant` in Vivify pushed the promoted clause into the
  `large` mirror only, never into `ordered`.
- A: since `1148168` the kernel derives a removed occurrence's `large` index
  from the count of large entries before it in `ordered`, which is exact only
  while `large` is the large projection of `ordered`; the Sweep session's
  `remove_large` and `collapse_large_to_binary` erased eagerly from `large`
  alone and left `ordered` to the batch's later republish, so a later in-place
  rewrite on the same literal inside the batch read a shifted index.

Under DefaultPolicy the mixed stream is attached only inside the Eliminate
group (SubsumeForward, Strengthen, BVE, Instantiate, BCE, CCE); canonical
Substitute and Vivify never run there, which is why every DefaultPolicy shape
and the checked gate were clean while every free-schedule shape aborted.

## Bounded repair (`repair.patch`, 379 lines including tests)

1. `src/rules/substitute/substitute.cpp`: canonical (non-session)
   `do_substitution` republishes an attached `track_order` image at its
   boundary (`RepublishAttachedMixedStream` calling
   `kernel_rebuild_dense_occurrences`), the representation boundary Congruence
   already closes in `kernel_canonicalize_binary_watches`. Inert under
   DefaultPolicy.
2. `src/kernel/kernel.hpp`, `src/kernel/kernel.cpp`: two kernel primitives,
   `kernel_dense_add_large_occurrence` and
   `kernel_dense_remove_large_occurrence`, own the paired `large`/`ordered`
   updates; `kernel_add_large_clause` and `kernel_replace_large_clause_in_place`
   now call them (the one-pass search moved into the remove primitive).
3. `src/rules/vivify/vivify.cpp`: `promote_irredundant` appends through the
   add primitive.
4. `src/rules/substitute/substitute.cpp`: the Sweep session's `remove_large`
   and `collapse_large_to_binary` erase through the remove primitive;
   `dense_erase_first` is gone.
5. Regressions: `tools/fuzz/corpus/regressions/028-substitute-stale-mixed-
   stream-binaries` (class B, 6 vars/12 clauses, Substitute stress seed 1),
   `029-vivify-promote-outside-mixed-stream` (class C, 21 vars/144 clauses,
   Sweep stress seed 1 with `--lrat`), `030-sweep-session-erase-desyncs-mixed-
   stream` (class A, 94 vars/516 clauses, random policy seed 1 with `--lrat`);
   `tests/test_substitute.cpp::test_canonical_substitution_republishes_
   attached_mixed_stream`.

Validation of the repair before the campaign:

- Reproductions: r1 pins (repairs 1-3) fixed B and C and left A (job 1668453);
  r2 pins (all four) pass all five reproductions (job 1668454). The older BVE
  assertion seen at `f51a988` on `ss020` is not reached on the r2 source (the
  case returns SAT).
- DefaultPolicy neutrality (job 1668456, eight short B28 gate instances, ranks
  1, 2, 15 SAT and 8, 9, 10, 22, 23 UNSAT): `pins-r2/tactix-release` produced
  byte-identical DRAT certificates (SHA-256 and size) and byte-identical
  non-timing `c` statistics to the 5304 candidate arm on 8/8.
- Preparation probe on r2 (job 1668455): ctest 75/76 with `demo3` excluded
  (only `doc_size` failed, as above; the new test passed); corpus 30 entries
  clean in all six shapes; reach probe 44/44 gdb cases oracle-clean; shadow
  smoke 35 verdicts of 40 with no population-oracle abort. Job 1668464
  replayed `micro_proofs` and `sweep_proofs` on the r2 ASan build: ok, 44
  derivations verified.

## Campaign on the repaired pins (r2)

Resource contract met on every generated leg and the reach leg: partition
`cpu-dense-hwulab-q`, 50 physical cores = 100 logical CPUs, 400,000 MiB, 50
harness workers, `08:00:00`, no array, no throttle, no node constraint. Achieved
concurrency: 50 workers per leg on 100 granted logical CPUs; two legs ran at a
time (one per node, memory-bound), the rest queued under SLURM priority.
Preparation and reproduction jobs used less with their reasons recorded in
the job scripts. Corpus allowances were pre-registered from the r2 prep:
entry 026 non-terminating under the two Sweep-stress shapes (1 each), 0
elsewhere; entries 006/011/022 are crash-oracle-only passes as recorded.

| Leg | Job | Elapsed | Verdicts SAT/UNSAT/TIMEOUT | Coverage | Required rules fired | Stress fires |
|---|---|---|---|---|---|---|
| default-drat (DRAT, drat-trim, then `-L` to lrat-check) | 1668465 | 4:15 | 5685 / 4030 / 285 | 9715/10000 (2.9% NT) | Decide 1420, Propagate 9708, ConflictAnalysis 1413, Sweep 2485, SweepPartition 1665, Substitute 2396 | n/a |
| default-lrat (native LRAT) | 1668466 | 4:02 | 5686 / 4018 / 296 | 9704/10000 (3.0%) | 1409, 7467, 1402, 2474, 1666, 2385 | n/a |
| random-lrat (per-case seed) | 1668467 | 5:04 | 5199 / 3551 / 1250 | 8750/10000 (12.5%) | Decide 3859, Propagate 6499, Sweep 2519, SweepPartition 3697, Substitute 4256 | n/a |
| sweep-stress-lrat | 1668468 | 8:49 | 4891 / 3485 / 1624 | 8376/10000 (floor 50) | Decide 2840, Propagate 6087, Sweep 5485, Substitute 4623 | 653,811 |
| subst-stress-lrat | 1668469 | 5:26 | 5195 / 3543 / 1262 | 8738/10000 | Decide 3793, Propagate 6452, Substitute 6039 | 1,208,875 |
| shadow-sweep-drat (population oracle) | 1668470 | 8:55 | 4888 / 3483 / 1629 | 8371/10000 | Decide 2786, Propagate 8310, Sweep 5468, Substitute 4592 | 645,974 |

All six: COMPLETED 0:0, corpus 30/30 clean, 0 generated failures, no
sanitizer or shadow-oracle text, harness snapshot removed by the job. Totals:
60,000 generated cases, 53,654 verdicts (31,544 SAT models independently
checked against the original CNF with kissat agreement; 22,110 UNSAT proofs
checked, DRAT by drat-trim on the two DRAT legs and native LRAT by lrat-check
on the four LRAT legs), 6,346 non-terminating at the 5 s budget (seeds retained
under `artifacts/<leg>/nonterminating`). On default-drat the drat-trim `-L` to
lrat-check cross-check skipped 2,429 degenerate-input cases for the harness's
documented reason (`conversion_crosscheck_unsound`); drat-trim verified them.
Audit: `logs/legs-r2-audit.txt`.

Reach leg (job 1668471, `reach-r3.sbatch`, harness-r3 with the r2 source's
witness lines 1476/1458/1461 for large/dup/pose, `sess` =
`substitute_apply_sweep_equivalences`): 2,300 scanned cases, 44 gdb cases, 0
bounded non-terminations, 44/44 oracle-clean under `check-one`; cases per
witness sess 44, large 40, dup 19, pose 18 against floors 20/15/8/7
(`reach-floors.json`, set from the r0 probe's measured rates); per universe
default 16/15/9/8, random 12/11/5/5, sweep-stress 16/14/5/5. REACH OK. The
default-universe rates equal the r0 probe's, a second sign that DefaultPolicy's
connected substitution is unchanged by the repair. (The r2 prep's own witness
counts are void: harness-r2 still carried the r0 line numbers.)

## Residue and cleanup

- Six leg snapshots `build/pin-fuzz-harness-*` removed by their successful
  jobs (checked by name); pre-existing snapshots from earlier campaigns were
  not touched. No new bundle under `tools/fuzz/failures`. Every job's TMPDIR
  is removed by its trap. The r0 ctest's empty snapshot residue was removed.
- 21 bisection build pairs and the base attribution build removed after use.
- Retained: `pins/` (frozen r0), `pins-r1`, `pins-r2`, `harness/`, `harness-r1`
  to `harness-r3`, `diag/`, `diag-base/`, `diag-trace/`, all job logs, and
  `build/pin-sweep-pose-fuzz-{,r1-,r2-}{src,rel,dbg,shadow}` (the r1 build
  directories are superseded and can be deleted at wrap-up).
- No campaign job remains queued or running.

## Jobs

1668339 (gate, root's), 1668415 (prep r0), 1668428 (proof tests r0), 1668429
(crash diagnosis), 1668430 (base attribution), 1668431-1668448 (bisection),
1668449-1668451 (step traces), 1668452 (gdb operands), 1668453 (r1 repro),
1668454 (r2 repro), 1668455 (prep r2), 1668456 (neutrality), 1668464 (proof
tests r2), 1668465-1668470 (legs), 1668471 (reach).
