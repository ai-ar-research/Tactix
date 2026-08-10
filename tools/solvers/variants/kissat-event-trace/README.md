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

`KISSAT_REDUCE_TRACE=/path/reduce.tsv` additionally records the pre-aging
clause population, tier/used/reason eligibility, sorted candidate rank, and
victim cutoff for every Reduce. The matching Tactix diagnostic variable is
`TACTIX_REDUCE_TRACE`; compare the files with
`tools/bench/compare_reduce_trace.py`.

This binary is for short SLURM diagnosis only.  Benchmark reference arms must
continue to use the untouched pinned Kissat binary.
