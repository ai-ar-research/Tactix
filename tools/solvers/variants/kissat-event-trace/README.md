# Kissat semantic-event diagnostic variant

This is a reproducible overlay for the locally pinned Kissat source
(`8af8e56f174b`).  It adds only disabled-by-default diagnostic output; it does
not change options, scheduling, or solver decisions.

Build from the repository root:

```sh
tools/solvers/variants/kissat-event-trace/build.sh
```

The script refuses to overwrite an existing generated tree.  The resulting
binary is `tools/solvers/variants/kissat-event-trace/worktree/build/kissat`.
Set `KISSAT_EVENT_TRACE=/path/trace.tsv` when running it.  The format is the
same `tactix-event-trace` version 1 consumed by
`tools/bench/compare_event_trace.py --mode semantic`.

`KISSAT_EVENT_TRACE_CONFLICT_FROM` and
`KISSAT_EVENT_TRACE_CONFLICT_TO` restrict detailed decision, propagation, and
analysis records to an inclusive conflict window while retaining lifecycle
records. Tactix exposes the matching `TACTIX_EVENT_TRACE_CONFLICT_FROM/TO`
controls, and `benchmarks/probes/cross-solver-semantic-trace.sh` sets both from
`CROSS_TRACE_CONFLICT_FROM/TO`.
In heap-vars-only mode, selected heap insert/remove/pop and root-assignment
lifecycle records also honor this window, permitting a bounded follow-up once
a scalar checkpoint locates the relevant conflict.

`KISSAT_REDUCE_TRACE=/path/reduce.tsv` additionally records the pre-aging
clause population, tier/used/reason eligibility, sorted candidate rank, and
victim cutoff for every Reduce. The matching Tactix diagnostic variable is
`TACTIX_REDUCE_TRACE`; compare the files with
`tools/bench/compare_reduce_trace.py`.
`KISSAT_REDUCE_TRACE_CONFLICT_FROM/TO` and the corresponding Tactix variables
bound this heavier clause-population stream; `p4_cvg.py` maps its requested
conflict window to both event and reduction traces.

`KISSAT_INPROCESS_BOUNDARY_TRACE=/path/boundaries.tsv` writes only native
snapshot-class `*_begin`/`*_end` macro boundaries without opening the global
trace. `SAT_INPROCESS_BOUNDARY_CONFLICT_FROM` suppresses earlier rows. The
matching Tactix variable is `TACTIX_INPROCESS_BOUNDARY_TRACE`.

The standalone BVE schedule observers accept
`SAT_BVE_SCHEDULE_ORDINAL=N` (zero-based among schedules at the selected
conflict), allowing a later Eliminate round to be fingerprinted without
opening the global trace.

For a coarse CDCL bracket, `SAT_SEARCH_CHECKPOINT_INTERVAL=N` and
`SAT_SEARCH_CHECKPOINT_CONFLICT_FROM=C` add only periodic scalar rows to the
standalone macro/boundary journals.  The observer is called from the search
loop but performs no formula, watch, heap, or variable scan.  Use the existing
macro/boundary exit selectors to stop at the next selected outer boundary.
Set `SAT_INPROCESS_SCORE_EXTERNAL=V` and the matching Tactix/Kissat score-trace
path to attach one variable's O(1) heap score, position, value, and active bit;
`SAT_INPROCESS_SCORE_CONFLICT_FROM=C` suppresses its earlier macro rows.
For a single physical watch-list snapshot at an existing macro boundary, set
`SAT_MACRO_WATCH_EVENT`, `SAT_MACRO_WATCH_CONFLICT`, and
`SAT_MACRO_WATCH_EXTERNAL` with `TACTIX_MACRO_WATCH_TRACE` or
`KISSAT_MACRO_WATCH_TRACE`. `SAT_MACRO_WATCH_ORDINAL` selects a later match
when the event repeats. Only that selected list is scanned, once.
For coarse within-phase localization, set the same external and mandatory
conflict selectors with `TACTIX_MACRO_WATCH_SERIES_TRACE` or
`KISSAT_MACRO_WATCH_SERIES_TRACE`; this emits only total and large-watch counts
at existing snapshot-class phase boundaries.
`SAT_EVENT_TRACE_FORMULA_SET_ORDINAL` similarly selects a later repeated match
for the existing bounded formula-set snapshot.
`SAT_EVENT_TRACE_FORMULA_SET_HASH` restricts that snapshot to one canonical
clause hash, and `SAT_EVENT_TRACE_FORMULA_SET_POSE` appends physical literal
order. `SAT_EVENT_TRACE_FORMULA_SET_META` appends glue, Vivify priority,
searched/used counters, reason state, and the in-place-shrink flag.
For selected occurrence counts without a global event trace, combine
`SAT_VIVIFY_COUNTS_TRACE_CONFLICT` and `SAT_VIVIFY_COUNTS_TRACE_LITERALS` with
`TACTIX_VIVIFY_COUNTS_TRACE` or `KISSAT_VIVIFY_COUNTS_TRACE`.
`SAT_VIVIFY_COUNTS_TRACE_CLAUSE_HASH` additionally appends one selected live
clause's size, redundancy, glue, priority, searched hint, and physical pose.

`KISSAT_BVE_CANDIDATE_TRACE=/path/candidate.tsv` and the matching Tactix
variable record only a selected candidate's begin/end flag state. Select with
`SAT_BVE_CANDIDATE_CONFLICT` and `SAT_BVE_CANDIDATE_EXTERNAL`; the optional
`SAT_BVE_CANDIDATE_EXIT_AFTER_END` stops after that candidate. By default the
exit stage is `end`; `SAT_BVE_CANDIDATE_EXIT_STAGE=postflush` retains the one
additional row after root propagation and connected-unit flushing.

The Tactix-only follow-up `TACTIX_BVE_RETRY_TRACE` can arm at that selected
candidate end and report the first later false-to-true retry transition,
including its general mutation source. It uses `SAT_BVE_RETRY_CONFLICT`,
`SAT_BVE_RETRY_EXTERNAL`, and optional `SAT_BVE_RETRY_EXIT_AFTER_MARK`.

For a late Factor seed, set `SAT_EVENT_TRACE_FACTOR_SEED_ONLY=1` with
`TACTIX_EVENT_TRACE_FACTOR_SEED=<external-literal>`. Tactix then rejects
unrelated search and heap events before emission while retaining that seed's
remainder/candidate/fresh rows and a matched Factor endpoint.

`SAT_EVENT_TRACE_FACTOR_METER=1` appends call-local ticks to bounded
`factor_fresh` rows. `SAT_EVENT_TRACE_FACTOR_METER_COMPONENTS=1` additionally
appends cumulative heap-pop, first-factor, next-factor, and matching counters.
After coarse localization, `KISSAT_FACTOR_MATCH_TRACE` (or the matching Tactix
variable) records only a pair selected by `SAT_FACTOR_MATCH_PAIR=seed,next` and
`SAT_FACTOR_MATCH_CONFLICT`, including each large remainder's scan degree and
charged visit count.

This binary is for short SLURM diagnosis only.  Benchmark reference arms must
continue to use the untouched pinned Kissat binary.
