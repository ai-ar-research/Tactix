# Factor input parity and the later Rank33 split

## Outcome

The latest full measured candidate5522 remains **7831.998/6739.060 =
1.1621795918x Kissat** over Rank1–40. Both solve all40 (22SAT/18UNSAT),
without timeouts. The within1% objective is not achieved. No new implementation,
measured optimization gain, or production promotion is claimed in this checkpoint.
The previous goal-development turn made progress; this turn adds validated
profiles and causal evidence that change the next investigation.

Current source is the isolated5522 Sweep repair, not production. Its separate
5546 Vivify-census successor still has87/87 tests twice and192 actual old/new
state/proof fixtures, but B28+C5 5550 is queued and timing5551 remains unsubmitted.
The latest profile does not measure5546. Source/pin authorities remain in the
5522 and5546 manifests; submitted pins were not changed.

## Clean current-source profile

`20260908-5544-small-rank39-profile/summary.json` now validates both complete
proofs and cleanup. Both arms reproduce the unprofiled5535 Rank39 work counters
and proof hashes; all Tactix non-timing statistics agree. Both have zero lost
samples. Tactix has5,937,166 conflicts,97,303,843 decisions and2,368,197,521
propagations; Kissat has5,803,404/102,885,243/2,545,310,923.

Tactix's profile reports421.983s total,165.202s in its inprocessing slot (39.1%)
and256.780s outside. Sweep65.466s and Vivify41.686s are the largest timed rule
families. Native reports361.97s total,120.19s simplify,51.61s Sweep and37.02s
Vivify. These scopes are not identical, and cross-arm work differs: these are
profile observations, not matched per-operation cost or acceptance timing.

Tactix's highest self samples are kernel propagation38.90%, embedded propagation
7.17%, ignore-capable propagation6.06%, and conflict analysis4.79%. Vivify
scheduling is0.89%, plus0.61% in its candidate sort. Therefore the unused census
is worth measuring but unlikely by itself to remove much of a16.2% aggregate
gap. Its scan is only part of the0.89% function, not the entire9.9% Vivify timer.
The larger losses still require work/fidelity diagnosis alongside engineering.

## Rank32: actual Factor inputs

Parent5537 is a validated complete100K-conflict diagnostic:18,696,140 matching
decision rows before a split at conflict20095. Earlier four Sweep binary-count
differences and a later13-irredundant-clause Factor difference were only counts.

Companion5565 and5569 now reproduce **every per-arm parent decision, prefix,
boundary and macro trace**, plus the identical partial-proof bytes and SHA.
They stop cleanly and delete scratch. Native has no macro trace; no missing
native macro artifact is inferred. Their input snapshots establish:

- Two Factor passes occur at conflict19375. Binary snapshots retain both:
 212403 then206128 clauses, with exactly equal multisets including duplicates.
- First-pass large inputs are586000 irredundant plus2420 redundant clauses;
  both the literal multisets and physical clause order agree.
- Second-pass inputs are553534 irredundant plus2013 redundant clauses.
  All clause contents and physical clause order agree; all captured metadata
  (glue, priority, searched, used, reason, shrunken) agrees. Exactly two clauses
  have different literal pose: a127-literal clause at physical slot15563 and a
  four-literal clause at slot328592. The latter swaps205425 and205670.

`5569/input-localization.json` freezes the full first differing pose and hashes.
These snapshots do not include per-literal binary watch order, Factor marks,
heap scores or heap topology. The two pose differences are not yet a proved
cause of the13-clause split. In particular, binary-count differences earlier in
Sweep do not imply unequal binary multiplicity at either Factor entry.

5564 records accepted Factor grids using the existing fresh-only observer.
That mode suppresses macro input snapshots in both implementations;5565 is a
separate no-global-trace replay, not a fabricated replacement for missing data.
5565's single-shot large snapshot selects the first pass by default;5569
explicitly selects ordinal1. Strict comparator5566 initially rejected broad-
observer journal additions. Successor5574 requires every original row unchanged
and in order after accounting for exactly105 extra Tactix macro rows (one
trace-begin,15 retier,89 checkpoints) and one native analysis-end row at19375.
All complete decisions, prefix records and partial-proof bytes agree per arm.
The first4824 accepted grids agree;4825 chooses Tactix182673/182615 versus
native187350/187292, both2 factors,4 remainders,gain2,fresh494642. Heap5571,
job1672476, now captures the actual post4824 state; validator5573.

## Rank33: a later decision split, earlier clause difference

5558 validates all31,494,584 decision rows through100K conflicts as identical.
Extended5567 reaches1M with two clean stops and validates the prior common
literal-prefix hash and every previous boundary row per arm. Its complete
suffix from100K has18,013,384 matching rows, then the first split is
conflict182093, decision49396661, level7257: Tactix−120438, Kissat−121103.
The suffix count is not a count from startup. The legacy unsigned wrapped
`start_decisions` prefix field is not used as a work metric.

5572 locates the first portable clause-count difference much earlier:
Congruence end9457. Entry has583991 irredundant clauses in both; exit remains
583991 in Tactix but583986 in Kissat. Active429793, redundant2885 and binary
754302 agree at exit. This may precede the later decision split, but counts
alone do not prove which five clauses differ, that they cause the split, or
that native performed a stronger transformation. Next capture actual clauses.

## Validation, hygiene and continuation

Production's unchanged Release suite passes77/77 in6.06s. New diagnostic parser
fixtures cover missing/truncated snapshots, wrong conflict/boundary/ordinal,
duplicate multiplicity, clause framing, nonzero Factor ordinals, literal pose,
metadata, journal projection and heap membership/topology:19+9+8+6 cases. No solver unit-test edit is
needed for these configuration-only replays; existing immutable pins and exact
parent replay validate their observer neutrality. The staging helper now selects
small traces only from audit-completed results and verifies their hashes, avoiding
staging a still-growing trace. Previously staged native5558 boundaries are now
complete and validated; no frozen solver source or report was rewritten.

Parent5530 B28+C5 is now66/66 checked and clean,0.930592x production, no solve
loss. Full5535 had70/80 checks at this checkpoint's inspection; strict final
sealing still waits for all80. Timing5531 retains its frozen three-pair
median0.889407x5510; checks continue. Pending checks are provisional passes for
investigation only. Claude-owned fuzz5539 remains under live owner70659 and
covers5522, not5546; only its terminal response is polled. No fuzz artifacts
were inspected and no duplicate campaign was started.

No manual deletion occurred. Unique raw traces, manifests, pins and pending
campaigns are retained. The validation audit records exact current normal-job
and artifact hashes. Living documents remain within their size limits; PROGRESS
is350/350 lines. Push remains unapproved; no retry or production promotion.

Next: compare post4824 heap via5571/5573, inspect Rank33's actual
Congruence clause difference, and start5551 only after5550 B28 solver snapshots.
Keep correctness checking in the background and pause dependent work on an
actual reported correctness failure.
