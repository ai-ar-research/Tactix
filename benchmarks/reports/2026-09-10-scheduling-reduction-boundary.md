# Same-conflict reduction boundary: open parity item, not a measured bottleneck

## Decision

Register `SP-SLOT-REDUCE-EDGE-1` as open. Do not confuse CaDiCaL's existing
Probe reduction-count coupling (`SP-SLOT-6`) with Kissat's same-conflict
exclusion. Do not treat this boundary difference as an explanation of the
Rank31 slowdown: it never occurred in the checked full7030 Rank31 phase trace.
No solver implementation, default, pin or campaign script changed.

The primary development priority remains the [complete inprocessing data-path
cost](2026-09-10-eliminate-lifecycle-cost.md), not a sequence of small fixes.
That structural BVE prototype still requires explicit approval; this diagnostic
does not authorize it or reduce its validation obligations.

## Evidence

[7085 summary](../runs/20260910-7085-schedule-boundary/summary.json) binds the
entire7030 source manifest, the linked immutable Release objects, actual
reference sources/headers, fixture sources, commands, executables and outputs.
The candidate fixture calls `DefaultInprocessScheduler::want_phase` in those
objects. The native fixture compiles actual `probe.c` and `eliminate.c` with
function sections, discarding unrelated functions at link time. Both retain
their Release `NDEBUG` layouts. No reference function body is copied into a
test and no real-instance solver is executed.

Each arm evaluates24 posed states: conflicts99/100/101 against deadline100,
same-conflict versus earlier Reduce, new versus absent elimination material,
and changed versus unchanged reduction count since the preceding Probe.
Both phase predicates are checked:96 actual-code results in total.

- With a changed reduction count, new material, and conflicts101, Tactix
  admits both phases even at the same-conflict reduction boundary; Kissat
  declines both. With Reduce at100 instead, both admit both.
- Probe's strict deadline and Eliminate's inclusive deadline agree away from
  that boundary when the other invitations are satisfied.
- The existing CaDiCaL-style Probe watermark independently suppresses Tactix
  when the reduction count is unchanged. It is not the missing Kissat check.

These are predicate fixtures, not proof that every Cartesian-product state is
reachable under the default clocks. Source inspection of the7030 outer loop
also finds no substitute same-conflict guard: Reduce updates its limit and
continues; the loop can subsequently consult phase admission without another
conflict. Reach frequency and performance effect remain unproven.

The hash-bound full Rank31 macro trace from7064 contains96 phase starts and
zero at the conflict count of the latest preceding `reduce_end`. Both7064
proofs were already checked/clean. This excludes the boundary as an executed
phase-entry cause in that trace, not on every benchmark or policy pose.

## Validation and handoff

The new fixed fixtures cover the uncovered invitation boundary without changing
solver behavior. Existing aggregate benchmark/SLURM and documentation checks
remain the wrap-up bar, recorded by successor audit7086. No new fuzz campaign
is needed for diagnostic-only work; current-source fuzz remains required before
candidate adoption and is not available. Proof checks continue asynchronously;
pending results remain pending, not accepted. No material deletion or denied
push/transfer retry. Andrew's new TeX/PDF is local and unsent.
