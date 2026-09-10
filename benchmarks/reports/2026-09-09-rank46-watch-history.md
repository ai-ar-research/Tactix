# Rank46: matching watch history before a localized clause-position change

## Decision and scope

Keep candidate6654 isolated and unchanged. The earlier conflict-selection result
does not justify sorting watches, changing branch policy, or restoring clauses.
The new history shows long agreement before a much later clause-position change;
investigate the earlier propagation sequence instead. This is diagnostic progress,
not another solver repair, runtime gain, or production promotion.

Prior timing and the complete Rank1–50 table remain owned by
[the preceding report](2026-09-09-congruence-runtime-and-rank46.md): Rank49's
three-wave paired median is0.142716734×parent; full50 is1.138354554×fixed Kissat,
still above1.01. Memory and source fuzz remain independent and unclosed.

## Exact replay before interpretation

Campaign[6684](../runs/20260909-6684-rank46-watch-history/manifest.json),
job1678564 submitted19:48:03 EDT, is terminal and clean. It uses the same
6654/native6090 trace pins, one solver per task,12 logical CPUs/48000MiB,
1200s solver timeout and25-minute allocation. Global tracing selects only the
complete watch list for propagated-16719 over conflicts0–108341. It continues
to the same1M-conflict stop; diagnostic overhead is not acceptance timing.

[Summary6686](../runs/20260909-6684-rank46-watch-history/summary-6686.json)
hash-binds every input/output and validates exact per-arm1M decisions, rolling
prefix, partial proof and old100K proof-byte prefix. Every44,447 original current
macro row is preserved; only the existing initialization/29retier/990checkpoint
additions occur. Native preserves every standalone original boundary and adds
108,143 analysis/end rows inside the declared window. Normalization has positive
and negative fixtures; no unrelated field changes are accepted.

The strict global streams have178,751 current and178,749 native watch rows,
well below the2M cap. Every276/277 old selected-window watch row reappears exactly,
including metadata and duplicates, except shifted sequence indices. The partial
proofs were hashed and deleted by the runners; they are not final UNSAT checks.

## First observed difference

[Localization6687](../runs/20260909-6684-rank46-watch-history/localization-6687.json)
compares every ordered watch row using conflict/decision/level/mode and
scan/blocker/clause identity/searched offset/literal order/values. Raw IDs,
propagation-counter conventions and branch-search hints are explicitly excluded.

The first177,292 portable rows agree from startup. The first mismatch is
conflict108138, decision402320, level19, in a32-literal clause: positions4/12
exchange-18994 and18996. Both are false; clause identity, blocker, searched offset
and the per-position value sequence agree. This is not yet the first global
propagation difference, since the observer follows only one propagated literal.

The eventual21-literal conflict clause is first observed identically at87425;
later observations, including its joint ordering with the18-literal clause after
BVE99580 and through102107, also agree. Its last matching selected observation
is108086, and its first differing one is108162. The18-literal clause's selected
payload stays equal until the already divergent search at108340. All occurrences
and one-sided/duplicate state blocks remain in6686; no ambiguity is deduplicated.

This moves the investigation earlier than the reversed order at108338 while
preserving that established result. It does not establish a creation-time bug,
BVE defect, incorrect replacement loop, or reason to reintroduce the rejected
shared-blocker experiment. The ordinary propagation replacement-search and
delayed-watch mechanisms have the same source-level structure; causal runtime
state still needs localization.

## Earlier complete search window

Campaign[6688](../runs/20260909-6688-rank46-earlier-window/manifest.json),
job1678566 submitted19:59:35 EDT, captures complete conflict/implication detail
108080–108170 using unchanged pins and the same1M replay contract. Validator6690
requires all own decisions/prefixes/proofs and original standalone boundaries to
survive, with only source-backed macro/native-analysis observer additions.
[Summary6690](../runs/20260909-6688-rank46-earlier-window/summary-6690.json)
validates complete replay and122,802/145,640 global rows, below the500K cap.
All5,976 decision rows in the window match. All91 conflict sets and91 learned
sets also match, although8 conflict orders and3 learned orders differ.

The first6,849 implication records agree. At conflict108084, decision399066,
level1195, both imply16358 but select different12-literal reasons. The last five
preceding implications are identical binaries whose false antecedent is16365.
Actual implied-literal/level order first differs at row23,066, conflict108122:
current implies18994 while native implies19043. At108124 the first learned-order
difference swaps19129/19011 at the same level16 without changing the clause set.
These observations move the next target earlier than the selected watch pose
change; they do not prove the108084 reason choice alone causes the final gap.

Campaign[6693](../runs/20260909-6693-rank46-reason-watch/manifest.json) adds only
the complete watch list for propagated-16365 in the same91-conflict window,
using the same frozen pins. Validator6695 must preserve all original global
rows and every standalone/decision/proof output before interpreting membership,
values or order of the two reason clauses. NEXT validate and interpret the
selected108084/399066/level1195 list, then trace any demonstrated physical-order
difference backward. Audit6691 owns the launch/job state and any later published
diagnostic results. No altered-output observer is accepted provisionally.

## Validation and limits

Five new maintained tests cover exact history projection, missing/changed/
reordered rows, duplicate preservation, one-sided blocks and noncontiguous
identical states. All207 benchmark-tool tests pass. No solver source was edited;
there is no new solver-change batch requiring duplicate fuzz. Audit6691 records
the aggregate tests,13 watcher tests, six documentation gates, preserved hashes,
job accounting and later published correctness records.

B/C6661 is66/66 independently checked and clean. Full50's remaining proof checks
continue asynchronously; audit6691 owns the latest published state, not a rewrite
of6668 or6679. Actual invalidity pauses dependent work. No live checker scratch,
fuzz artifact, fixed reference, production source or existing campaign was altered.
No manual deletion, denied external transfer/push retry, or FlatLists rewrite.
