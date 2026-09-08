# Full bound-repair runtime view and the next causal losses

## Outcome

Full campaign **5465** now contains every Rank 1–40 solver pair. Tactix/Kissat
proof-on solver totals are **8069.874/6874.899 s = 1.173817099x**. Both solve all
40 cases, 22 SAT and 18 UNSAT, no timeout or verdict loss. The
[solver snapshot](../runs/20260907-5465-bound-advance-ranks1-40/solver-checkpoint.json)
has **58/80 checked and clean**, with the remaining certificates pending.
Investigation proceeds; these are not 80 verified results or a promotion gate.

The at-most-1.01x goal is not met. This is no aggregate breakthrough over the
preceding complete 5454 view of 1.170282883x. Campaign windows differ, so their
small ratio difference does not establish a paired causal timing regression
or gain. The new candidate's large Rank-37 work increase is independently
visible in deterministic solver counters and selects the next investigation.
Production remains unchanged at checked 1.2787518x; later 5466/5473 are not
included in this full view and retain their separate pending measurements.

## Residual work and cost

[Next-loss analysis](../runs/20260907-5465-bound-advance-ranks1-40/next-losses.json)
validates both complete snapshots and every source-record hash and instance
pair. It retains all forty ranks, work ratios, preceding work, peak memory,
selected component statistics and verification status; no rank is removed.

| Rank | Tactix / Kissat s | Excess s | Conflict ratio | Propagation ratio |
|---|---|---|---|---|
| 37 | 712.017 / 346.009 | 366.008 | 2.114745 | 1.265941 |
| 32 | 421.021 / 274.413 | 146.608 | 1.367353 | 1.263398 |
| 31 | 417.482 / 283.258 | 134.224 | 1.542264 | 1.164893 |
| 27 | 358.706 / 240.141 | 118.565 | 1.239047 | 1.107322 |
| 39 | 443.315 / 364.614 | 78.701 | 1.115283 | 0.926696 |

Rank 37 now has 13,521,309 conflicts, 244,585,735 decisions and 4,674,359,400
propagations; the preceding candidate had 7,764,161, 206,459,604 and
3,694,823,040. Kissat has 6,393,826, 215,248,091 and 3,692,400,046. The new
candidate therefore does materially different and more full-run search work
despite matching every recorded decision through 100000 conflicts. That bounded
closure must not be generalized to the full solve. Current excess time is not
explained by unit cost alone; counters do not establish a per-operation cost
because the paths and operation mixes differ.

Its own timers show 246.430 s inprocessing and 465.365 s outside the slot.
Vivify accounts for 72.454 s and Sweep 69.255 s. Sweep runs 77 rounds and
1,222,523 embedded solves. These identify work to inspect after the earliest
divergence, not a license to cut their budgets or attribute all their time to
implementation overhead. Source-backed bound repair 5459 remains the baseline
while the next coupled mechanism is localized; the old compensating lifecycle
is not restored solely to recover one trajectory.

Only Ranks 9, 21, 32, 37, 39 and 40 change the three reported Tactix work
counters from 5454. Rank 31's counters remain exactly unchanged and its queued
trace is still an independent priority. Rank-37 peak RSS is 631940 versus
269236 KiB; Rank 31 is 581484 versus 265752 KiB. These memory gaps remain open.
Full-view maxima 15967668/15348304 KiB neither discharge per-rank differences
nor identify live allocation owners.

## New diagnostics

**5478 / job 1671258** extends Rank 37 to one million conflicts with the same
immutable candidate/native observers and input as validated 5463. Its complete
recorded decision suffix begins at conflict 100000; the earlier 35,346,388-row
common prefix has separate hashed authority. Whole-prefix digest, suffix rows,
all trace/pin hashes, controlled stop and cleanup are required. Report suffix
matching counts as suffix counts, never as all decisions from startup.
Watcher session 30072 owns the job wait; proof checking of the full run is
independent and continues in the background.

**5479 / job 1671263** selects Rank 32, now the second-largest full-view loss.
It uses the same trace pins, records all decisions from zero through 100000
conflicts, and checks normalized boundaries before choosing any repair. Pending
Rank-31 5470 and short Square 5471 remain in scope, not replaced by these probes.

Both new diagnostics are full-width two-task arrays, one proof-on solver per
12-logical-CPU/48000-MiB task, no throttle or node constraint. Existing runners
hash and delete partial certificates and scratch; stopped partial proofs do not
count as checked complete verdicts. No solver source or runner contract changed.
The unit-test consideration found no uncovered solver transition introduced by
these campaign/analysis changes: existing full-stream comparators and fixed
tests apply, while all eighty actual records and predecessor hashes validate
the new analysis. Production tests pass **77/77 in 5.03 s**.

## Continuation and limits

The wrap-up skill's scoped validation and mandatory Andrew TeX/PDF own the
documentation and retention audit. All unique submitted pins, runners and raw
trace authorities are retained; no manual deletion occurred. Pending B28+C5,
watch-pair/Sweep timing and predecessor certificates remain independent gates.
Claude fuzz owner session 30094 is still live on 5438; no fuzz job or artifact
was inspected. Its next periodic checkpoint must include changes through 5473.
Actual correctness failures pause dependent work; pending checks never certify
promotion. No push retry, production promotion, or separate-checker contract
change occurred. Push remains unapproved. Next localize the earliest relevant
later Rank-37 boundary and compare the independent Rank-31/32 and Square traces.
