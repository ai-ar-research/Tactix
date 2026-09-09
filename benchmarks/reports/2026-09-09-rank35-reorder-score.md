# Rank35: first score difference is within Reorder

## Decision and experiment

Current6579 remains frozen and unpromoted. Rank35 costs506.019 seconds against
fixed Kissat306.699; complete6596 remains8401.066/7239.010=1.160526923 over50.
This checkpoint localizes a trajectory difference against earlier Tactix6305,
not a new native timing denominator or a demonstrated runtime improvement.

The preceding6619/6621 replay established equal availability at the first late
decision split, with2407 displaced heap positions and one score difference
among49690 heap members. The selected variable1373 had scores4.92578125 and
5.92578125. That downstream snapshot did not establish where the score changed.

Campaign6624/job1678419, submitted17:44:00 EDT on September9, uses the same
current6579 and earlier6305 pins. Existing standalone score tracing adds all
1373 macro snapshots from conflict0 through4.4M; no solver or observer source
changes. Each independent task has12 logical CPUs,48000MiB,1200 solver seconds
and25-minute allocation, without a node constraint or array throttle.

Both tasks completed cleanly. Validator6626 binds the exact pins, input,
dependencies, closed outputs and partial-proof hashes before interpretation.
Every ordinary, heap and choice output plus partial proof exactly replays each
arm's6619 control. This is observer neutrality and current applicability, not
final UNSAT proof acceptance. Tasks hash/delete partial proofs and scratch.

Authoritative result:
`benchmarks/runs/20260909-6624-rank35-score-history/summary-6626.json`.

## Complete selected history

The captures contain113324 current and124010 earlier rows, with no counter
resets. Both complete histories pass schema and state validation. Causal
comparison ends before the first split at(conflicts3074183,decisions161631565),
not at a conflict-only cutoff that would include already divergent decisions.

All76276 pre-split boundary sites align in order, with no unmatched sites.
Repeated labels retain occurrence ordinals. There is exactly one differing
score site; position, value and activity have no differences over these sites.

| Boundary at conflict3074062, decision161620843 | Current6579 | Earlier6305 |
|---|---:|---:|
| reorder_begin score | 0 | 0 |
| reorder_begin heap position | 14094 | 14094 |
| reorder_end score | 4.92578125 | 5.92578125 |
| reorder_end heap position | 28190 | 28190 |

Both snapshots have value0 and active1. The first score difference is therefore
within this Reorder call, not an observed earlier1373 score-history drift.
The entire Reorder implementation file is byte-identical between both frozen
sources. Entry score zero stays zero under rescaling, so its outgoing score is
the formula-derived weight. This narrows the next question to incident input
clauses/binaries and root values. It does not prove erroneous arithmetic, whole
formula identity, or that this single residual causes every displaced heap
position and the eventual runtime difference.

## Next diagnostic, tests and safeguards

New6629/job1678421, submitted18:04:31 EDT, keeps all6624 observers and enables
existing snapshots at reorder_begin3074062: every live large clause containing
either polarity of1373 with physical order, metadata and root values; incident
binary multiplicities; and the complete active-variable set. It uses unchanged
pins and no global event tracer. First require all6624 traces and partial proof
to replay, then validate snapshot framing and reconstruct the two polarity
weights. Never assume an unobserved endpoint is free. Removing or restoring a
binary just to obtain the earlier branch is not an acceptable repair.

New maintained score_history.py has seven focused tests: framing and identity,
finite scores and valid state, warm-up counter resets versus search regression,
repeated/unmatched sites, independent comparison fields, exact decision cutoff,
empty histories and reordered alignment. All174 benchmark tests pass; the
checkpoint audit6627 reruns these,13 watcher tests and six documentation checks.
No solver implementation changed, so no duplicate fuzz is required by this
diagnostic; current-source Claude-owned fuzz remains unavailable/unclosed.

Full6594 has50 expected outcomes and48 independently checked/clean at the
pre-report snapshot. Ranks22/43 remain pending; checking continues independently
without delaying development. Audit6627 owns later exact statuses. B/C6584 is
66/66 checked, and Rank38/50 repeats have six checked certificates each. No
completed gate or fixed reference is rerun or rewritten.

Preserve compact-stamp6357, activation6424, material scheduling6579, Rank4/32
closures, stronger Rank33 closure6519, Rank31's rejected5920 composition,
N1--N4 and SP-SLOT-5. FlatLists approval remains outstanding. No fuzz access,
denied transfer/push retry, promotion, manual deletion or live checker-scratch
inspection. Fixed41--50/6108, fresh6121/6227 and cpu-q5784 stay immutable.
Living documents retain their existing caps. Andrew's corresponding TeX/PDF
is local and not sent.
