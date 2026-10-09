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
mode, `--default-graph-labeling=on`, cpu-dense-hwulab-q at 8 CPUs / 32 GiB).
The release pins were deleted afterwards; each campaign keeps its own hashed
copy.

### Headline rows (5,000 s)

| Row | c2 | base | fix |
|---|---|---|---|
| R, symmetry off | UNSAT 3,857 s, 154,977,663 conflicts | UNSAT 4,234 s, 154,977,663 | **UNSAT 536 s, 21,049,450** |
| adv_gc original, symmetry on | UNSAT 3,865 s, 154,977,663 | UNSAT 4,245 s, 154,977,663 | **UNSAT 537 s, 21,049,450** |
| noL-11-10 | UNKNOWN (100.95M conflicts) | not run | UNKNOWN (100.04M) |
| SC25_Timetable_C_495_E_48 | UNKNOWN (43.8M) | not run | UNKNOWN (45.7M) |

On adv_gc, fix reproduces Kissat 4.0.4 `--fastel=1` exactly and solves 7.2x
faster than c2. Base matches c2's trajectory exactly; it is 380 s slower at
identical counters, which is a c2-to-`163429b44` difference, not this repair.
The UNSAT certificates of the adv_gc rows were not checked: the 1.6 GB fix DSR
and the larger c2/base certificates all exhausted the 1,800 s checker window.
That is a timeout, not a rejection. The repair changes no proof emission, and
the smaller UNSAT rows below verified.

noL-11-10 and Timetable are not closed. Diagnostics (bounded jobs, trace
builds) located a different first divergence on each:

- **noL-11-10** (the Rank401 instance). On Satsuma's residual
  (`0441005e…`), fix with symmetry off matches Kissat 4.0.4 `--fastel=1`
  exactly: 919,409 conflicts, 29.5 s versus 34.8 s, with all 402 boundaries
  equal. With Tactix's own structured symmetry, the census after
  preprocessing is equal to the residual's, but the decision sequence
  differs before the first Probe (12,885 versus 12,492 decisions at
  conflict 3,031). The run then times out at 1,500 s. This is the residual
  search hand-off already isolated in
  `2026-10-07-rank401-residual-seed-causal`, not the mechanism repaired here.
- **Timetable.** On Satsuma's residual (`eda26885…`), Tactix's startup fast
  elimination stops on its typed effort budget (`fastel … 1 budget stops`)
  after 106,395 eliminations. Kissat's fast elimination has no tick budget,
  only per-pivot resolvent and occurrence caps and four rounds; it eliminates
  139,861. The first search-time Probe census then differs (98,004 versus
  131,472 active). Fix with symmetry off solved that residual in 647 s; Kissat
  4.0.4 `--fastel=1` was UNKNOWN at 2,400 s. The budget is a documented
  Tactix design, and changing it was outside this task.

### Search-sensitive and WV2-89 rows (1,200 s)

| Row | c2 | fix | before vs after the repair (trace builds, same source) |
|---|---|---|---|
| mp1-Nb5T15 | SAT 26.8 s | SAT 132.3 s | identical: 3,764,636 conflicts both |
| battleship-14-26-sat | SAT 0.9 s | SAT 2.8 s | before 1,336,190 conflicts / 41.4 s, after 126,026 / 3.3 s |
| veer_axi p23 | UNSAT 10.4 s ✓ | UNSAT 22.4 s ✓ | before 13,634 / 30.9 s, after 13,598 / 27.5 s |
| EDP3-11000 | SAT 59.0 s | SAT 34.3 s | — |
| T105.2.0 | UNSAT 311.5 s | UNSAT 311.6 s | identical conflicts (342,205); both checks timed out at 1,800 s |
| sted5 0x24204-50 | SAT 123.4 s | SAT 125.0 s | identical conflicts (2,333,179) |
| asconhash U19 | UNSAT 108.6 s ✓ | UNSAT 95.2 s ✓ | — |
| noL-11-0 | UNKNOWN | **SAT 95.5 s** | before UNKNOWN at 1,200 s, after SAT 118 s |
| noL-11-8 | SAT 1,131 s | UNKNOWN | before and after both UNKNOWN at 1,200 s |
| ntil-90d-37 | SAT 22.0 s | UNKNOWN | before and after both UNKNOWN; Kissat 4.0.4 `--fastel=1` 756 s |
| ntil-90d-40 | SAT 92.1 s | UNKNOWN | before and after both UNKNOWN; Kissat also UNKNOWN at 1,200 s |
| Break_20_72 | SAT 685.5 s | UNKNOWN | before and after both UNKNOWN; Kissat also UNKNOWN at 1,200 s |
| snw_16_9_CCSpre | UNKNOWN | UNKNOWN | — |

Every SAT model was verified. ✓ marks a verified UNSAT certificate.

Every row that is slower than c2 was attributed, and none of the slowdowns
comes from this repair:

- **Per-pass timing and throughput.** On mp1 and Break, conflicts per second
  are equal across arms, so the extra time is extra conflicts. On ntil-37,
  Vivify takes 12% of the timed-out run against 7% of c2's 22 s solve. No
  pass grew without output.
- **veer_axi.** The extra time is inprocessing: one more search-time
  Eliminate (235,290 against 115,749 eliminations), with 14 s unattributed
  slot time against 4.4 s. Before and after the repair cost the same there.
- **Same-source comparison.** Trace builds of the same source before and
  after the repair are identical (mp1) or both slower than c2 (veer_axi,
  noL-11-8, ntil-37/40, Break). The regressions therefore come from the
  changes between c2 and `163429b44`.
- **Missing statistics.** Two fix-arm timeout records (ntil-40, snw_16_9)
  carry no statistics. A direct SIGTERM check of the base and fix release
  pins on both rows at 1,200 s exited within 0.1 s and printed statistics.

The ntil-90d-37 c2 solve (22 s) is far ahead of Kissat 4.0.4 (756 s). On that
row both Tactix builds diverge from Kissat inside startup Congruence:
1,939 binaries against Kissat's 1,829.

## Not run

- No full B28+C5, WV2-89 or Rank gate. The pairs are one draw per arm.
- No fuzz campaign.
- No base arm in the pair campaigns other than adv_gc and R. Before/after
  attribution elsewhere used trace builds of `6324756c7` with and without the
  repair, through bounded diagnostic jobs.
- No perf sampling. The slower rows are explained by conflict counts at equal
  conflicts per second, or by the c2-to-HEAD difference.
- No proof check completed for the adv_gc and R UNSAT certificates (checker
  window).
- No Kissat SC2024 trace. Kissat 4.0.4 `--fastel=1` was the reference
  throughout.
## Follow-up observation (not changed)

In Kissat's fast elimination, eliminating a pivot marks the literals of its
irredundant clauses as removed (`clause.c:mark_clause_as_garbage`). This
re-arms neighbours that were already attempted in that round, so a later
round can try them again. `docs/implementation-notes.md` records that in
Tactix "attempted pivots stay disabled".

The matched census on R shows no difference here: Tactix's minority rule
declined round two, and Kissat's later rounds eliminated nothing. This report
did not test the difference.
