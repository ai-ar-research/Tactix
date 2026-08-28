# Rank 16 watch-vector lifecycle localization

Date: 2026-08-28

## Outcome

Rank 16's remaining SAT cost was partly a watch-vector lifecycle mismatch, not
Factor output, formula classification, or search quality. Tactix invoked a
whole watch-arena pack from per-list growth, including propagation's delayed
rewatches. Pinned Kissat never does: `inlinevector.h` grows or relocates one
vector, while `clause.c:new_clause` alone checks the `collect.h` defragmentation
predicate after clause creation.

Tactix now follows that lifecycle and retains exactly the same Rank 16 search:
14,441,406 decisions, 4,777,402 conflicts, and 152,705,482 consumed-literal
propagations. The reversed same-CPU comparison reduces solver time from
173.339 to 151.348 seconds (12.7 percent) and independently checks the SAT
model. No formula-, density-, benchmark-, rank-, identity-, or conflict-based
selector was added. Factor remains enabled at 50 permille and performs zero
transformations on this instance.

The repair deliberately exposes the remaining representation issue: peak RSS
rises from 79,196 to 96,032 KiB when Tactix retains Kissat's power-of-two stack
capacity. Current Kissat uses 60,012 KiB on the same formula. That memory
gap remains open and must be localized generally rather than hidden behind a
formula-size gate.

## Profile and source owner

Campaign `20260828-1530-tierd-sat-rank16-perf-callgraph`, job 1654579, sampled
the accepted production binary with call graphs. `propagate_literal` owned
67.33 percent of children. `WatchLists::defrag`, called from propagation-time
growth, accounted for 7.17 percent of total cycles, including 5.85 percent in
`memmove`. Vector resize/memset and further movement were separate residuals.

The pinned source audit found the following reference contract:

- `inlinevector.h` extends a vector at the global tail when allocation remains,
  consumes an invalid spare word for a non-tail vector, or calls
  `kissat_enlarge_vector`.
- `vector.c` relocates to twice the live size (one word when empty), counts the
  abandoned image plus unused suffix in `vectors.usable`, and packs nonempty
  vectors in physical-offset order.
- `collect.h` checks only when stack size is strictly above `2^18` and usable
  words are strictly above 75 percent.
- `clause.c:new_clause` is the checkpoint owner. Specialized binary constructors
  used by Factor, Congruence, Strengthen, Substitute, Sweep, and Vivify's direct
  paths do not acquire an additional checkpoint.
- `proplit.h` hoists the propagated literal's assignment metadata once before
  scanning its binary watches.

## Implementation

`WatchLists` maps Kissat's offset-zero sentinel without storing it. First
allocation therefore exposes three physical words, the defragmentation size
predicate adds the omitted sentinel, and post-pack capacity is the smallest
power-of-two stack containing the sentinel and live words. A tail slice extends
in place; every other full slice relocates to twice its live size. `holes` is
now the exact `vectors.usable` image, including abandoned words and every unused
per-slice capacity word. Removal, probing rollback, batch rebuild, Walk resume,
and Congruence rebuild all maintain that accounting.

Defragmentation is absent from growth and propagation. Its explicit consumers
are the Tactix equivalents of Kissat's generic clause constructor: original
import, learned clauses, BVE resolvents, large Factor quotients, and binary
Vivify replacements. Large Vivify in-place rewrites and specialized binary
constructors are not consumers. DIMACS import has no private deferral or final
pack.

Full propagation also passes one hoisted `Assigned` source to every binary
implication in the scan, matching `proplit.h` rather than reloading identical
level/reason metadata per watch.

## Controlled evidence

The first same-node localization pair, campaign
`20260828-1605-tierd-sat-rank16-samecpu-kissat-vectors` job 1654593, compared
the accepted binary with the lifecycle candidate. Both models checked and all
solver-state counts were identical. The accepted binary took 177.219 internal
seconds / 79,196 KiB; the candidate took 148.101 seconds / 83,532 KiB.

