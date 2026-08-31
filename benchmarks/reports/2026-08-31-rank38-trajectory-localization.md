# Rank 38 trajectory localization

Date: 2026-08-31
Status: mechanism repaired; learned trajectory exact through conflict 1,000,000

## Objective

Localize the stable Rank 38 search-work excess without reverting any accepted
general mechanism or the Kissat creation-order default.  The production repair
must remain uniform and preserve N1--N4; observers are read-only and cannot be
used as benchmark-dependent policy.

Rank 38 is
`tierD24-26/266e890b706cf336378bf148bbda1aff-mexam_15_12_2.sanitized.cnf.xz`
(SHA-256 `ff8adaab...65fcc`).  Homogeneous candidate time is 790.264 s versus
pinned Kissat 366.258 s.  Candidate search work is 35,907,678 conflicts and
47,372,472 decisions, nearly twice Kissat's 18,430,710 and 24,657,248.

## Bounded trajectory evidence

Fresh immutable trace binaries were built from current Tactix and pinned
Kissat `8af8e56f174b778aef3aa45af9f739b2a5f492c2`.  Local prefixes matched all
portable semantics through conflict 10,000, including the raw, sorted, shrunk,
and final learned-clause hashes.

Campaign `20260831-4360-tierd-rank38-trajectory-prefix-observer-budget-bug`,
job `1658057`, initially reported a boundary mismatch at conflict 10,044.
The adjacent conflicts and all four learned-clause hashes were exact.  The
cause was asymmetric instrumentation: Tactix's generic post-Reorder detail
budget stopped emitting `analysis_begin` and `analysis_end` after 1,024 detail
calls, whereas the Kissat observer retained them.  This campaign is classified
observer-invalid and makes no solver-divergence claim.

Compact trajectory mode now bypasses only that high-volume-detail budget for
portable analysis boundaries.  A direct regression performs 1,024 filtered
post-Reorder detail calls and requires both boundaries to remain.  Focused
trace build and test pass.

Replacement campaign `20260831-4361-tierd-rank38-trajectory-prefix`, job
`1658059`, pins Tactix SHA `d09b66ac...235b03` and Kissat SHA
`8b67cb6f...92ab`.  Both arms are valid and untruncated.  The 458,818 common
portable events are identical through Kissat conflict 50,000.  Tactix stops at
50,001 and contributes only an expected eight-event suffix.  This excludes
startup, preprocessing, early inprocessing, mode switching, decisions,
conflict analysis, minimization, and learned-clause insertion through that
boundary.

## Learned-prefix and exact conflict localization

Campaign `20260831-4362-tierd-rank38-learned-prefix1m`, job `1658061`, is
valid and untruncated in both arms.  Final learned clauses match through
conflict 136,868 and first differ at conflict 136,869.  Tactix reaches that
conflict after switching stable to focused one decision earlier than Kissat.
The stable-mode deadline formula is exact; Tactix's absolute search-tick clock
first gains one tick between conflicts 128,927 and 128,928.

A full watch-cost observer for conflict 128,927 identifies scan `+367` as the
sole owner.  Both solvers begin with 2,998 watch words, finish with 2,678,
charge base 95, make three binary assignments, dereference 172 large clauses,
and make no large assignment.  Tactix initially reports 160 relocations and
430 total ticks versus Kissat's 159 and 429.  All 1,501 logical watch entries
match in physical order.  Per-clause pose, `searched` cursor, and literal
values also match exactly.

The 160-word-pair removal decomposes differently because one physically
identical 21-literal clause is already garbage in Kissat but live in Tactix:

`{-476,-451,-368,-367,-364,365,366,370,371,394,396,397,405,407,408,411,455,469,581,590,645}`

It is Tactix clause ID 147584.  Kissat's selected-clause lifecycle callback
shows `source=eagerly_subsume_last_learned` at conflict 128,927, decision
235,973.  The size-17 learned clause at that conflict strictly subsumes it.

## General mechanism repair

Tactix inserts ID 147584 into its four-entry recent-learned register at
conflict 128,925.  The following Vivify dose rewrites large candidates in
place but must reclaim append-only unit/binary replacements with a Tactix-only
arena packing pass.  That pass previously reused the normal collector contract
and cleared `eager_recent`.  Pinned Kissat performs no collection at Vivify
exit, so its `last_learned` register remains populated and owns the later eager
subsumption.

