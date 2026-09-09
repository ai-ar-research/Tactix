# Rank50: concrete producers behind the missed admission

## Outcome and decision

Native's two newly counted subsumption-material events are now localized:
at conflict 780400, decision 1165921, `new_binary_clause` marks external
literals 574 and -525, advancing the cumulative counter 2508→2509→2510.
Both variables are active, nonfixed, and previously unmarked; this is not
forward-tail rearming. The selected pair `(574 ∨ ¬525)` is independently
observed being added at that conflict, then deleted at 823562 during the
elimination phase that Tactix's previously validated guard declines.

Use this witness when validating the pending general added-material invitation
repair. Do not use its rank, literals, conflicts or counters as policy controls.
This is diagnostic localization, not that repair, approval for it, a full
runtime-causation claim, or permission to replace the shared touch epoch blindly.

## Immutable observations

[6550 build](../runs/20260909-6550-native-material-observer-build/manifest.json)
clones only cached native observer 6283, adding a callback immediately after
the real false-to-true `flags.subsume` transition and counter increment.
The callback only records state. Tactix and production sources are unchanged.
The native binary SHA is
`03010cb49b7538297cf77405b8ccdf2ec797b836d4ba7539eed15c14b53090f7`.

[6553](../runs/20260909-6553-rank50-material-producers/manifest.json),
job 1678212 since 2026-09-09T13:00:49, runs this observer to the existing
1,000,000-conflict diagnostic stop. [6555](../runs/20260909-6553-rank50-material-producers/summary-6555.json)
requires byte-identical decisions, boundaries, prefix, admission records and
partial proof against native 6286, whose ordinary streams already replay 6211.
The inherited 139633-decision prefix hash remains `236dcfa1a362af4d`.
There is no fabricated macro stream: the validated record set is the actual
four ordinary files plus the new material trace.

| Conflict | Decision | Counter | Saved | Literal | Immediate producer |
|---:|---:|---:|---:|---:|---|
|626242|960063|2508|2459|-608|forward_subsumed_clause|
|780400|1165921|2509|2508|574|new_binary_clause|
|780400|1165921|2510|2508|-525|new_binary_clause|

At the first admission record at 823562, native has 4753 irredundant clauses,
deadline 823562, last Reduce 817616, removal counter/saved value 2251/2251,
and subsumption counter/saved value 2510/2508. It answers true. The same
conflict has 54 subsequent calls after elimination: each answers false, with
deadline 1054494 and subsumption counter/saved value 2524/2524. All 55 records
are retained and recomputed against their actual guard operands; later calls
are not repeated versions of the first invitation.

[6556](../runs/20260909-6556-rank50-material-pair/manifest.json), job 1678213
since 2026-09-09T13:05:02, reuses the exact same binary and enables its existing
selected-pair lifecycle observer. [6558](../runs/20260909-6556-rank50-material-pair/summary-6558.json)
requires every 6553 trace including material events and the partial proof to
replay exactly before parsing lifecycle records. It observes:

- add at 780400, no pre-existing selected-pair copies, literal values -1/0;
- delete at 823562, both literal values zero.

The observer's deletion copy count is representation-specific; zero does not
mean the semantic pair was absent. The source tags identify immediate marking
callers, not the full upstream call stack. These two observations strongly tie
the admission witness to new binary material, but adjacency alone must not be
presented as a complete clause-to-counter call-stack proof. No learned-clause
origin or full runtime effect is claimed from a generic `new_binary_clause` tag.

Both jobs are terminal COMPLETED/0 and report scratch deleted. Each generated
and hashed a 63963136-byte partial proof, SHA
`eab8f5a38264ade4d15148a8565432e51c8c31ebe6b3d9d055cf216146637068`.
The runners deleted those proofs and decompressed inputs. These are bounded
diagnostic certificates, not independently checked final UNSAT proofs.
Both use 12 logical CPUs, 48000 MiB, one bound solver, 1200 solver-only seconds,
a 25-minute allocation, no node constraint/throttle, and separate immutable
pins. No Kissat timing reference is refreshed.

## Tests and failure classification

The actual callback fixture checks inclusive counter and conflict boundaries,
signed exports, fixed flags, repeated/opposite-polarity and rearmed suppression,
and observer-state preservation. A deliberate second invocation confirms
no-overwrite failure (exit 2) and identical retained output. This expected
negative control is not a solver crash.

Nine new material-reader unit tests cover exact large integers, complete
consecutive events, missing/truncated/duplicate records, invalid bounds,
fields/tags/numbers/flags, invalid transitions and nonmonotone histories.
All 149 benchmark-tool tests pass. Existing lifecycle-reader coverage is reused
because the follow-up changes only observer configuration, not that format.

The first fixture compilation failed because native's literal macros capture
a variable named `solver`; the fixture used `s`. The failed log is retained,
the alias was corrected, and only the exact unsubmitted fixture stage resumed.
No benchmark had run then. The first 6555 validation was too strict about
all admission calls at the selected conflict being invitations. It failed
before writing its summary; the complete cached-identical stream showed one
invitation followed by 54 post-elimination declines. Validation was corrected
to retain and check both phases, then frozen once. Neither failure is a solver,
proof, model or soundness failure; no successful writer was rerun.

## Next step and remaining scope

The next safe diagnostic is to confirm the selected pair's lifecycle through
the same boundary on current 6424 Tactix with its existing observer, while
retaining an exact ordinary-stream/proof control. Then use the concrete producer
and the fixed 6547 counter lifecycle tests to constrain the still-unapproved
general material tracker. Keep real additions distinct from forward-tail
rearming, root/removal invitations and unrelated consumers; admission remains
strategy-owned and public explicit rule actions remain available.

Full 6439 remains 50/50 checked/clean at 8731.653 / fixed 7239.010 =
1.206194355; older checked 6169 is better. The fresh evaluation and same-hardware
41–50 references are already completed, not repeated here. All fifty and memory
remain scored. Existing independent checks stay asynchronous. Audit 6559 owns
current accounting, hashes, tool/doc checks and report provenance. No live
checker scratch or Claude-owned fuzz access, denied transfer retry or push.
Andrew's local report is in `report-to-andrew/20260909-1310-rank50-material/`,
not sent. The goal remains unmet.
