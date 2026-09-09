# Rank37: heap topology, not score or availability

## Outcome and decision

The within-1% goal remains unmet: current6049's frozen full40 measurement is
7443.116s/fixedKissat6810.066s=1.092957983x. No new solver implementation or
runtime gain is claimed here. The new evidence rules out score, membership,
assignment and active-state differences at the last common Rank37 decision;
it selects earlier heap topology as the next causal producer to investigate.

6076/job1676769 is terminal and fully replay-validated. Every cached ordinary
1M trace, prefix, previous auxiliary trace and partial proof byte matches its
arm. Both tasks exit0 and delete scratch. Full heap snapshots have89,594
variables each, with identical membership and all shared score/assignment/
active metadata. Exactly254 positions differ.

At conflict272979, after common decision76953348 (level387,literal116330),
Tactix has116360 at heap position1 and116362 at3; native has116362 at1 and
116360 at3. Both variables are active, unassigned and have exactly the same
printed score4.8334225168340841e-29. Tactix then decides116360 followed by
116362; native decides them in the opposite order. This is a physical-order
difference under equal scores, not one solver having an unavailable variable.

The selected116362 state first differs in position at decision76953052 within
the observed conflict, while both choose51042 atlevel91. That is not the heap's
creation point: the retained topology may have diverged much earlier. The
snapshot is after a common decision and before subsequent propagation; do not
infer broader state equality from it.

## Next producer investigation

Source review finds matching strict-left child ties and swap-last/up/down
removal procedures in current Tactix heuristics and native `inlineheap.h`.
That is supporting static evidence, not an exhaustive same-state differential
test or permission to tune the tie rule. Earlier Sweep152381 binary-copy and
Vivify188501 retained-clause differences remain unresolved; no causal link to
this choice is asserted.

6081/job1676773 is complete: the existing standalone score/position logger
for116362 replays every ordinary1M, heap, choice and partial-proof artifact.
Across11,804 exact-label/counter/occurrence matched boundaries, no selected
score, position, value or active flag differs. All unmatched sites are retained;
case-sensitive rule names differ between solvers, so this is not a claim of
complete rule-boundary alignment. Equality of one variable's position cannot
exclude earlier differences elsewhere in the heap. Native reran only for
missing history; fixed timing remains unchanged.

## Correction: two rounds in the split conflict

The preceding frozen runtime report and20:55 Andrew report incorrectly called
228591 the last inprocessing conflict. A strict conflict-only cutoff excluded
two full rounds at272979/decision76952961, before the first split at76953349.
New6088 uses both counters and tests before/equal/after boundaries, repeated
rounds and alignment rejection. Its hash-bound output preserves all mapped rows.

Both solvers enter those rounds with92,675 active variables,524,761
irredundant large clauses,32,888 redundant large clauses and128,315 binaries.
The first Sweep ends with128,486/128,494 binary copies; the second with
124,018/124,028. Following Substitute endpoints reconverge in each round.
All mapped active/large-clause counts agree; final Factor endpoints also agree
at92,344 active,498,999 irredundant,26,882 redundant and123,373 binary clauses.
These counts do not establish formula identity, equal effort or heap identity.

6084/job1676775 and6085/job1676776 completed using the same immutable binaries to snapshot
the complete heap at272978/76952961 and272979/76952962, respectively. Validation
confirms every parent ordinary trace, choice row and partial proof byte
replays; only the deliberately moved heap is exempted. The bracket includes
propagation/conflict/backtrack, both inprocessing rounds and the first next
decision, not inprocessing alone. Neither campaign changes ongoing runners.

The complete incoming77,698-member heaps agree in every recorded field and
position. The outgoing92,344-member heaps have identical membership, scores,
assignment and active flags, but298 differing positions. This places creation
of the observed topology difference within the bracket, not earlier heap
history. Other incoming formula/reason/assignment state is not proved equal.
Selected116362 remains at19227 in both;116360 instead moves from9613 to19228
in native only. This explains why tracking only116362 missed that transition.

## Next internal transition

New6090 adds a standalone, compile-time diagnostic-only whole-heap series
writer in fresh Tactix/native source copies. The shared C-compatible writer
has an exact conflict selector and emits every recorded member at each macro
snapshot, without enabling the global hot-loop trace. Adapters only read
existing state. Tactix98/98 trace tests pass, including a new actual macro
integration fixture that verifies repeat snapshots and unchanged heap/values.
The writer fixture covers disabled/malformed/overflow/mismatched selectors,
repeated/empty snapshots and exact records. Native builds cleanly; its adapter
still requires the full real-instance replay check, not just compilation.

6092/job1676779 captures those transitions at272979. New6094 uses explicit
native rule-name mappings, conflict/decision/occurrence keys, monotone alignment
and bounded-memory full-state comparison. Its tests cover repeated/empty
frames, exact names and nine malformed-frame classes. All ordinary1M traces,
previous heap/choice/auxiliary traces and partial proof bytes must replay6076
despite changed observer binaries before interpretation. No solver behavior
repair, eligibility change, heap tie tuning or runtime gain is claimed.
The manifest's `observer_sources` and `observer_source_hashes` own these new
diagnostic builds; inherited source fields retain predecessor provenance.

## Tests and scope

New6080 tests isolate topology-only, assignment, score and membership changes,
plus malformed positions, flags and duplicate identities. The complete
state-analysis output binds its input hashes to the independently validated
6076 results. The adapted history comparator passes two positive and13 negative
parser controls, preserving finite/nonnegative scores, optional positions,
monotone counters, exact selected identity and framing. No acceptance or
proof checker logic changed. Existing22 benchmark tests and documentation
checks remain required at landing. New diagnostic integration tests cover the
observer; a solver-transition regression remains required for an actual repair.

All probes use independent12-logical-CPU/48000-MiB tasks,1200s solver cap and
25min allocation, without throttle or node constraints. No global EventTrace
is enabled. Diagnostic partial proofs are hashed and removed, not counted as
checked final proofs. B28+C5 and full6061 certificate checks continue
asynchronously; the fresh normal-job audit owns their current checked counts.
The cpu-q5784 runner and pins remain unchanged. Checker timeouts and the four
previously classified checker-status mismatches remain invalid/unscored.

No current-source fuzz claim or production promotion is made. The denied
external-transfer retry and push were not retried or bypassed; Claude-owned
fuzz artifacts were not inspected. No manual deletion occurred. Preserve all
active jobs and referenced pin/source/trace authorities.

Primary evidence: `20260908-6076-root-lock-rank37-choice/choice-validation.json`
and `state-analysis.json`; new history authority
`20260908-6081-rank37-position-history/summary.json` and
`same-conflict-localization.json`. The preceding
`2026-09-08-root-lock-runtime.md` owns the full runtime and initial Rank37
localization; `2026-09-08-root-reason-repair.md` owns6049's implementation and
62 checked fixed proofs.