Campaign `20260828-1615-tierd-sat-rank16-samecpu-antecedent-hoist`, job 1654595,
isolated the reference metadata hoist without changing any state. Watch
lifecycle alone took 149.920 seconds; adding the hoist took 145.782 seconds.

The final exact-consumer, exact-capacity reversed pair is campaign
`20260828-1630-tierd-sat-rank16-samecpu-final-watch-lifecycle`, job 1654601.

| arm | solver seconds | wall seconds | peak RSS KiB | decisions | conflicts |
|---|---:|---:|---:|---:|---:|
| accepted `45945da` | 173.339 | 173.35 | 79,196 | 14,441,406 | 4,777,402 |
| final lifecycle | 151.348 | 151.36 | 96,032 | 14,441,406 | 4,777,402 |

Both arms returned SAT, independently checked all 7,823 clauses, and left no
scratch or certificate residue. The final source binary is SHA-256
`652285f317fd0f961c7245e0fa3153da57824b0cf26da3d943613594fa9791cb`;
campaign `20260828-1650-tierd-sat-rank16-samecpu-final-source-v-kissat`, job
1654604, is its immutable comparison with current pinned Kissat. On CPU 32 of
node014, Tactix takes 139.740 internal / 139.75 wall seconds and Kissat takes
127.36 internal / 128.12 wall seconds. Both models check. The remaining wall
gap is 1.091x, not the former multi-fold SAT gap; RSS remains 96,032 versus
60,012 KiB.

## Allocation follow-up: rejected raw-stack candidate

The 1,134-row slot-memory trace in campaign
`20260828-1710-tierd-sat-rank16-slot-memory`, job 1654605, shows that live
storage is not a 96 MiB representation. At the final Vivify boundary Tactix
holds a 32 MiB watch allocation, about 9.5 MiB of clause-arena capacity, and a
current RSS of 59,532 KiB. Its 96,600 KiB high-water mark is transient.

A follow-up candidate replaced the vector backing store with the same raw
`realloc` stack interface used by Kissat and packed the live vectors downward
in place. Reversed same-CPU campaign
`20260828-1740-tierd-sat-rank16-samecpu-realloc-watch-arena`, job 1654607,
preserved every decision, conflict, propagation, and checked model. It was
slightly faster than the vector baseline (144.145 versus 145.464 internal
seconds), but peak RSS regressed from 96,032 to 104,224 KiB.

Corrected diagnostic campaign
`20260828-1810-tierd-sat-rank16-realloc-slot-memory`, job 1654609, localizes the
regression. At the corresponding final Vivify boundary the raw-stack process
retains 46,367 KiB of free heap storage versus 19,167 KiB in the vector trace;
the following Sweep raises the high-water mark to 104,736 KiB. Thus raw
`realloc` changes allocator retention without reducing the live formula image.
The production candidate was rejected and the simpler vector backing retained.
No allocator-, formula-, size-, rank-, or benchmark-specific gate was added.

Job 1654608 was the first trace attempt and is non-scorable infrastructure
evidence: its production binary lacked event-trace support, so the SAT model
checked but the requested trace file did not exist. Job 1654609 uses a pinned
diagnostic-only trace build and supersedes it.

## Correctness and reopening conditions

The complete Release suite passes 76/76. Focused Kernel, Parse, Substitute, and
Congruence tests pass under ASan/UBSan with leak detection disabled for the
ptrace-constrained environment. The growth regression pins the three-word
initial tail, doubled relocation image, usable accounting, strict 75-percent
boundary, post-clause-only pack, and word preservation across both relocation
and defragmentation.

N1--N4, proof behavior, the proof-rule/strategy boundary, formula
classification, and all production effort/admission settings are unchanged.
Reopen this closure on a different vector encounter order, usable count,
threshold boundary, checkpoint consumer, Rank 16 trajectory, model/proof
failure, or a profile showing propagation-time defragmentation. The 96,032 KiB
RSS and the residual runtime gap are open representation questions, not grounds
for restoring the old growth pack or adding an instance gate.
