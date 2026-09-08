# Full Rephase view, measured metadata cost, and next boundaries

Production remains `c2a95a6`, checked Rank 1–40 ratio **1.2787518x**. The goal
of at most 1.01x Kissat is not achieved. This checkpoint changes the next
diagnostic targets, not production source or the benchmark/checker contract.
The [preceding report](2026-09-07-root-entry-followup.md) owns the two isolated
implementations and their tests; no new solver implementation is claimed here.

## Complete solver view; final checking remains separate

Campaign 5423 now has all 80 expected solver verdicts: both arms solve all
40 instances, 22 SAT and 18 UNSAT, with no timeout. Proof-on solver totals are
**8103.802/6741.743 seconds = 1.202033658x**. This is the complete requested
scoring view, not an extrapolated partial aggregate. The
[solver checkpoint](../runs/20260907-5423-rephase-root-ranks1-40/solver-checkpoint.json)
preserves every rank's work, time, memory, record hash and verification state;
64/80 certificates were checked at that snapshot. Pending expected results
are provisional passes for investigation, never verified acceptance.

Largest remaining positive deltas are Rank 37 (+277.501 s), 32 (+180.370 s),
31 (+157.042 s), 40 (+128.537 s), and 27 (+106.023 s). Rank-37 conflicts are
1.8725x Kissat. Peak RSS is 15,967,668 versus 15,348,304 KiB across the full
view; aggregate maxima do not discharge per-instance memory differences.
The ratio is descriptively lower than production's checked 1.2787518x, but
different campaign windows are not an exact paired speedup experiment.

Two previously pending gates now have final authority:

- [5421 timing replicas](../runs/20260907-5421-rephase-root-rank35-runtime/summary.json):
  all nine UNSAT proofs checked, scratch clean. Three-wave median paired ratios
  remain 0.658242907x the preceding candidate and 1.183227545x Kissat, with
  complete ranges 0.652884271–0.659718195 and 1.181525670–1.185729269.
- [5422 B28+C5](../runs/20260907-5422-rephase-root-BC-gate/summary.json):
  66/66 checked and clean, no solve loss or timeout. Candidate/production time
  ratios B 0.846424508, C 0.989190920, combined 0.936764341. This is the unchanged
  cumulative regression gate, not a full Tier-C/D competitiveness result.

## Metadata optimization: real, small, still unpromoted

All six 5428 solves have expected UNSAT verdicts. Every candidate/predecessor
pair has identical complete non-timing statistics and identical proof SHA-256.
The [three-pair checkpoint](../runs/20260907-5428-watch-truncate-rank35-runtime/solver-checkpoint.json)
reports median paired **0.993882186x**, range **0.989911575–0.999527763**:
about a **0.6%** median reduction, not a large contribution to the goal.
Candidate median is 361.467 s (359.132–361.935); predecessor median 362.792 s
(362.106–363.692). Both complete arm ranges are below 2% of their medians.
Checks remain pending, so the exact proof-byte comparison is not a proof check.
The fixed B28+C5 campaign 5429 remains separate and in flight. The change is
still independent of phase-entry 5430; no combination or promotion occurred.

## Clean profiles move attention toward inprocessing

Both [5425 profiles](../runs/20260907-5425-rephase-root-profile/solver-checkpoint.json)
have zero lost samples and reproduce conflicts, decisions and propagations
from all three unprofiled 5421 replicas of their respective arm. Pins, report
hashes and compressed profile identities were checked. These are clean Release
profiles, not the previously rejected dormant conflict-stop instrumentation.
Both proof checkers continue running.

Tactix's own timers report 148.087 s in inprocessing and 213.956 s outside it.
Kissat reports 81.30 s simplify and 219.44 s search. Timer scopes and trajectories
differ; this is prioritization evidence, not subtraction establishing unit cost.
Sweep is 30.314 versus 7.12 s, but Tactix runs 39 rounds, 627,937 sub-solves,
1,114 equivalences and 60 units, versus Kissat's 25 rounds, 379,501 sub-solves,
495 equivalences and 36 units. Neither the whole Sweep difference nor the
whole inprocessing difference can be assigned to engineering overhead.

Self samples include Tactix propagation 32.32%, heap-down 4.80%, analysis 4.62%,
ignored-clause propagation 4.35%, watch rebuild 2.45%, in-place replacement
1.84%, Vivify scheduling 1.66%, and Sweep-session rewrite 1.02%. Static review
finds repeated membership searches in large-clause rewrite and substantial
Vivify scheduling work, but no new ablation establishes a winning change.
Do not convert these unmatched samples into a claimed per-operation slowdown.

## First decision split is not the first coarse state split

[5424](../runs/20260907-5424-rank35-rephase-prefix/summary.json) has two controlled
proof-on stops at 100,000 conflicts, recorded partial-certificate hashes and
clean scratch. Both preserve the established 18,516,218-row prefix hash
`a6eb1df4f4e0e591`. The window from conflict 13,833 contains another 7,399,407
identical decision rows before the first split: conflict **32,072**, ordinary
decision **25,844,533**, level 249, Tactix **148432** versus Kissat **147866**.

Within that later Probe phase, the second Sweep changes active counts from the
same 147,396 to 147,373/147,380. That is not evidence that Tactix is weaker.
More importantly, aligning all earlier portable rule boundaries finds an
[earlier count discrepancy](../runs/20260907-5424-rank35-rephase-prefix/boundary-localization.json)
at Sweep end, conflict **19,365**: active 162,868, irredundant large 571,429 and
redundant large 1,857 agree, but binary counts are **204,961/204,963**. Counts
and later matching decisions do not prove the formula, duplicate multiplicity,
watch order or pending control state were identical. That earlier boundary,
not the eventual branch choice, is the next causal target.

## New diagnostics and continuation

- **5436 / job 1669994:** latest phase-entry trace pin 5431 versus native fast
  observer 5414 on Rank 37, complete decision stream through 100,000 conflicts.
  Comparator retains the earliest portable split. This targets the largest
  measured current loss without first modifying its trajectory.
- **5437 / job 1670050:** Rephase 5417, phase-entry 5431 and native 5414 on Rank
  35, stopped at conflict 19,366. Selected Sweep formula digests and full binary
  multisets at conflict 19,365 distinguish duplicate removal, unique-clause
  differences and root-entry effects. Prefix identity and nontruncation remain
  required; snapshot differences still require causal interpretation.

Both use existing immutable pins, full-width arrays, one bound proof-on solver
per task, 12 logical CPUs/48,000 MiB, no node constraint or throttle, and bounded
cleanup-owning runners. No real solver was run outside SLURM. Campaigns 5433–5435
continue their previously declared phase-entry prefix/full-view/regression work.
No pending proof check blocked investigation; any actual rejection must pause
dependent work. No new fuzz campaign or fuzz artifact was inspected.

Production fixed CTests pass 77/77 (5.90 s). Living docs remain within all
30 budgets. Detailed validation is in the companion validation index. All
active pins, sources and job-owned checker scratch are retained; no manual
deletion occurred. The completed 5424 runner deleted its own bounded input and
partial proof scratch after hashing; no full solve was claimed. User allocation
1668242 is untouched. Separate future solver/checker jobs and push remain
unapproved; neither contract changes nor a push retry occurred.
