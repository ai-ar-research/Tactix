# Cross-solver semantic divergence gate

**Date:** 2026-08-09

**Tactix base:** `b4d8add` plus the diagnostic worktree

**Kissat source:** `8af8e56f174b`

**SLURM validation:** `1605862` (schema smoke test), `1605863` (analysis-tail refinement)

## Outcome

Tactix and a diagnostic-only Kissat overlay now emit one semantic TSV schema
without enabling Kissat's general logging. The comparator reports the first
portable event/state/trajectory divergence with bounded context and writes a
JSON record. With `--require-explained`, a divergent record is rejected unless
it carries a non-unknown disposition, a cause, and at least one evidence
handle.

Ordinary Tactix builds use `TACTIX_ENABLE_EVENT_TRACE=OFF`. The Release binary
contains neither an `EventTrace` symbol nor the trace schema/event strings;
only a build configured with `TACTIX_ENABLE_EVENT_TRACE=ON` links the
implementation. Binary SHA-256 values for this validation were:

- production Tactix: `e762eac8…de524`;
- diagnostic Tactix: `5fd2b8ab…3faa`;
- diagnostic Kissat: `3c91346c…426b4`.

## Correctness gate

Both solvers returned UNSAT (20) on `tr-n170-s053`; both DRAT proofs verified
with `drat-trim`. Both returned SAT (10) on `tr-n220-s013`; independent model
checking satisfied all 937 clauses with all 220 variables assigned. Thus the
diagnostic overlay did not merely produce traces; each traced result passed the
normal verdict oracle.

## Explained first divergences

### `tr-n170-s053`: accepted reference choice

The streams are identical through 27 decisions and the first conflict. At
semantic record 37, `analysis_end` is level 26 in Tactix and level 18 in
Kissat. Tactix's accepted `SP-CHRONO-1` CaDiCaL choice applies
analysis-side trail reuse; Kissat's `learn.c` has no counterpart and uses the
ordinary jump level. The explained JSON disposition is `accepted`, with source
handles to the permanent ledger and both call sites.

### `tr-n220-s013`: actionable ordering gap

The initial top-32 queues first differ by the order of variables 159 and 79;
the detailed streams remain aligned until decision 25, where Tactix chooses
159 and Kissat chooses 79. Source reading localizes the cause before search:
Tactix sorts a clause before recording first activation, while Kissat activates
the surviving clause in original file-literal order. This reopens the formerly
closed `SP-DECIDE-2` row as `ORDER-TUMBLE-1`. Its JSON disposition is
`actionable`.

## Reproducer and next gate

`benchmarks/probes/cross-solver-semantic-trace.sh` owns the fixed two-fixture
run and its proof/model checks. The reproducible Kissat overlay is
`tools/solvers/variants/kissat-event-trace/`; the comparator is
`tools/bench/compare_event_trace.py`.

Close `ORDER-TUMBLE-1` before cumulative Lucky pricing: preserve original
literal order for first activation while retaining sorted clause storage, then
require exact initial top-32 and decision-prefix identity on both fixtures,
proof/model cleanliness, and a Tier-A cumulative comparison.
