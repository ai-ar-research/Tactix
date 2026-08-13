# S2a Factor selection boundary

**Status:** landed and frozen. Current semantics and guards are owned by
`docs/calculus.md` E2 and `docs/implementation-notes.md` §1/§5. Companion
evidence: `docs/design/e2-factor-research.md` and
`benchmarks/reports/2026-07-20-tierB-vs-kissat.md`.

## Durable boundary

Factor owns the proof-bearing operation: construct one maximal-delta grid from
a seed, introduce the fresh variable, emit dividers and quotients, delete the
grid last, and maintain proof/model state. Strategy owns which legal seed to
try, its priority, tied-factor scoring, admission bound, and scheduling effort.
These choices cross the rule-owned `FactorSelection` interface through the
DefaultPolicy `FactorMatchPolicy` adapter; they do not duplicate the mutation.

The public N1 instance is a seed literal. `factor_instances` enumerates every
unassigned, unretired seed independently of DefaultPolicy scoring or touch
filters, and `factor_apply_instance` performs at most one factoring for that
seed. The grid grown from a seed is the rule's deterministic construction, not
an opaque variable-length API payload. This replaces the earlier, incorrect
claim that policy swappability alone established N1.

## Shipped strategy and constraints

- Default seed and tied-candidate scores reproduce the reference watch-based
  ordering; `factor_bound=0` retains the positive-gain floor.
- Factor may snapshot the BVE bound at a posed scheduling boundary, but the
  snapshot only advises admission and never narrows the public seed surface.
- `factor_size`, candidate rounds, composite remainder scope, and effort are
  named controls. They change rule-internal construction or strategy, not the
  proof primitive.
- The matcher reads kernel state and returns scores/admission only. It does not
  add or delete clauses, emit proof, or mutate reconstruction state.
- Tests pin the N1 seed surface, identity/default trajectories, policy-bound
  handoff, and DRAT/LRAT correctness under arbitrary legal schedules.

Historical matcher alternatives and mp1 attribution probes are intentionally
left to their frozen reports and Git history; they are not live contracts.
