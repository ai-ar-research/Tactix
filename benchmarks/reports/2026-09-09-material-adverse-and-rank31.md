# Material repair: adverse replication and next UNSAT localization

## Decision

Retain source6579 as isolated development, including the adverse Rank38 result.
Do not promote it, refresh fixed Kissat, or rewrite completed snapshots.
All50 remains8401.066/7239.010=1.160526923, with every rank scored; this is not
convergence. B/C6584 is now66/66 independently checked/clean. At this read,
full6594 is42/50 checked/clean, eight UNSAT checks remain asynchronous.
[Audit6609](2026-09-09-material-followup-audit-6609.json) owns later status.

## Rank38 adverse repetition completed at solver level

[Frozen6601](../runs/20260909-6599-material-adverse-rank38/solver-checkpoint-6601.json)
includes all three predeclared pairs, reversed middle order, and exact per-arm
reported non-timing work and proof hash/bytes against full6594/6439 oracles.
Two results are independently checked/clean at sealing, four proofs pending.

| Wave | Candidate6579 seconds | Previous6424 seconds | Candidate/previous |
|---|---:|---:|---:|
| 0 | 341.285 | 336.577 | 1.013988 |
| 1 | 340.945 | 337.682 | 1.009663 |
| 2 | 341.493 | 337.888 | 1.010669 |

Median paired ratio is1.010669216; within-arm range/median is0.16057% candidate
and0.38824% previous. Both satisfy the predeclared2% spread rule. The small
slowdown repeats; independent proof completion remains necessary for a checked
gate. No selective extra wave, exclusion or attempt to erase the adverse result.
The candidate's native-matching conflict/decision counts do not prove that the
older cheaper trajectory was incorrect, or that both complete formulas agree.

Wave1 job1678408 since16:39:11 and wave2 job1678410 since16:45:24 both proceeded
after expected solves, without waiting for predecessor proofs. All three
submissions now exist: never rerun6600 or successful summary6601. The independent
Rank50 three-wave gain remains closed by6597, not rerun here.

## Rank32 cost attribution: no speculative phase-saving rewrite

Offline [annotation6604](../runs/20260909-6604-rank32-propagation-annotation/manifest.json)
uses the already checked, zero-lost-sample6372 pair. It verifies unchanged
propagation and assignment source sections between6357 and current6579, not
whole-source or linked-image identity. Both original compressed profiles and
pins are hash-bound. Eight nonempty annotations were generated; exactly
1010452800 bytes of owned temporary profile expansions were removed. Compressed
inputs, prior reports and active checker scratch were untouched.

[Summary6608](../runs/20260909-6604-rank32-propagation-annotation/summary-6608.json)
uses the maintained strict instruction-period parser and reviewed exact address
ranges. The two ordinary-search phase-saving sites account for0.17878% and
0.18614% of candidate program sample periods, combined0.364916%. Those sites
include useful phase saving, not exclusively removable checks. This does not
justify a new phase-saving specialization as the main Rank32 gap-closing step.

Watch decode/blocker and first clause-load/garbage-test regions account for
7.02478% and4.67540% of program sample periods. Sampling skid prevents attributing
those weights to individual loads or branch misses. No prefetch, metadata,
watch-order or shared-blocker change is selected. Rejected6448 remains rejected.
The search symbol has421489892971 candidate versus453108192545 native sample
periods: a large candidate percentage alone does not establish slower BCP.
Function scopes, physical work and propagation currencies are not interchangeable.

The candidate probing annotation is only its wrapper (0.15156% of program),
not the separate4.61%-self `propagate_literal` callee; no probing cost ceiling
is claimed. Recorded6372 candidate timing is122957ms inprocessing out of327195ms:
Vivify36238, BVE19138, Factor14531, Substitute13131, Congruence10596, Sweep8213,
Strengthen7579, Backbone3553, TransitiveReduce3180, unattributed6797ms. These
are owned timing scopes, not native-normalized work or a new performance gain.
Keep cost diagnosis open, but pursue the larger unresolved search gap as well.

## Current Rank31 against retained native trace

Complete6596 selects Rank31 as the largest UNSAT excess:479.076/264.532 seconds,
9019812/4540757 conflicts. The old preclean5893 current/native split at394397
does not automatically describe current6579. Native5893 retains a complete
300K-to1M suffix, and5735 owns the common300K prefix89336421 rows with hash
`f7f8f88b101c79fa`. Current-source comparison must establish that premise first.

