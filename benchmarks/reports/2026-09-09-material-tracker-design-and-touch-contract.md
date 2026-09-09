# Material tracker: current-source mapping and protected touch contract

## Decision and approval status

The next substantial Rank 50 experiment needs a separate material lifecycle,
not a new interpretation of `touch_epoch`. The current-source witness is
6562/6564: native and Tactix first diverge at the native Eliminate invitation
823562 after both create the selected binary at 780400. Native 6553/6555 shows
two actual false-to-true subsumption-material events behind that invitation.
This report finishes the implementation mapping and records new baseline tests.
The isolated implementation was proposed to the user; approval is pending.
No material tracker, policy change, new solver run or runtime gain is present.

## Source authority and reference choice

Current Tactix is the immutable 6424 source/build manifest. Native producer and
consumer semantics are read from the hash-bound 6550 observer source, whose
ordinary replay is closed by 6555/6558. CaDiCaL reference commit is
`7b99c07f0bcab5824a5a3ce62c7066554017f641`; `internal.hpp`, `clause.cpp`,
`subsume.cpp` and `flags.hpp` are clean and hash-bound by audit 6569.

Both references use persistent subsumption flags. CaDiCaL's `mark_subsume`
counts a clear-to-set transition; its forward round only resets flags when
completed and routes shrunken clauses through `mark_added`. Kissat clears
variable flags at the end of its forward scan and rearms remaining/strengthened
large clauses and new binaries without incrementing `variables_subsume`.
These are not interchangeable lifecycle protocols. Use Kissat for the default;
retain Tactix's explicit CaDiCaL victim-band arm and N1 instance access.

## Why shared touch cannot implement the counter

`kernel_touch_var`, `kernel_removed_var` and `kernel_touch_kept_var` all advance
one shared epoch on every call. Repeated advice is deliberately an event.
Kissat's added-material counter instead advances only when a real producer
changes the variable's subsumption flag from false to true. Clearing does not
reset the counter; retry rearming does not increment it; opposite polarity is
the same subsumption variable but a different Factor flag.

| Current consumer | Preserve through the repair |
|---|---|
| Standalone C1 SubsumeForward | Existing root notification, touch watermark, tail retry and public instances; it is not the native fused forward owner. |
| C2 Strengthen | Canonical binary scan, two-variable victim predicate and all-free-variable connection predicate migrate together; preserve old shared touch publication for other rules. |
| BCE and CCE | Added-or-removed predicates, independent watermarks and retry events. CCE still needs root notifications. |
| HyperTernaryRes and Instantiate | Their own touch watermarks, candidate predicates and budget-tail retry events. |
| Explicit likely-kept redundant band | Keep its separate `touched_kept` channel; never treat redundant-band retry as canonical irredundant material. |
| Sweep proof snapshots and diagnostics | Existing shared epoch remains meaningful and monotonic; do not silently replace its currency. |
| Factor | Already reads independent per-polarity flags, not shared touch predicates. Preserve its paired producer calls, counter and drain watermark; older comments mentioning Factor as a touch consumer are not current authority. |

The new fixed test covers existing behavior, not the proposed replacement.
Removing or coalescing shared touch calls would break that protected contract.

## Exact producer wiring required

| Current owner | Separate material publication required |
|---|---|
| `kernel_reserve_vars` | Each fresh activation; preserve extension-variable and deferred-initialization behavior. |
| `kernel_add_large_clause` | Irredundant literals only when `mark_added_material` is true; not ordinary learned large clauses. |
| `note_binary_add` | Both variables when semantic publication is enabled, including learned binaries and physical duplicate additions; the flag suppresses repeated counter increments. |
| `kernel_replace_large_clause_in_place` | The existing selected irredundant surviving-literal loop when publication is enabled. |
| Analyze on-the-fly strengthening | Its explicit surviving old-tail subset; preserve the retained-first-watch and originally-redundant exclusions. Do not widen to the whole replacement. |
| Vivify large rewrite | Its explicit old physical tail as well as the replacement prefix. Native marks through the old-size walk before installing the new size. |
| Vivify promotion | All literals when a learned clause becomes permanent irredundant material. |

Do not publish material from `kernel_touch_sync_fixed`, removal notifications,
C1/C2/BCE/CCE/ternary/Instantiate retry loops, kept-band advice, or physical
mirror/reconnect operations that already suppress added-material publication.
In particular, do not insert the new helper inside `kernel_touch_var`: its
callers intentionally include all these non-producers. Preserve true addition
on a fixed variable; native does not apply the removal producer's fixed filter.

## Proposed isolated structure and migration

