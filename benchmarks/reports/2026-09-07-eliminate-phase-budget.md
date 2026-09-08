# Eliminate's phase resolution boundary

## Outcome

Isolated candidate 5450 repairs a missing general scheduling stop, confirmed
by a real ledger and an actual old/new unit fixture. Both independent Release
builds pass 81/81 tests; trace passes 80/80. The Rank-37 causal prefix extends
from 7,504,348 to 7,918,162 matching decision rows. No runtime gain is claimed
for this repair yet: full Rank 1–40 and B28+C5 are queued.

Production stays `c2a95a6`, checked ratio 1.2787518x Kissat. Rephase's full
view is now 80/80 checked and clean, **1.202033658x**; phase-entry remains
1.205579786x with 77/80 checked at this checkpoint. Neither meets 1.01x.

## Causal evidence and implementation

[5436 localization](../runs/20260907-5436-rank37-inprocess-prefix/boundary-localization.json)
established equal first-round Strengthen/BVE counts at conflict 29761. Tactix
then executed another Strengthen round, producing 21 extra units, before the
first decision split at 29767. Extra simplification alone was not a defect.

[5449's validated ledger](../runs/20260907-5449-rank37-eliminate-limit/summary.json)
identifies the missing mechanism. BVE's shared ceiling was 12587131; the last
rejected pivot attempt, external variable 102635, advanced the resolution meter
from 12587083 to 12587157. The scheduler nevertheless offered outer round 1.
All 354525 selected ledger records belong to the same conflict/ceiling; pin,
trace, controlled stop and scratch cleanup were checked. Partial certificates
were hashed and deleted, not treated as checked complete proofs.

Kissat `eliminate.c::eliminate_variables` checks the strict resolution ceiling
after round collection and before optional next-round forward subsumption.
CaDiCaL `elim.cpp::elim` stops an incomplete elimination round before its next
subsumption/block/cover step. Candidate 5450 adds a private phase-local latch
after BVE's consistent round cleanup and consults it at the outer boundary.
It preserves strict `>` (not `>=`), mandatory current-round and phase-exit
cleanup, disabled/unadmitted entries and fresh-phase ceiling derivation.
Heap emptiness and progress are not substitutes for this meter comparison.

This is a policy repair: no abstract rule, public guard/action surface, proof
delta, reconstruction, global primitive, effort constant or formula-dependent
selector changes. The owning disposition is `SP-ELIM-RESOLUTION-LIMIT-1`.
[5450 authority](../runs/20260907-5450-eliminate-limit/manifest.json) owns source,
patches, logs and full hashes. It descends from replacement cursor 5445 and
excludes independent metadata 5426. Release 5450/5452 share SHA-256
`c7f57cbcb92f2611e8afd451e13ec1203e67e2f5fb7b937cf5a2e8df3e1eb611`;
trace 5451 is `97f52c5235e3db3affa171290644ba4bb7a6e4838f1e9c3cebd2515222d1f304`.

## Unit fixture and failed expectation

The first fixture incorrectly expected only one eliminated variable at exact
equality. Development paused for the required bounded Claude review, now
terminal **PASS**: pure-variable eliminations can continue for zero additional
resolutions. This was a test expectation error, not a solver defect. Claude
changed only the new fixture and passed 49/49 predecessor in-process unit
executables; the deliberately compiled future expectation failed only at the
intended round-count assertion. Original failure logs remain in 5450's logs.
The terminal response is
`benchmarks/probes/scratch/claude-eliminate-fixture-5450-response.json`.

The corrected fixture uses pivot 5 in `(5∨1),(5∨2),(¬5∨3),(¬5∨4)` and its four
real counted resolvents. Twelve scenarios cross allowance 3/4/5 with Strengthen
off/on and BVE off/on, then invoke a fresh second phase. It checks strict
exhaustion versus equality/remaining allowance, free pure eliminations,
occasions/admissions, clause state, fresh ceiling, root fixpoint, disconnected
dense cleanup, public guards and invariants. The old exhausted case offers two
rounds; the repaired case offers one. Equality remains unchanged.

Release fixed suites pass 81/81 in 6.96 and 9.54 seconds; trace 80/80 in 7.10.
Unchanged production passes 77/77 in 5.32 seconds. These are fixed tests, not
new fuzz claims. During the fixture review Claude removed only its newly
generated 121-byte `ctest -N` temporary log/directory from the predecessor pin;
immutable predecessor contents were unchanged. No user data was removed.

## Repaired prefix and next owner

[5453 summary](../runs/20260907-5453-rank37-eliminate-limit-prefix/summary.json)
validates two clean proof-on stops at 100000 conflicts. The extra conflict-29761
round is absent. Complete streams agree for 7,918,162 rows; the next decision
split is conflict 32073, ordinary decision 7849145, level 231: Tactix literal
41073 versus Kissat 149472. Full prefixes are not identical.

[Next boundary](../runs/20260907-5453-rank37-eliminate-limit-prefix/boundary-localization.json)
is the second Factor call at 32073. Both enter with active 148610, irredundant
593188, redundant 6620 and binary 181122. Tactix exits active 148613/binary
181013; Kissat exits 148618/180960. This selects Factor's seed/grid/material or
effort lifecycle for the next diagnostic; counts do not prove physical-state
identity or that a particular algorithm is weaker. No Factor edit was made.

## Other checks revisited

- [5437 binary multiset summary](../runs/20260907-5437-rank35-sweep-binary-boundary/summary.json):
  all three stops are clean and their prefixes agree. The first three selected
  snapshots match exactly; at the second Sweep end Kissat has one extra copy
  each of `(-112752,112742)` and `(-112742,112752)`. Unique sets agree. Both
  Tactix candidates have identical multisets throughout. This closes the
  missing-unique-clause hypothesis, not all possible physical ordering effects.
- [5440 solver checkpoint](../runs/20260907-5440-vivify-max-rank35-runtime/solver-checkpoint.json):
  three complete same-statistics/same-proof-byte pairs give median 0.995068538x
  predecessor, range 0.988733698–0.997473130. Both arm time spreads are below
  2%. This is a small roughly 0.5% exact-work gain, not aggregate closure;
  proofs were 0/6 checked at the frozen snapshot and remain separate.
- [5429 final gate](../runs/20260907-5429-watch-truncate-BC-gate/summary.json):
  66/66 checked and clean, no solve loss/timeout; combined candidate/production
  ratio 0.924071528. Metadata's three timing pairs were already checked.
- [5423 final view](../runs/20260907-5423-rephase-root-ranks1-40/summary.json):
  both solve 40/40 (22 SAT/18 UNSAT), all 80 checked and clean; totals
  8103.802/6741.743 seconds. This replaces provisional status, not the timing.

## Pending work and checkpoint constraints

Full-width jobs 1670292 (5454 Rank 1–40) and 1670293 (5455 B28+C5) use the
unchanged immutable one-solver/12-CPU/48000-MiB contract, no node constraint or
throttle. 1670291 (5453) is terminal. Cursor 5447/5448 remain queued; prior
proof tails continue. Compare all requested ranks, work and memory; do not
select favorable trajectories or require exact work after a policy repair.

Background Claude fuzz owner session 30094 still tests source 5438. Only its
terminal response is polled; no fuzz job or artifact was inspected. Neither
5445 nor 5450 is covered. Include both in the next periodic checkpoint; no
candidate is promoted. A reported correctness failure interrupts dependent
work; normal development continues while proofs and fuzz remain pending.

Living documents remain within required budgets. The broad progress checker
would inspect independently owned fuzz resources, so citation-only checks and
explicit non-fuzz job/pin audits replace that scan, with the limitation retained.
The wrap-up skill requires an Andrew TeX/PDF and local coherent checkpoint.
Push and separate future solve/check jobs remain unapproved and unattempted.