The collector now has an explicit implementation-reclamation mode.  In that
mode only, live recent-learned references are forwarded through the packed
arena and garbage entries are invalidated.  Default whole and sparse
collections still reset the register exactly as Kissat's real collection
sites do.  Vivify is the only production caller of the forwarding mode.  A
focused regression proves both forwarding and garbage invalidation; the
existing reset regression remains.  This is a general rule/lifecycle repair
and has no formula, benchmark, rank, size, density, ordering, or effort gate.

After repair, ID 147584 remains registered and is eagerly deleted at conflict
128,927.  Scan `+367` matches Kissat at 159 relocations and 429 ticks.  A fresh
local low-volume comparison then matches all 996,949 final learned-clause
records exactly through conflict 1,000,000, including conflict numbers, order
hashes, and set hashes.

## Validation status

Release 76/76 and Trace 75/75 pass.  Rank 33 campaign
`20260831-4370-tierd-rank33-vivify-eager-register`, job `1658063`, returns the
expected UNSAT result provisionally in 466.840 seconds versus the prior valid
467.925 seconds.  Independent proof checking remains active and retained but
does not block timing progression.

The full fixed Tier B+C campaign
`20260831-4380-BC-vivify-eager-register`, job `1658064`, is submitted with the
committed shipped Release SHA `c18133ce...cab00` and repaired candidate SHA
`9b53e154...52ab6`.  All 66 provisional verdicts are expected: candidate
2,007.209 seconds versus shipped 2,000.647 seconds, a 1.003280x ratio inside
the 1.10 guard.  Final validation continues.

That solver gate launches exactly one uniform campaign,
`20260831-4390-tierd-ranks1-40-vivify-eager-register`.  Initial job `1658130`
stopped all 40 tasks at claim creation because the new campaign omitted the
runner-required empty output directories; no solver or benchmark input was
invoked, and its logs remain immutable.  After creating those directories,
job `1658170` is the sole solver execution from the same repaired pin, without
a comparator rerun, node constraint, throttle, or selector.  Final
independently checked models/proofs and zero residue remain mandatory for both
campaigns.

Rank 38 provisionally returns expected UNSAT in 388.117 seconds and 18,197,625
conflicts, versus the preceding candidate's 790.264 seconds / 35,907,678 and
pinned Kissat's 366.258 seconds / 18,430,710.  The repaired mechanism therefore
converges search work on its causal witness; the residual approximately 6%
wall-time difference is now an implementation-cost lead rather than a search
trajectory discrepancy.  After this validation closes, run a bounded
attribution screen before selecting the next primary track: compare search
work, runtime/RSS, and collection cost on the largest remaining gaps.  Physical
representation convergence is selected only if matched-work cost dominates;
otherwise resume localization of the leading general search mechanism.  No
wholesale backend replacement is authorized without differential tests and
isolated evidence for each stage.

The homogeneous solver phase completes 40/40 expected verdicts in 9,758.178
seconds (SAT 4,473.787; UNSAT 5,284.391).  This is 0.950487x the preceding
candidate, 0.869489x accepted Tactix, and 1.271385x pinned Kissat.  Thus the
repair closes its causal Rank-38 search gap and improves the whole set by about
5%, but does not by itself attribute the remaining aggregate gap.  The bounded
search-work versus representation-cost screen above is therefore the next
decision gate.  Model/proof validation and zero-residue classification continue
asynchronously.

The first read-only attribution screen does not select a broad representation
refactor.  The remaining aggregate gap is 2,082.942 seconds.  Ranks 27, 31,
and 34 alone contribute 1,202.346 seconds and run at 3.067x, 2.201x, and
1.757x Kissat's conflict work; adding Ranks 22, 37, and 21 accounts for
1,867.295 seconds, or 89.65% of the total gap, with material search-work
divergence.  Matched-work ranks still justify bounded collection/runtime/RSS
measurement, but the leading stable search-mechanism discrepancy remains the
primary localization candidate unless deeper attribution reverses this result.
