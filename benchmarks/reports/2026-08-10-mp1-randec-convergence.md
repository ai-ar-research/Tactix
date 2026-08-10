# `mp1-klieber2017s-0500-023-t12`: random-sequence convergence

**Date:** 2026-08-10

**Fixture:** `tierB/mp1-klieber2017s-0500-023-t12.cnf.xz` (SAT)

**Reference:** local Kissat `8af8e56f174b` plus the diagnostic-only event overlay

## Outcome

The one-fixture divergence loop removed the 300-second Tactix timeout.  The
correctness-checked production run now finishes SAT in 21.64 seconds; Kissat
finishes in 12.20 seconds.  Both complete models verify against the original
CNF.  This also improves on the earlier 31.87-second Tactix baseline, but the
remaining 1.77x time gap and 109,502-versus-64,149 conflict gap remain open.

The portable trajectory is now exact from search entry through conflict 1,020.
At conflict 1,000 both solvers apply Reduce before SwitchMode, retain exactly
678 redundant clauses, and then make the same decisions.  Tactix still carries
120 additional binary clauses from its representation of preprocessing
equivalences, but that difference has no observed trajectory effect through
the current frontier.

## First-divergence sequence

The investigation compared decisions, conflict-clause sets, raw learned-clause
sets, and post-analysis backjump levels.  Clause order was retained in the raw
traces but normalized out when the set and resulting learned clause agreed.

1. After the formula-classification repair, the two streams were exact through
   conflict 550.  The first meaningful mismatch was after conflict 588: Kissat
   selected ranked literal `23`, while Tactix started a random sequence and
   selected `10489`.
2. Kissat increments its decision level before
   `next_random_decision` admits a due sequence only at level at most one.
   Tactix calls its brancher before Decide creates the frame.  Its copied
   `level <= 1` test therefore admitted randomness from a reused level-one
   restart prefix, corresponding to Kissat level two.  The equivalent Tactix
   condition is pre-decision level zero.
3. That repair moved the first mismatch to conflict 591, where both solvers
   correctly began their first random sequence.  Their LCG words were
   identical, but the same word selected different slots: Kissat used a
   population of 30,551, while Tactix used 30,587.
4. Kissat's initial sparse collection calls literal compaction when units are
   present.  `compact.c:21-94` maps every fixed variable onto the first fixed
   representative, so the post-compaction variable population is
   `active + 1`.  Tactix intentionally preserves kernel/external identities,
   but its policy-owned random permutation had not mirrored that compaction.
5. Tactix now retains the first root-fixed variable in tumble order and removes
   the remaining fixed variables from only the random-index permutation.  The
   kernel formula, proof mapping, model reconstruction, and public Decide
   instances are unchanged.  From conflict 591 onward, RNG word, slot,
   external variable, acceptance, phase, and decision all match Kissat.

Neither old behavior was strictly better.  Starting a sequence one decision
level too high is a lost reference gate.  Sampling a larger representation-only
population does not strengthen the formula or decision rule; it merely maps the
same deterministic RNG state to a different branch.

## Other cumulative repairs in this pin

This production binary also contains the preceding source-backed `mp1`
repairs which made the pre-random prefix converge:

- Congruence imports paired binary equivalences before gate closure;
- Sweep removes a merged endpoint from its partition immediately, matching
  Kissat's conditional follow-up state flow;
- preprocessing suppresses saved-phase writes across the whole probing scope;
- saved phases initialize to zero and use the positive initial fallback;
- propagated binary jump reasons use Kissat's `smallclauses=100000` and
  `bigbigfraction=990` classification, while learned-binary assertion remains
  ungated;
- Factor admission uses its actual per-literal material marks; and
- BVE's occurrence limit is a named `DefaultStrategyConfig` value rather than
  a hidden rule-side constant.

The CaDiCaL-style analysis-side trail-reuse default remains off under the
Kissat tie-break.  No temporary tracing hook was left inside a proof rule.

## Evidence

| jobs | conflict window | result |
|---|---:|---|
| `1606165` / `1606166` | 551-600, before repair | exact through conflict 588; then Tactix alone starts randomness |
| `1606167` | 551-600, level repair | exact through conflict 591; common RNG word maps to a different slot |
| `1606169` / `1606166` | 551-600, fixed-representative repair | normalized trajectories identical |
| `1606170` / `1606171` | 601-700 | identical |
| `1606172` / `1606173` | 701-800 | identical |
| `1606174` / `1606175` | 801-900 | identical |
| `1606176` / `1606177` | 901-999 | identical |
| `1606179` / `1606180` | 1000-1020 | common Reduce -> SwitchMode order, 678 survivors, identical trajectory |
| `1606178` | production | Tactix 21.6397 s, Kissat 12.2018 s; both SAT models verified |

The short Tactix trace jobs intentionally time out after collecting their
diagnostic windows; their harness-level `failed verdict/proof/model validation`
message is not a solver verdict.  Complete correctness evidence comes from job
`1606178` and the test suite.

## Correctness and architecture gates

- Production build: 69/69 CTest tests pass, including proof/model checks,
  scheduling, configuration wiring, rule/policy boundaries, documentation,
  and event-trace zero-overhead checks.
- Focused tests pin the pre/post-decision level translation, late tumble
  attachment, and fixed-representative random population.
- Production tracing remains compiled out: the existing zero-overhead test
  checks that event-trace symbols and strings are absent.
- Every strategy number added in this increment lives in
  `DefaultStrategyConfig`; the random-level and one-representative values are
  semantic conditions, not tunable magic numbers.

## Next frontier and reopening conditions

The next pass should stay on this fixture and locate the first normalized
trajectory mismatch after conflict 1,020.  Do not attribute the final runtime
gap to the 120 extra binaries unless a trace shows the first affected
implication or conflict.  The random-sequence repair reopens only if a common
RNG state maps to a different compacted slot/population, a sequence is admitted
above Kissat's level-one boundary, or a later variable-compaction event changes
the two random universes differently.
