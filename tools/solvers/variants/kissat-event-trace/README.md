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

This binary is for short SLURM diagnosis only.  Benchmark reference arms must
continue to use the untouched pinned Kissat binary.
