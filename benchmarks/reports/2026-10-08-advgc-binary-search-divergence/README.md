# adv_gc residual: two Kissat start-of-search scheduling gaps

## Question

Tactix with structured symmetry off needed 154,977,663 conflicts (3,866 s) on
Satsuma's residual R of `adv_gc_n300_k12_UNSAT_s42`, against Kissat 4.0.4
`--fastel=1` at 21,049,450 conflicts. Preprocessing counts were close. The
question was whether a Tactix mechanism caused the 7x gap.

## Method

R is Satsuma's competition-command output: the original formula plus 19,800
AMO binaries (sha256 `1f7a3aee…ffaff`; 6,900 variables; 84,060 clauses, of
which 83,760 are binary). It is cached as
`benchmarks/cache/advgc-residual/adv_gc_n300_k12_satsuma_residual.cnf.xz`.

The Kissat side used the event-trace build of 4.0.4 (`95286fbe…`) with
`-s --fastel=1`. The Tactix side used event-trace builds of `6324756c7` with
`--default-graph-labeling=on --default-structured-symmetry=off` and no proof
output. Each run produced two traces:

- hashed learned clauses (`*_EVENT_TRACE_LEARNED_ONLY` plus
  `TRAJECTORY_ONLY`) for conflicts 0 to 400,000;
- the standalone inprocessing-boundary journal
  (`*_INPROCESS_BOUNDARY_TRACE`), which records the conflict, decision and
  active/irredundant/redundant/binary census at every pass boundary.

All runs were bounded SLURM jobs. The scripts and raw traces are in
`benchmarks/probes/scratch/advgc-div/`. That directory is scratch and was
deleted after this report.

## Finding 1: the first search-time Probe ran 5 conflicts late

The startup pipelines were identical, including the census after Factor and
after fast elimination: 6,996 active variables, 300 irredundant clauses and
65,351 binary clauses. The first 2,508 learned clauses (conflicts 1 to 2,517)
were also identical.

Kissat then entered its first search-time Probe at conflict 2,517. Tactix
entered it at conflict 2,522, and every later learned clause differed.

The cause is the population that scales the deadline.
`search.c:start_search` calls `kissat_init_limits` after `kissat_preprocess`
(startup Probe, then fast elimination) and late Lucky.
`INIT_CONFLICT_LIMIT(probe, true)` therefore uses that final `BINIRR_CLAUSES`:

- Kissat: scale(100) at 65,651 clauses = 2,515. Probe is due at the first
  idle point after conflict 2,515, and conflicts 2,515 to 2,517 had no
  decisions in between.
- Tactix: the deadline was set when the startup Probe round closed. That was
  after Factor and before fast elimination, at 60,095 clauses, giving
  scale(100) = 2,522.

**Repair:** `DefaultInprocessScheduler::initialize_search_limits` now
re-derives the Probe deadline next to the Eliminate deadline. This is n=1,
i.e. probeinit, at the start-search population. A scheduler that ran no
startup Probe round keeps its existing startup invitation. Ledger:
`ORDER-ELIM-INIT-1` (sub-id `ORDER-PROBE-INIT-1`).

## Finding 2: the first search-time Eliminate skipped 849 variables

With repair 1, learned clauses matched through conflict 12,816. The boundary
journal showed the first divergence earlier, at the first search-time
Eliminate (conflict 12,578):

- Kissat went from 11,014 to 10,433 active variables;
- Tactix went from 11,014 to 10,430.

The BVE heap trace (`*_EVENT_TRACE_BVE_HEAP_ONLY`) at that conflict showed
`bve_schedule scheduled=11014` for Kissat and `scheduled=10165` for Tactix.
All 748 candidates that Kissat tried and Tactix never tried are factor
extension variables, with external ids above 6,900.

`fastel.c:kissat_fast_variable_elimination` clears each tried pivot's
`eliminate` flag. It then sets the flag on every variable before returning
(`fastel.c:917-918`), so `schedule_variables` later schedules every active
variable. Tactix's fast elimination clears the retry bit of each kept
candidate in `bve_attempt` and never re-arms it. Only variables whose clauses
changed later became schedulable again; the untouched extension variables
were lost.

**Repair:** `run_initial_fast_elimination` now re-arms every retry bit at exit
without counting a mark. Kissat also does not increment `variables_eliminate`
here. Ledger: `ORDER-ELIM-INIT-1`
(sub-id `FASTEL-REARM-1`).

## Result: Tactix now reproduces Kissat's run

With both repairs, the trace build (`323b5130…`) was run to completion
alongside Kissat (job `1758783`):

| | Kissat 4.0.4 `--fastel=1` | Tactix before | Tactix after |
|---|---:|---:|---:|
| conflicts | 21,049,450 | 154,977,663 | **21,049,450** |
| decisions | 34,495,500 | 253,666,027 | **34,495,500** |

All 397,751 learned clauses in conflicts 0 to 400,000 are identical. The two
boundary journals match on all 2,056 boundaries up to conflict 21,048,963,
including every census column.

The one later difference is reporting only. Inside the final Congruence pass
the irredundant count differs (13,402 versus 13,428); both solvers agree again
at the next boundary. Both runs end in UNSAT at identical counters.

The 7x gap therefore had no binary-specific search cause. The two
start-of-search scheduling gaps shifted the trajectory once, and the binary-rich
residual amplified that shift into 7x more conflicts. The suspects named in
the brief did not produce the gap:

- extension-variable decision share;
- the binary passes: transitive, backbone, eagersub, binprop, OTFS, chrono;
- the mode-switch count.

Their counter differences were downstream of the divergence.

Both gaps affect every formula on which startup fast elimination changes the
clause population or keeps a candidate. Fast elimination is on by default.

## Pair validation

Pins, built from `163429b44` by `benchmarks/runs/20261008-advgcdiv-build`:

- fix = `163429b44` plus the repair: `build/pin-advgcdiv-fix-rel`, `b91abd16…`;
- base = `163429b44` alone: `build/pin-advgcdiv-base-rel`, `18ab0f4d…`;
- c2 = `build/pin-candidate-c2-rel`, `bec9eede…`.

Correctness checks from the same job, all on the fix source:

- Debug full CTest: 356/356 passed (the base source also passed 356/356);
- ASan/UBSan focused CTest: 12/12 passed, including `scheduling`,
  `bve_invocation` and `micro_proofs`.

Every pair runs its arms sequentially on one CPU through
`benchmarks/runs/20261008-advgcdiv-pair-*` (`runtime_task.py`, automatic proof
mode, `--default-graph-labeling=on`).

Results are pending (jobs 1759009-1759025).

## Not run

Recorded with the pair results.

## Follow-up observation (not changed)

In Kissat's fast elimination, eliminating a pivot marks the literals of its
irredundant clauses as removed (`clause.c:mark_clause_as_garbage`). This
re-arms neighbours that were already attempted in that round, so a later
round can try them again. `docs/implementation-notes.md` records that in
Tactix "attempted pivots stay disabled".

The matched census on R shows no difference here: Tactix's minority rule
declined round two, and Kissat's later rounds eliminated nothing. This report
did not test the difference.
