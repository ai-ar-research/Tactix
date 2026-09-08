# Sweep retained-entry eligibility

## Outcome and scope

Candidate5510 on5473 restores a missing reference scheduling check. It is an
isolated, unfuzzed investigation candidate, not a production promotion or a
measured runtime win. The last full Rank1–40 result remains1.165337x Kissat;
the ≤1.01x goal is unmet. B28+C5 remains mandatory.

## Causal evidence

Rank32's old trace5490 agrees with native for the first14 seeds of Sweep at
conflict19375, then Tactix alone attempts836. Diagnostic5505 captures both
selected entry binary snapshots and the first selected large-clause snapshot.
Successor `compare-rank32-eligibility-5509.py` validates all pin/input/trace
hashes, clean stops and scratch deletion, then whole per-arm decision/prefix/
boundary identity against5490. Two positive/six negative framing fixtures pass.
Its frozen authority is
`benchmarks/runs/20260908-5505-rank32-sweep-eligibility/eligibility-validation.json`.

Both first entry snapshots contain exactly `(-841,836),(-796,836),(783,836),
(834,836)`; second entries replace-841 with-826. No large clause contains836.
The11 selected large clauses containing1236 are identical in both arms. Thus
836 is already one-sided at entry, not made one-sided by the preceding seed.
This does not independently prove complete physical-watch/active-flag parity,
or that skipping836 alone explains native's later unit-103918 or runtime.

Kissat `reschedule_previously_remaining` and CaDiCaL's same-named method both
check current activity and `scheduable_variable` before retaining an identity.
Tactix used its existing occurrence predicate only for fresh enumeration.

## Repair and contracts

Private `sweep_revalidate_retained` applies that existing predicate to live
front and remaining-tail slots once per call, after limits and occurrences are
ready. Rejected entries become tombstones; matching membership slots and live
counts are updated without reordering survivors. The call captures whether it
was retained before filtering, preserving existing completion ownership rather
than beginning another fresh pass when filtering drains it.

Eligibility is not rechecked at pop: a same-call equivalence representative can
gain bridge occurrences beyond the entry cap and is still processed. Pure
non-fixed seeds remain public instances and legal `sweep_try` requests. No
proof vocabulary, guard, primitive, policy option, budget or constant changes.
Native incomplete flags, fresh admission and completion remain a separate
parity obligation; this repair does not claim to implement that full lifecycle.

## Validation and failed expectations

Two independent Release builds pass85/85 and have identical solver SHA256
`5a54d60d90a8e8323e962267e6041347491c66a8875d71710bf960d70a5d37ed`.
Trace5511 passes84/84. The final new test linked against actual5473 objects
reproduces the attempted pure retained seed; the repaired build declines it.
Fixtures cover front/tail order, holes, slots, live counts, invalid/duplicate
identities, root-fixed/retired seeds, positive/negative caps, grown caps,
sparse/dense large occurrences, all-filtered completion, N1, and same-call
representatives crossing the entry cap. Existing proof/model/API suites pass.

Initial full suites failed an obsolete `resumed>=1` expectation: the fixture's
substitution leaves only an isolated representative and three retired variables.
The updated test proves that state before requiring zero resumed attempts.
Intermediate new-test expectations wrongly assumed same-call bridges had
already disappeared and sparse large watches used one word; observation showed
five bridge entries per polarity and two-word sparse watches. Those expectations
were corrected without further solver changes. No soundness, model or checker
failure was observed. Initial logs remain preserved.

Source patches, hashes and final logs are frozen under
`benchmarks/runs/20260908-5510-sweep-retained/manifest.json`. Production source
remains unchanged. Claude's live5492 campaign covers5473, not5510.

## Verified follow-up and measurements

- Trace5515/job1672164 is complete. Successor5518 validates hashes, clean stops,
  cleanup, native whole parent replay and both full candidate comparisons.
  All11023 native first-call seeds and environment sizes now agree. Unit-103918
  is restored, but Tactix continues seven extra seeds and derives-112830 one
  call early. First decision split is19375/18522692 after18583394 common rows.
- B28+C5 5516/job1672165 uses the unchanged66-task cumulative production gate.
- Timing5517 wave0/job1672166 returns UNSAT on both arms:438.175 versus410.609
  seconds, about6.7% worse; certificates pending. Wave1/job1672240 owns2-3;
  submit4-5 after both solver snapshots. Proof checkers continue in background.
  No favorable timing claim is supported; complete all three pairs and gates.

Aligned5520 joins effort to its actual seed/environment instead of shifting
rows when Tactix omits zero-effort records. Its join fixture and three negative
fixtures pass. First differing effort is seed340186:270 variables/one clause,
Tactix0 ticks/0 solves versus native814/1. All33 omitted environments have270
variables/one clause, costing26836 native ticks. Over the11023 common seeds,
Tactix spends54798904 ticks versus54824412; its seven extras spend25505.
There are36 differing effort records, so not every discrepancy is claimed to
be a missing initial solve. Frozen `effort-localization.json` owns this result.

Source inspection finds `sweep_process_seed` resets the embedded solver but
returns before importing/solving when vars<2 or clauses<2; native unconditionally
performs the initial solve. The next repair must test the actual tiny-environment
solve/model lifecycle against both references, not pad a meter with guessed
cost. Then reassess cumulative Rank1–40 and current-source fuzz. The current
candidate remains unpromoted and the objective is unmet.
