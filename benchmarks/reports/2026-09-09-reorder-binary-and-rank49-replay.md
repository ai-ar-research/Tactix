# Reorder's exact binary residual and a current short Rank49 witness

## Outcome and scope

Rank35's selected score discrepancy is exactly explained by one incident
binary clause, not by differing large-clause weights or a demonstrated Reorder
arithmetic defect. Current6579 is unchanged and unpromoted. Full6596 remains
8401.066/fixed7239.010=1.160526923 over50; this checkpoint claims no runtime gain.
Rank35 takes506.019/306.699 seconds, and Rank49 takes158.615/22.555.

Rank35 campaign6629/job1678421, submitted18:04:31 EDT September9, is complete.
Sealed `benchmarks/runs/20260909-6629-rank35-reorder-input/summary-6631.json`
binds both unchanged6579/6305 pins and every dependency. All6624 ordinary,
heap, choice and score traces plus partial-proof bytes/hash replay exactly
through4.4M. Both tasks exit0 and delete their partial proof/input/scratch.
These stopped prefixes are not final UNSAT proof acceptance.

## Exact selected Reorder accounting

At reorder_begin3074062/decision161620843, both ordinary macro rows independently
confirm stable mode, level0 and58822 unassigned variables. Both complete active
sets contain exactly those58822 identities. Active membership therefore proves
unassigned here; absence would not prove a retired endpoint assigned.

All25 incident live large-clause rows agree, including redundancy, literal set,
physical order, metadata and per-literal root values. Their contributions are
1.013671875 for positive1373 and0.8984375 for negative1373. The counted binaries
are:

| Binary | Current6579 copies | Earlier6305 copies |
|---|---:|---:|
| (-34621,-1373) | 1 | 1 |
| (-29898,-1373) | 0 | 1 |
| (-1373,-918) | 1 | 1 |

Every endpoint is known unassigned at this root boundary. No missing value is
defaulted to zero. The positive weight is1.013671875 in both; the negative
weight is2.8984375 versus3.8984375. Applying max(pos,neg)+2*min(pos,neg) gives
exactly4.92578125/5.92578125, the observed outgoing scores. Entry score was zero
in both, so rescaling contributes zero. The two frozen Reorder implementations
are byte-identical. All multiplicities are retained, not unique-set counts.

This explains the selected score but not the binary's first mutation, whole
formula equivalence, all2407 later heap displacements, or the runtime difference.
It does not justify restoring a possibly redundant binary for an earlier branch.

New6637/job1678424, submitted18:21:39 EDT, enables the existing v3 lifecycle
observer for exactly(-29898,-1373), with all6629 observers retained. Require
full own trace/partial-proof replay, then compare complete lifecycle events,
rules, endpoint values and copy counts before Reorder3074062. Internal ids and
payload metadata must not be mistaken for portable semantic differences. There
is no new global event tracer, observer implementation or solver change.

## Rank49: current applicability of the shorter witness

While6629 ran, the existing6201 Rank49 witness was found and reused. An initial
preflight assertion of full-work identity failed before campaign creation:
current6579 and semantic6107 have identical certificate bytes but differ in
BVE resolutions, definition-check effort and material-touch counters. This is
a rejected diagnostic premise, not a solver/checker invalidity. The exact six
old/new statistics are retained in6632's manifest and6634's summary. Do not
call certificate identity full work identity.

Candidate-only6632/job1678423, submitted18:18:42 EDT, completed cleanly using
the unchanged6579 pin. No native invocation occurred. Sealed
`benchmarks/runs/20260909-6632-rank49-current-replay/summary-6634.json` proves
all622123 decisions, complete inprocessing boundaries, prefix and partial proof
through100K exactly match6201's candidate. All22699 macro rows were compared:
the sole difference is preprocess_begin's startup stamp4400→2200, with all
other fields and rows equal. This exception is retained, not normalized away.

The cached native first decision mismatch therefore remains applicable:
57569 matching rows, then current(conflict9274,decision57332,level7,-2198)
versus native(9273,57332,8,-2161). Next capture the implication/conflict-analysis
window leading into that split on this short witness. The earlier Congruence
binary-count gap at2988 is not a proved cause. Preserve current activation and
compact-stamp repairs despite the startup stamp difference. Any new observer
must replay each arm's existing ordinary traces/partial proof before use.

## Validation, gates and hygiene

Maintained reorder_input.py has nine new tests: active framing/count/identity,
unique root-boundary selection, binary multiplicities and polarity reward,
redundant/satisfied exclusions and root-false literals, physical100-open cutoff,
small-open clamping, missing endpoints, inconsistent values/active state,
nonroot use, nonincident clauses and inconsistent physical order. Existing5785
formula fixtures cover six malformed cases; maintained binary snapshot and
lifecycle readers retain their tests. All183 benchmark tests pass. Audit6635
reruns these,13 watcher tests and six documentation checks. No solver source
changed, so no duplicate fuzz campaign is required by these diagnostics;
current-source Claude-owned fuzz remains unavailable and unclosed.

Full6594 remains50 expected outcomes,48 checked/clean at the pre-report snapshot,
with Rank22/43 checks asynchronous. Audit6635 owns later statuses. B/C6584 is
66/66 checked; Rank38/50 repeats each have six checked certificates. Do not
refresh completed gates, fixed references, fresh6121/6227 or matched6108.
cpu-q5784 scripts/pins remain untouched. All new tasks use12CPU/48000MiB,
1200 solver seconds and25-minute allocation, one solver each, no throttle/node
constraint. Only published results are inspected; no live checker scratch.

Preserve Rank4/32 closures, stronger Rank33 closure6519, rejected Rank31
composition5920, N1--N4/SP-SLOT-5, compact-stamp6357 and activation6424.
FlatLists remains unapproved. No fuzz access, denied transfer/push retry,
promotion or manual deletion occurred. Living documents remain within fixed
caps. Andrew's timestamped TeX/PDF is local, not sent.
