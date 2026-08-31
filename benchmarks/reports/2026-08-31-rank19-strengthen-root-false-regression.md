# Rank 19 regression localization: Strengthen root-false lifecycle

Date: 2026-08-31

## Outcome

Rank 19's 168.543-to-452.403-second regression is localized to the general
Strengthen repair that removed Tactix's non-reference whole-arena root-false
flush. Pinned Kissat's `forward.c` does not perform that flush: it deletes
root-satisfied irredundant candidates but retains root-false literals
physically unless the particular clause is later strengthened. The repair is
therefore retained. The prior fast behavior was a compensating simplifier,
not a reference lifecycle to restore.

The result exonerates the Rank-34 Congruence occurrence repair, Factor mark
state, BVE physical-binary gate input, BVE occurrence compaction, immediate BVE
unit flushing, embedded-definition reset, second-round occurrence repair,
inline-H publication, and TransitiveReduce failed-source units. No source edit,
rollback option, formula/rank selector, or production gate was introduced.

## Frozen production-binary ladder

Campaign `20260831-3640-tierd-rank19-repair-ladder`, Slurm job `1657604`, runs
six already-immutable Release binaries spanning the 9,305.953-second candidate
through the current checkpoint. Every task uses the same Rank-19 archive, one
solver per task, the same pinned model checker, and task-local scratch deletion.

| Stage | Seconds | Model | Residue |
|:--|--:|:--:|:--:|
| homogeneous base `2710` | 160.867 | verified | zero |
| Congruence occurrence `2820` | 160.614 | verified | zero |
| Factor mark parity `2980` | 160.911 | verified | zero |
| BVE-retry Release `3310` | 452.669 | verified | zero |
| inline-H Release `3520` | 445.891 | verified | zero |
| TransitiveReduce Release `3620` | 446.457 | verified | zero |

The first possible owner is therefore between `factor2980` and
`bve_retry3310`. Later inline-H and TransitiveReduce changes leave the long
trajectory unchanged. Rank 19 emits no TransitiveReduce failed-source unit in
the current solve, independently excluding that mechanism as its owner.

## Immutable BVE midpoint ladder

Campaign `20260831-3650-tierd-rank19-bve-repair-ladder`, job `1657610`, tests
four intermediate Trace binaries with event output disabled. The first three
arms return model-verified SAT and reproduce the fast arm's complete trajectory
exactly: 27,288,576 decisions, 750,813 conflicts, and 1,737,041,537
propagations in every arm.

| Stage | Seconds | Verdict | Trajectory/validation |
|:--|--:|:--:|:--|
| physical-binary gate input `3080` | 199.167 | SAT | exact; model verified |
| compaction and unit flush `3170` | 203.532 | SAT | exact; model verified |
| definition/round-2 state `3220` | 202.296 | SAT | exact; model verified |
| retry observer `3270` | 900.005 | UNKNOWN | observer timeout |

The final task's 3.63-GB output and every derivative were deleted. Its timeout
is not scored; it is evidence only that the trajectory-changing interval lies
between `3220` and `3270`.

## Final Strengthen split

Campaign `20260831-3660-tierd-rank19-strengthen-repair-ladder`, job `1657614`,
tests the two intermediate pins in that interval. The `3240` binary contains
the production repair that removes Strengthen's whole-arena root-false flush.
The `3260` binary adds the focused external-1042 observer without changing the
retained production mechanism. Both reach the 900.005-second diagnostic cap;
both delete roughly 3.6 GB of task output and leave zero residue.

This makes `3220 -> 3240` the causal interval. In the fast arm, Strengthen
physically removed every root-false literal across the arena before forward
subsumption, converting effective binaries into physical binaries. The old
operation was not present in pinned Kissat and had already been proven to cause
Rank 34's eight extra binary conversions and wrong second BVE schedule. The
repair cannot be reverted merely because that extra simplification happened to
help Rank 19.

## Behavior change, not direct cost

The current Release arm spends only about 118 seconds in all inprocessing but
334 seconds in search/scaffolding. Relative to the fast arm, conflicts grow
750,813 to 2,347,173, decisions 27.29M to 84.13M, search propagations 1.385B to
4.639B, and inprocessing phases 28 to 47. BVE eliminates only 34 additional
variables. The regression is thus a later formula/search trajectory effect,
not the cost of retaining root-false literals or of TransitiveReduce.

## Next owner

The next diagnostic should compare the first post-Strengthen common-state
boundary on a rank where the current candidate is also slower than Kissat,
preferably Rank 17 (449.150 versus 121.813 seconds) or Rank 21 (289.039 versus
104.477). It should inspect the physical and active clause images, binary
population/order, BVE schedule/output, heap state, and first search decision
before divergence. The repair remains unconditional while that downstream
consumer is localized; no Strengthen rollback or formula-specific profile is
admissible.

At this report snapshot the homogeneous campaign has all 40 expected solver
verdicts, 22/22 SAT models, and 17/18 UNSAT proofs independently verified with
zero observed invalidity or residue. The last proof checker remains nonblocking.