Use one byte per variable for a separate subsumption flag, a 64-bit cumulative
kernel material counter, and a strategy-owned 64-bit Eliminate watermark. The
counter is derived lifecycle state, not a diagnostic Stats field repurposed as
policy state. A per-variable 64-bit epoch is unnecessary. Account explicitly
for vector capacity, growth/compaction peaks and the added storage in memory
diagnostics; one byte per live variable is the logical payload, not a promised
RSS bound. Do not pack into existing Factor/BVE flags as an unrelated redesign.

Deferred startup may represent all activated variables as implicitly armed:
count each new activation once, suppress repeat parsing additions, materialize
the all-set flag image before consumption, and never invent a later counter
increase during materialization. Normal extension growth starts fresh flags
clear then publishes activation. Variable compaction remaps live flags but
never recounts or resets the cumulative counter or consumer watermark.

Migration stages after approval:

1. Add the independent state and exact producers with no scheduling consumer.
   Retain every existing touch/Factor/removal call and its order. Add focused
   lifecycle tests and prove observer-free work/proof neutrality before cutover.
2. Migrate the three canonical C2 predicates together. At the normal scan-end
   boundary clear flags and rearm the unreached live irredundant candidates,
   live strengthened survivors and newly derived binaries without incrementing
   the material counter. Keep strong interruption behavior: an entry/partial
   abort must not retire unseen material. Explicit single-instance calls never
   consume a whole-round flag image; band-only retries cannot arm canonical
   material. Public inference guards and proof deltas remain unchanged.
3. Add the material operand to Eliminate admission alongside the existing
   removal/root invitation behavior. Do not copy the new watermark at generic
   `run_phase` entry: that would consume work before a successful owner visit.
   Save it at the reference-shaped maximum-bound phase-exit bookkeeping point.
   Preserve the existing lower-bound advance and removal rearming; no reset of
   the material counter or implicit coupling to a Probe phase.

The native phase-exit detail matters: its per-round `last_round_eliminated`
shadows the outer variable, and the heap is released before the final remaining
population query on consistent paths. Current Tactix already preserves this
effective bound-progress behavior. Do not independently "fix" it, substitute
the last C2 `strengthen_complete` result for it, or couple this proposal to an
unapproved scheduling reinterpretation. Keep SP-SLOT-5 removal-event semantics.

## New baseline tests and validation required for cutover

6567 is a fixed C++ fixture linked against unchanged 6424 Release objects.
Frozen 6568 runs optimized and fixture/inlined-code UBSan builds. Each passes
1300 cases: all 1296 four-event sequences over two variables and three shared
producer types, plus deferred activation/extension, root notification,
publication suppression/duplicates and compaction controls. The exhaustive
sequences verify 6480 state snapshots per build and every applicable watermark
query, including future and equal watermarks. Factor and BVE counters must
remain unchanged by the three shared advice primitives.

These tests establish what a future change must preserve. UBSan does not
instrument the prebuilt Release objects; no new flag implementation is tested.
No CNF, certificate, solver/fuzz execution or cleanup artifact was created.
Both builds passed on their first execution, with logs and binary hashes
retained. Existing actual native 6547 tests cover the complementary flip,
opposite-polarity, clear/rearm, fixed-variable and admission contract.

Future focused coverage must execute the new producer/consumer code: repeated
and opposite-sign additions; each selective producer above; fixed variables;
normal completion, budget tails, interrupted entry and partial progress;
multiple consumer schedules; cold startup, extension and compaction; and
explicit C1/C2 instances with empty scheduling flags. Add DRAT/LRAT proof and
SAT model checks for rule-level cases. A plus B and mechanism-relevant C precede
targeted Tier D under the existing protocol; Claude-owned fuzz remains a
promotion obligation, currently unavailable, not something this fixture replaces.

## Metric, risks, rollback and aggregate obligations

The first experiment must recover the missing native material invitation on
Rank 50 without rank/literal/conflict-specific code, and explain any earlier
decision split. It must not assume that scheduling equality is runtime success.
Then measure full solver runtime and memory with three independent paired
waves, reversed middle order and the existing noise rule, followed by full
Rank 1--50 scoring and proof/model cleanup. Capture adverse cases, not only
improvements. The 6424 immutable source/build and checked 6439 run are the
rollback/control point; older checked 6169 remains the better aggregate result.

Risks are broader search-trajectory changes, omission of a selective producer,
premature watermark consumption, interruption starvation, public-API leakage
and memory overhead. If any premise fails, retain the evidence and stop that
candidate instead of adding benchmark-specific constants or compensating knobs.
Full current 8731.653 / fixed 7239.010 = 1.206194355 remains unchanged.
Matched 41--50 and the fresh all-fifty reevaluation are already closed and must
not be repeated. Audit 6569 verifies the now-final 6535/6539 proof checks and
their cleanup; there is no remaining owned live checker from that checkpoint.
Production, frozen campaigns and fixed references remain unchanged. No push,
fuzz access or promotion; approval and the full convergence goal remain open.
