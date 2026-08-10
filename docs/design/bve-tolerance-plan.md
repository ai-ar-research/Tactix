# BVE tolerance experiments — frozen compact record

**Status:** historical design snapshot; superseded by the live scheduling ledger.

## Question and outcome

This pass tested whether changing BVE's clause-growth tolerance could recover
performance independently of the rest of elimination scheduling. The experiments
showed that tolerance, round cadence, backward processing, and Factor coupling
cannot be interpreted as independent knobs. The old plan's staged prescriptions
and present-tense tasks are therefore retired.

## Durable record

- BVE is the calculus E1 rule; its legal effect and reconstruction witness are
  owned by `docs/calculus.md`.
- Default bounds, effort, gate extraction, backward processing, and Factor
  coupling are owned by `docs/implementation-notes.md` §13.
- Measurements remain frozen in `benchmarks/reports/` and
  `benchmarks/probes/`; historical log references may continue to cite this
  file as the experiment's design context.
- No live task is owned here. Reopening requires a named ledger item, a
  reachability counter, proof/model gates, and a cumulative comparison.

