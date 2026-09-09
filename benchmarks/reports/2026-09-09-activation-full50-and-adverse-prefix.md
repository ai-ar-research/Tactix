# Activation repair: complete50 regression and next causal controls

## Outcome and decision

Do not promote 6424 or claim an aggregate improvement. It repairs Rank 45's
observed scheduling discrepancy and preserves Rank 32's decision closure, but
the complete Rank 1–50 solver view regresses against predecessor 6383.
Keep it isolated while measuring its adverse changes and locating the older
Rank 35 regression. Production remains unchanged; the target is not met.

Primary authority: [6441 summary](2026-09-09-activation-full50-6441/summary.json)
and [all-50 comparison table](2026-09-09-activation-full50-6441/comparison.tsv).
All 50 expected outcomes returned, split 25 SAT / 25 UNSAT. There is no solve
loss, timeout transition or excluded invalidity in this snapshot. Thirty-seven
candidate certificates/models were independently checked when it was frozen
at 08:58 EDT; all 50 fixed references are checked. Remaining checks are
asynchronous and unverified. Audit 6462 owns subsequent checks and cleanup.

| View | Activation 6424 seconds | Parent 6383 seconds | Fixed Kissat seconds | 6424/Kissat |
|---|---:|---:|---:|---:|
| All 50 | 8731.653 | 8660.228 | 7239.010 | 1.206194 |
| Original 40 | 7531.562 | 7437.009 | 6810.066 | 1.105946 |
| Added 10 | 1200.091 | 1223.219 | 428.944 | 2.797780 |

The overall gap is 1492.643 seconds, of which added ranks contribute 771.147.
The new total is 71.425 seconds above its predecessor: original40 loses 94.553
seconds while added10 gains 23.128. These cross-window numbers do not isolate
instruction cost. Sixteen ranks change certificate identity and twenty change
at least one non-timing statistic; all are retained in the full table.

## Rank45 improves, but regressions outweigh it

Rank45 completes in 44.239 seconds versus parent 71.132 and fixed Kissat
45.187. Its 2,754,580 conflicts and 3,611,660 decisions now exactly match native
full totals, down from parent 4,435,138 and 5,893,257. The earlier complete
184,622-row/100K replay and restored 32/5 binary deletions provide the bounded
causal link. Full totals are not full-trace or physical formula identity, and
propagation-counter scopes still differ. Its final UNSAT proof is checked.

The largest changed-work parent losses are:

| Rank | Status | Parent → candidate seconds | Change | Parent → candidate conflicts |
|---|---|---:|---:|---:|
| 33 | UNSAT | 298.160 → 354.284 | +56.124 | 4,223,604 → 5,189,038 |
| 17 | SAT | 26.759 → 76.738 | +49.979 | 11,820 → 70,066 |
| 43 | UNSAT | 119.556 → 125.875 | +6.319 | 26,089 → 34,848 |

Rank17 remains faster than fixed Kissat's 113.158 seconds, but its lost
advantage still harms the aggregate. Rank33 is now slower than fixed Kissat's
311.385 seconds despite fewer native-relative conflicts; raw counts alone
are not cost parity. Its decisions also rise from 148,810,135 to 178,461,826.
Do not conceal these losses behind Rank45's 26.893-second gain.

Rank4 remains 7.528 versus fixed 7.681 seconds, with equal full conflict and
decision totals. Rank32 remains 325.044 versus 274.456, also with equal full
conflict/decision totals and the preserved 153,176,971-row/3M closure. Thus a
cost investigation is appropriate there; these are not whole-state identities.
The isolated 6448 shared-blocker experiment addresses that cost hypothesis and
does not yet have measured timing. Its B/C solver gate passed later in this checkpoint.

## Independent paired timing campaigns

The already-declared [6443 repair witnesses](../runs/20260909-6443-activation-three-wave-runtime/manifest.json)
stay unchanged: ranks 32, 35 and 45, three paired activation/6383 waves. Their
full-run work/certificate oracles became available, so wave0 **1677986** was
submitted **2026-09-09T08:51:28 EDT**, without waiting for proofs.

The complete50 result motivates a separate
[6464 adverse campaign](../runs/20260909-6464-activation-adverse-three-wave-runtime/manifest.json).
Its deterministic selection is the three largest parent runtime losses among
changed-certificate ranks in frozen 6441: 33, 17 and 43. Matrix order is
17/33/43, with the middle wave reversing arm order. Wave0 **1678054** was
submitted **2026-09-09T09:00:56 EDT**. This adds a new question; it does not
replace, selectively repeat or remove any existing witness or full50 score.