New [6605](../runs/20260909-6605-material-rank31-prefix/manifest.json), job1678413
since16:49:47 EDT, runs one immutable6579 trace binary through1M conflicts.
No new native invocation or timing-denominator update. Source/pin hashes,
all66 checked B/C results, the complete all50 selection and cached native traces
are dependencies. Runner6376 is reused with only candidate arm and scratch label.
Twelve logical CPUs,48000MiB,1200 solver seconds,25-minute allocation, no node
constraint/throttle. Partial proof is hashed/deleted, not a final proof check.

Validator6607 binds every complete trace before interpretation. A changed
common prefix is reported as `earlier_prefix_changed`, not a solver failure;
then localize before300K rather than attributing the old split. If common,
compare every suffix row and retain the last portable boundaries before the
first changed decision. Equal counters/boundaries do not prove formula or effort
identity. No rank-specific behavior, public guard narrowing or scheduler edit.

Completed6607 reports `earlier_prefix_changed`: current common-count hash is
`3fdfee87c3823cf4`, not native `f7f8f88b101c79fa`. The current1M prefix counts
123392789 decisions, with27074947 retained suffix rows; native has26026009
suffix rows. No later split is interpreted when the inherited premise fails.
This is diagnostic evidence, not solver failure. The1M stop is terminal/clean.

A read of the bound boundary streams finds the first search-row count difference
at ordinal359, BVE end179823. Preceding Strengthen begin/end and BVE entry counts
match. BVE exit candidate/native active counts are94981/94998, irredundant
554257/554333, binary134599/134624; decisions76834765 and redundant23092 match.
This brackets a difference, not its first semantic cause or relative strength.

New [early6611](../runs/20260909-6611-material-rank31-early/manifest.json),
job1678415 since16:58:40 EDT, requests the missing current0..300K decision stream
against cached native5735. Same immutable binary/resources; no new native run.
This is not a repeated acceptance measurement:6605 retained only rows after300K
and cannot locate the changed earlier prefix. Validator6613 compares every early
row with full-stream accounting and no common-prefix premise.

Early6611 completed cleanly. [Frozen6613](../runs/20260909-6611-material-rank31-early/summary-6613.json)
compares all complete early rows and finds76952538 matching decisions. First
split: conflict179823, decision76834833, level68, current literal-177741 versus
native-173977 (both non-warming). The preceding BVE/count bracket is confirmed
by the early boundary stream, not inferred from a later prefix. This selects
the first179823 BVE invocation's input, heap/scores, definition/core order and
effort for the next causal comparison; do not alter tie-breaking from the pair.

New maintained `validate_suffix_accounting` handles zero threshold overlap:
native5893's first suffix decision is at300002, so inventing one row at300000
would corrupt accounting. Five tests cover zero/multiple overlap, reset solver
counters, missing/extra/impossible row counts, invented/missing threshold rows,
malformed edges and wrong derived counters. All154 benchmark tests pass.
The actual prefix hash and any cached overlap bytes remain caller obligations;
the helper does not claim to validate interior rows from endpoints alone.

## Limits and continuation

No solver, rule, checker or observer implementation changed. Source6579 full
suites110/110/109 and24 fixed proof/model controls remain bound. Parser and
documentation edits receive focused tests and the normal full tool/doc checks.
Summary6608's first attempt stopped before output on an overly strict string
replacement-count assertion (the field occurs twice); the exact two occurrences
were corrected, without rerunning profile collection or successful annotation.

The memory peaks remain attributed to dense-list growth/compaction, not the
later collector. The separate private FlatLists revamp still requires explicit
approval; a nonblocking follow-up was sent this turn. No implementation or
approval is inferred from the active goal. Claude-owned source fuzz remains
unavailable: no access, launch or denied transfer retry, and no promotion.

Next: reuse the frozen6607/6613 localization; both diagnostics are terminal/clean
and must not be rerun. Compare the first179823 BVE invocation and resulting branch
state with existing native lifecycle authorities before adding an observer.
Revisit full6594 and all
Rank38 proof outcomes. Keep Rank4/32 closures and stronger Rank33 closure6519;
do not reopen completed fixed41–50, fresh6121/6227 or Rank50 localization.
The unchanged cpu-q5784 remains protected. Documentation stays within existing
caps; no automatic compaction. Andrew's new local PDF is generated through
generate-report; no report delivery or push is attempted. Goal active/unmet.
