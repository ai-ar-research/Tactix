# `mp1-klieber2017s-0500-023-t12`: Probe/watch convergence

**Date:** 2026-08-10

## Localized frontier

The normalized Tactix/Kissat trajectory remains exact through conflict 2504.
Both solvers enter the first search-time Probe at conflict 2501 from the same
decision/conflict/learning trajectory.  The first semantic trace difference is
the raw learned clause at conflict 2505.

The differing clause is now explained one edge earlier.  After Probe, both
solvers imply the same literals but sometimes retain different valid reasons.
At conflict 2503, for example, Kissat records
`(-12435,-10193,-10192)` as the reason for `-12435`; Tactix records
`(-12435,-7821,-3491,-3489,-7)`.  Conflict 2505 is the first analysis that
exposes this reason choice: Kissat retains `-10193`, while Tactix retains
`-3491,-3489`.  This is a localized antecedent-order divergence, not an
unexplained branching or conflict-selection divergence.

## One-arm exclusions

| arm | Probe output | first mismatch | production result |
|---|---|---|---|
| shipped | HBR adds 32 binaries; HyperTernary adds 1,762 redundant clauses | conflict 2505 | 21.64 s SAT, verified |
| `-HyperTernaryRes` | removes the 1,762 clauses | unchanged | 54.35 s SAT, verified |
| `-HyperBinaryRes` | removes the 32 binaries | unchanged | 102.09 s SAT, verified |

Neither extra rule causes the first mismatch, and each is strongly beneficial
on this fixture.  They remain enabled.  These arms do not erase the separate
cross-fixture evidence against unbounded hyper-resolution; they only reject it
as the root cause at this frontier.

## Sparse reconnect correction

Kissat's `kissat_watch_large_clauses` reselects the two watched literals under
the current root assignment before reconnecting clauses in arena order.
Tactix's phase-exit `kernel_rebuild_large_watches` reconnected the old pair.
The shared Kissat-style selector is now used at both sparse-rebuild sites, with
a focused test showing two root-false front literals displaced by unassigned
literals.

This correction is neutral on the fixture: job `1606203` has the same
normalized trajectory through conflict 2520, and correctness-checked job
`1606204` finishes in 21.73 s (Kissat 12.20 s).  The next wire to examine is
Kissat Vivify's persistent occurrence-ranked permutation of candidate clause
literals.  Tactix currently uses that order only in a temporary probe vector,
so its phase-exit watch rebuild cannot inherit the same clause/watch order.

## Evidence

- Default trace: jobs `1606197` / `1606198`.
- `-HyperTernaryRes`: trace `1606199`, production `1606200`.
- `-HyperBinaryRes`: trace `1606201`, production `1606202`.
- Sparse reconnect repair: trace `1606203`, production `1606204`.
- All complete production runs are SAT with verified models and proofs.  The
  short trace runs intentionally time out after collecting diagnostic data.
