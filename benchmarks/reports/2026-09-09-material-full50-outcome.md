# Material scheduling: repeated Rank50 gain and complete Rank1–50 outcome

## Decision

Retain approved isolated source6579 for continued development. The general
material scheduling repair has a reproducible Rank50 benefit, but does not
close all50 competitiveness, memory, or source-fuzz acceptance. Production is
unchanged. No additional solver change is made in this checkpoint.

The [complete table](2026-09-09-material-full50-6596/comparison.tsv) and
[frozen full summary](2026-09-09-material-full50-6596/summary.json) include every
rank: 50 expected solves, 25 SAT/25 UNSAT, no timeout or solve loss. At sealing,
36 were independently checked/clean and 14 proofs pending; audit6602 owns later
status, without rewriting the snapshot. The fixed Kissat vector is 50/50 checked.

| Scoring view | Candidate seconds | Previous6424 seconds | Fixed Kissat seconds | Candidate/Kissat |
|---|---:|---:|---:|---:|
| All50 | 8401.066 | 8731.653 | 7239.010 | 1.160527 |
| Original40 | 7583.667 | 7531.562 | 6810.066 | 1.113597 |
| Added10 | 817.399 | 1200.091 | 428.944 | 1.905608 |

No failures or timeouts means solve-time sums equal PAR-2 at this provisional
solver checkpoint. Full-set improvement versus parent is 330.587 seconds;
original40 worsens by 52.105 seconds. Older6169 scored 8403.998 seconds:
the 2.932-second difference is not a demonstrated gain over that older result.
Only ranks38 and50 change reported non-timing work and certificate bytes/hash.
The other48 exactly reproduce parent work/certificates; cross-window timing
and RSS differences do not isolate overhead or a causal allocation change.

## Rank50 replicated runtime

[6597](../runs/20260909-6592-rank50-material-three-wave-runtime/solver-checkpoint-6597.json)
binds three independent pairs, reverse middle order, exact own-arm work and
certificate replay, all6 independently checked SAT models and zero residue.

| Wave | Candidate6579 seconds | Previous6424 seconds | Ratio |
|---|---:|---:|---:|
| 0 | 45.539 | 427.198 | 0.106599 |
| 1 | 45.789 | 428.712 | 0.106806 |
| 2 | 45.637 | 426.397 | 0.107029 |

Median paired ratio is 0.106805968. Candidate and previous range/median are
0.5478% and 0.5419%, both below the predeclared2% bound. This is approximately
9.36x faster on this witness, not an all50 or isolated unit-cost claim.
Candidate conflicts/decisions fall from11797027/15960708 to1718121/2493760;
the latter match fixed Kissat. Full6594 independently returns45.639 seconds,
but was not substituted for any planned pair. Full candidate peak RSS49008KiB
improves on parent72192, yet is2.615x Kissat18740.

The causal [6589 closure](../runs/20260909-6587-rank50-material-schedule/summary-6589.json)
already restores the missing823562 invitation, all1459556 decision rows and532
normalized boundaries through1M conflicts, without an earlier split. Do not
rerun that closed native localization. The inherited6592 manifest's obsolete
B/C validation prose is not its acceptance authority: its explicit six-task
matrix, three-wave submission contract and acceptance section govern this run.

## Adverse result and remaining gap

Rank38 is the sole changed-certificate rank with positive parent time delta:
341.232 versus335.948 seconds (+5.284), fixed Kissat362.395. Conflicts increase
from18021141 to18430710 (+2.27%); decisions24152976 to24657248. Candidate now
matches native conflicts/decisions, but not a claimed identical trajectory or
propagation currency. Its single cross-window1.57% loss requires replication.

[Adverse6599](../runs/20260909-6599-material-adverse-rank38/manifest.json)
selects every changed-certificate parent loss from the complete6596 table,
exactly rank38. Three paired waves, reversed middle, reproduce each arm's own
full-run work/certificate and use the same2% spread requirement. Wave0 job1678406
was submitted16:32:43 EDT. Later waves proceed after expected solves, not proof
checks. This result is retained, not excluded or used to undo a general repair
merely to copy a fortuitous older search trajectory.

| Remaining witness | Candidate/Kissat seconds | Evidence and next direction |
|---|---:|---|
| Rank31 UNSAT | 479.076 /264.532 | 9019812 vs4540757 conflicts; search/scheduling gap |
| Rank35 UNSAT | 506.019 /306.699 | 8942669 vs4751990 conflicts; search/scheduling gap |
| Rank21 SAT | 249.607 /96.821 | 140595299 vs47246834 decisions; work and memory |
| Rank49 SAT | 158.615 /22.555 | 3499397 vs600411 conflicts; added10 search gap |
| Rank32 UNSAT | 326.399 /274.456 | Equal4636494 conflicts/182202021 decisions; cost candidate |

This is a prioritization, not attribution from raw counts alone. Rank32's
existing checked6372 profile places kernel propagation at34.38% self. Closed
Factor6544/6545, Congruence6541 and retention probes do not justify reopening
tiny-cost hypotheses. Rejected shared-blocker6448 remains rejected. Rank35's
6483 selected Vivify replay has no semantic/physical candidate difference and
only a small effort residual; that alone does not establish the later branch
split's cause. Reuse cached witnesses before any fresh diagnostic run. Preserve
the stronger Rank33 closure6519 rather than copying a weaker native omission.

Memory remains independent: candidate/native KiB are3142092/1436392 on21,
3413516/888820 on25,587228/299980 on32,603172/265332 on33, and4823660/3517460
on47. Largest candidate RSS16014884KiB on10 is below16GiB, but low absolute
peak does not excuse the large ratios. No FlatLists redesign is authorized.

## Validation, resources, hygiene and continuation

Source6579 and its110/110/109 suites plus24 fixed proof/model controls remain
unchanged; prior fixture failures and the first legacy micro-proof hash omission
remain disclosed in the [implementation checkpoint](2026-09-09-material-scheduling-cutover.md).
This is not fuzz. Claude-owned source fuzz remains unavailable: no access,
transfer, or launch is attempted, and missing fuzz blocks promotion.

New helpers only prepare/submit owned matrices or reuse maintained classification,
scoring and repeat summaries. No runner, parser, checker or solver behavior
changes. Test consideration: existing149 benchmark and13 watcher tests cover
these primitives; helpers assert complete identities, hashes, exact own-arm
work, all50 selection, score totals and all3 pair waves. Documentation checks
cover the status-only edit; no new unit case is needed for this evidence update.

All new solver tasks use12 logical CPUs,48000MiB,1200 solver-only seconds,
16000 checker seconds, five-hour allocations and one bound solver per task;
full-width arrays, no node constraint or throttle. Fixed12CPU Kissat is not
rerun/refreshed. Matched41–50/6108 and fresh6121/6227 remain closed. Protected
cpu-q5784 scripts, manifests and pins are hash-verified unchanged. No source,
reference, previous report or active checker scratch is modified or deleted.

Next: finish remaining adverse waves using6600, seal6601 once all6 expected
solves exist, and revisit B/C6584 and full6594 independent checks. Continue
mostly-UNSAT31/35 causal work and Rank32 cost investigation using existing
authorities. Runtime and memory convergence remain unmet. Audit6602 records
published results, exact accounting, protected hashes and validation; Andrew's
new local PDF accompanies this checkpoint. No push or report delivery is made.

One premature adverse wave1 request was rejected by the expected-outcome guard
before sbatch; it submitted no job and changed no campaign artifact. An initial
PDF line overflow was corrected before the final two compiles. Neither is a
solver/checker failure. Exact job launches, not attempted commands, own the run.
