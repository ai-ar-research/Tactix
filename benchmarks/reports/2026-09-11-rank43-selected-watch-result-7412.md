# Rank43 selected watch replay passes; construction/order is the next lead

Frozen [7409](../runs/20260911-7408-rank43-selected-watch/solver-summary-7409.json)
passes both full own solver replays against7397 and exact completed-core7406
replay with only166/168 selected watch additions. Both full solvers returned
UNSAT with their exact own statistics, certificate and entire decision stream.
The new1--82 traces contain62773 candidate and54011 native rows, both uncapped.
Independent proofs were pending when7411 audited the campaign; no timing or
production acceptance is claimed.

Each arm has exactly one captured selected scan at conflict81, decision4048,
level115, before the conflict82 increment. Every original row is retained,
and watch snapshots preserve order, physical duplicates and large-clause poses.

| Clause | Candidate zero-based position | Native zero-based position |
|---|---:|---:|
| `(1038,-49957)` |49|52|
| `(1038,49959)` |130|1|

Both clauses occur once in both snapshots. The candidate clause multiset is a
subset of the native one; native alone has one `(1038,49900)` binary and one
`(-50106,1038,49900)` large watch. No candidate-only entry exists. The first
five native watches are `(-1039,1038)`, `(1038,49959)`, `(1038,49963)`,
`(1038,49900)`, `(-49909,1038)`. Candidate begins `(-1039,1038)`,
`(-49909,1038)`, `(-49910,1038)`, `(-49911,1038)`, `(-49912,1038)`.
These are ordered entries, not deduplicated formula counts.

This strengthens the incoming-watch construction/order hypothesis. Both ordinary
search propagators stop at their first conflict, and their chosen binaries are
already present with different order. It does not yet establish simultaneous
falsity of both binaries: these binary payloads omit blocker values, and none
of the captured large-clause value vectors includes variables49957 or49959.
The two extra watches are not proven to cause the split or to be stronger.
Do not sort/reorder watches as a benchmark-conditioned patch.

Next use existing selected implication and binary-lifecycle observers on these
pairs to determine assignment signs/reasons and where their watch admission,
removal or reconnection diverges. Trace the earlier common startup state as
needed; the1697 total-binary discrepancy after Congruence is not yet linked to
these two extra entries. The earlier conflict48 physical-order difference is
still retained. Existing pinned native5414 and candidate7373 sources contain
`SAT_EVENT_TRACE_IMPLY_LITERALS` and the per-arm `BINARY_LIFECYCLE_TRACE/PAIRS`
observers. No successor run has been prepared or submitted for this next step.

7411 owns the latest scheduler/publication snapshot: full7385 has48/50 checked,
target7382 all12, profiles7389 three of four,7397 one of two,7401 one of two,
native7403 all one, selected7408 zero of two. Host Debug7342 PID3043191 was live,
not terminal. Watcher51749 follows7408; the remaining checker jobs continue.
No solver change, duplicate fuzz, manual deletion, old-run mutation or denied
transfer/push retry. The comparator controls cover order, duplicates, malformed
payloads, missing rows and mixed snapshots; no additional solver unit test is
needed for this read-only result. Runtime/memory convergence remains unmet.