Each campaign requires all three independent pairs, exact own-arm work and
certificate replay, independently checked final outcomes and complete arm
spreads. The established 2% spread condition governs absolute claims. Helpers
6444/6465 submit the next wave only after the preceding six expected solver
outcomes; 6445/6466 freeze summaries only after all18. SAT17 uses the existing
SAT model checker and accepted-status classification, not an UNSAT-only shortcut.
Pending checks permit development, never final acceptance.

## Rank35: locate the later adverse split

Current Rank35 still uses 8,942,669 conflicts and 206,706,026 decisions versus
earlier6305's 4,434,673 and 186,209,089. The earlier6357/6305 paired timing
6382 already confirmed median 1.518663 (range 1.509089–1.518734); that exact
paired number is not relabeled as a new6424 measurement. The old6389 trace
matched all99,197,378 decisions through1M conflicts. Activation6424 preserves
headline full35 counts versus6383, but changes proof identity and sixteen
TransitiveReduce ticks, so its old prefix must be revalidated rather than assumed.

[6459 later-prefix campaign](../runs/20260909-6459-rank35-later-prefix/manifest.json),
job **1678013** submitted **2026-09-09T08:54:43 EDT**, compares current6424 and
earlier6305 through3M conflicts. It reuses the bounded observer and independently
pinned arms, emitting the suffix from1M. Validator6461 first requires both
99,197,378-row common hashes `140843d3c85c31da`, then validates the complete
threshold overlap and every suffix row, with nearby projected boundaries.
A changed common hash requires earlier localization, not a false inherited
closure or a solver-failure label. There is no new native reference run.
Partial-proof hashing/deletion is diagnostic cleanup, not final-proof acceptance.

## Memory, validation and remaining gates

Memory convergence remains open independently of timing. Rank45 falls from
84,496 to64,268 KiB, but native uses31,976. Rank32 is590,456/299,980 KiB;
Rank4 is29,512/13,008 and Rank25 is3,408,252/888,820. The largest runtime gaps
remain50(SAT,+378.832s),31(UNSAT,+212.595),35(UNSAT,+193.357),
21(SAT,+152.214),49(SAT,+136.067), then43(UNSAT,+79.786).
The private FlatLists allocation redesign and Rank50 material tracker remain
unapproved; neither is implemented by a general continuation instruction.

No solver-source change occurred this checkpoint. Source6424's108 Release
tests in each independent build/107 trace and source6448's109/109/108 remain
valid immutable authorities. New wrappers reuse the tested complete-row,
overlap and malformed-boundary controls, 65 benchmark tests and13 SLURM tests.
Test consideration: the new adverse matrix admits both expected SAT and UNSAT
labels using existing tested classification and checker paths; no parser,
runner or checker behavior changes. All18 exact matrix/input/own-oracle checks
remain required. The new Rank35 wrapper runs the maintained negative controls
before interpreting its complete streams. Documentation checks remain mandatory.

All runtime arrays retain12 logical CPUs,48000MiB, one bound solver/thread,
1200 solver-only seconds,16000 checker seconds and5h allocation, without hard
node selection or throttle. Bounded6459 uses25 minutes. All successful summaries
are frozen and must not be rerun. Shared-blocker gate6451/job1677969 now has all
66 expected outcomes, 64 checked at frozen6453, and all33 candidate work/proof
pairs exactly matching6424. B/production ratio is0.731624 and C is0.930346;
these are gate observations, not isolated cost gains. Cost campaign6454 wave0,
job1678060, was submitted2026-09-09T09:06:22 EDT without waiting for proofs.
Continue6455 waves1/2 after preceding six expected outcomes,6456 afterall18.

The accepted fresh reevaluation6227 remains100 checked results with zero residue.
Fixed41–50 matched6108 remains20 checked; the fixed50 reference is unchanged.
No new solver/proof/model/build failure, manual deletion, source promotion,
fuzz inspection, denied transfer or push retry. Current-source fuzz remains
Claude-owned/unavailable. Andrew's local report is
`report-to-andrew/20260909-0900-activation-full50/report.pdf`, not externally sent.
Two initial PDF overfull lines (9.17638 and9.5363 points) were caught before
the audit. The original log is retained; the paragraph was reflowed, with two
clean compilations required before landing.
