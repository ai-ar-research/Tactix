# Complete phase-budget runtime view and next loss

## Outcome

Campaign 5454 now contains all 80 expected solver results: both arms solve all
40 instances, 22 SAT and 18 UNSAT, with no timeout. Proof-on solver totals are
**7939.013 / 6783.841 seconds = 1.170282883x Kissat**. The
[complete snapshot](../runs/20260907-5454-eliminate-limit-ranks1-40/solver-checkpoint.json)
has 63/80 checked certificates; pending checks are provisional passes for
continued investigation, never final checked acceptance. This is the full
requested scoring view, not a partial projection.

The 1.01x objective remains unmet. Production is unchanged at checked
1.2787518x. The preceding phase-entry run 5434 is now fully checked and clean
at 1.205579786x. Different campaign windows prevent treating the ratio change
as a paired causal speedup. Candidate 5450 also includes the small Vivify and
replacement-cost optimizations; this experiment does not isolate each change.
The [phase-budget report](2026-09-07-eliminate-phase-budget.md) owns the code,
source-backed strict stop and exact causal prefix repair.

## Work changes and diagnostic selection

The [ranked losses](../runs/20260907-5454-eliminate-limit-ranks1-40/next-losses.json)
retain all 40 ranks, complete old/new work, memory, input identity and source
snapshot hashes. Every referenced record hash was revalidated before selection.

| Rank | Tactix / Kissat seconds | Time ratio | Conflict ratio | Extra seconds |
|---|---|---|---|---|
| 31 | 418.509 / 264.298 | 1.583474 | 1.542264 | 154.211 |
| 37 | 485.468 / 345.455 | 1.405300 | 1.214322 | 140.013 |
| 32 | 414.467 / 276.584 | 1.498521 | 1.400319 | 137.883 |

Rank 37 changes from 11,972,299 to 7,764,161 conflicts; its propagations fall
from 4,553,228,017 to 3,694,823,040. The latter is now approximately equal to
Kissat's 3,692,400,046, despite a remaining 40.5% time excess. Rank 32 likewise
changes work materially. Rank 31 retains exactly the preceding candidate's
7,003,044 conflicts, 212,528,693 decisions and 3,337,062,094 propagations.
It is now the largest absolute time loss, justifying another causal target
without abandoning the existing Rank-37 bound investigation.

These counts do not decompose unit cost exactly: operation mixes, timer scopes,
proof volume and inprocessing work differ. Peak RSS also remains an independent
gap. Rank 31 is 581484/265752 KiB; Rank 37 is 631940/269236; Rank 32 is
621988/299980. Full-view arm maxima remain 15967668/15348304 KiB, which cannot
discharge the much larger per-instance ratios. No memory repair or acceptance
is claimed by these aggregate maxima.

## New causal probe

**5470 / job 1670801** uses the immutable bound candidate's trace pin 5460 and
native observer 5414, copied into its own campaign pins. It records both
complete decision streams and normalized inprocess/macro boundaries from zero
through 100000 conflicts on Rank 31, `oski15a01b40s_opt`. The
[manifest](../runs/20260907-5470-rank31-bound-prefix/manifest.json) owns exact
input and binary hashes. Full-width two-task array, one proof-on solver per
12-logical-CPU/48000-MiB task, no throttle or node constraint. Controlled partial
certificates are hashed and deleted by the runner; this is not a complete
proof or acceptance-timing run.

The comparator reuses the established all-row, pin/hash, stop and cleanup
oracle. A divergence selects an owning mechanism; it does not automatically
establish a defect or weaker simplification. No solver code changed this turn.
Under the new unit-test consideration requirement, no solver unit test is
needed for this data-selection/campaign-only change. Existing comparator tests
and hash/identity assertions cover the reused machinery; real 5470 result
validation remains pending. No new duplicate fuzz campaign is warranted.

## Newly checked gates

- [5434 full view](../runs/20260907-5434-inprocess-root-ranks1-40/summary.json):
  80/80 checked and clean, both 40 solves, ratio 1.205579786x. This replaces
  provisional certificate status, not the measured times.
- [5440 Vivify timing](../runs/20260907-5440-vivify-max-rank35-runtime/summary.json):
  six checked proofs, identical complete work/proof bytes per pair, median
  0.995068538x predecessor, full range 0.988733698–0.997473130. Both absolute
  arm spreads remain below 2%; the local gain remains small.
- [5441 Vivify B28+C5](../runs/20260907-5441-vivify-max-BC-gate/summary.json):
  66/66 checked and clean, no solve loss/timeout; B ratio 0.810248684, C
  0.986912801, combined 0.922119895 against production. This does not close
  newer candidates' gates or their fuzz obligation.

Cursor 5447/5448, phase-budget 5454/5455 and newer bound/watch gates remain
independently owned. Their pending proofs are not a reason to suspend diagnosis.
Bound confirmation 5458, short B witnesses 5462 and Rank-37 prefix 5463 remain
queued at this checkpoint. Their pins and launch files are unchanged.

## Handoff and limits

Next inspect 5458/5462/5463/5470 terminal diagnostics, complete the newest full
5459 scoring view in 5465, and evaluate watch append 5468 only after all three
exact-work/proof-byte pairs exist. Seal checked/clean B28+C5 gates independently.
The latest source remains isolated 5466; no production promotion occurred.

Background Claude fuzz owner session 30094 remains live on 5438. Only its
terminal response is polled; no fuzz job or artifact was inspected. The next
periodic checkpoint must cover 5445, 5450, 5459 and 5466. A reported correctness
failure interrupts dependent work. No manual data deletion, push retry, or
separate-checker contract change occurred; push remains unapproved. The scoped
validation index and required Andrew TeX/PDF own final checkpoint validation.
Living docs stay within their required lengths.
