# Watermark cost profile and Rank21 trajectory follow-up

## Decision and authority

The semantic baseline remains isolated6730; productionc2a95a6 is unchanged.
The full Rank1--50 score stays8300.164/7239.010=1.146588277x against the fixed
Kissat vector. Fresh reevaluation6121/6227 and added-ten6108 are complete
authorities, not requests to rerun references. The runtime/memory goal is unmet.

The preceding [layout ablation](2026-09-09-watermark-layout-ablation.md) is
neutral and not adopted. Three matched pairs retain median1.058875784x Rank38
current/6654 cost, with essentially equal retired instructions and6.06% more
cycles in the first checked pair. Restoring measured member offsets in6763
does not recover that cost. Static6777/6778 accounts every main-propagation
instruction difference after relocation, but code placement is not a cause
proved by those measurements.

## Matched Rank38 sampled diagnostic

[6787 manifest](../runs/20260909-6787-rank38-watermark-profile/manifest.json)
predeclares one two-task diagnostic using the unchanged6730 and6654 Release
pins. Job1678803 was submitted23:16:02 EDT; watcher41747 follows it. Each task
uses12 logical CPUs,48000MiB,1200 solver-only seconds, a separate16000-second
checker allowance and5h allocation. There is no node constraint or throttle.
Proofs stay enabled and checking proceeds asynchronously.

The existing hash-verified5544 sampling wrapper is copied into this campaign;
only its two solver names change from candidate/kissat to candidate/previous.
It samples cycles:u at199Hz with the existing DWARF call-stack protocol, emits
self/callpath reports, and retains hash-bound compressed sample data. No solver,
checker, observer, rule, policy, or sampling algorithm changes.

Validator6789 requires both expected UNSAT outcomes, exact within-arm complete
non-timing statistics and counters, identical certificate bytes/hash against
the checked6754 first pair, valid report hashes, and zero lost samples. Pending
certificate checks permit investigation, not final acceptance. Sample shares
localize a possible execution-cost owner; they are not operation prices,
proof of a cache/front-end cause, or repeated timing acceptance.

No new unit-test cases are needed for the copied sampling protocol and arm-name
binding: preparation verifies immutable source/runner hashes and exact two-arm
matrix identities, and full work/proof replay is the integration oracle.
All219 benchmark-tool and19 watcher tests pass; all three new helpers compile.

[Sealed6789](../runs/20260909-6787-rank38-watermark-profile/solver-summary-6789.json)
passes both exact full-statistics/certificate replays with zero lost samples.
Both proofs are pending at its snapshot. Diagnostic solver times are362.912
and341.963s; these are not replacement unprofiled acceptance measurements.
Main `kernel_propagate` self shares are68.00% versus65.87%.

[Offline annotation6790](../runs/20260909-6790-watermark-propagation-annotation/summary.json)
checks all933 instruction offsets against independent6777 objdump inventories.
Its exact sampled periods for that symbol are928443010744/848015261323,
ratio1.094842337. The largest positive difference is at relative offset840,
`test $0x1,%al`:192180964993/105748951905 periods, delta86432013088.
This is the watch-loop binary/large branch immediately after the watched
literal's value load. Sampling/skid means this is a region to investigate,
not86billion cycles intrinsically spent on a register test. Code placement,
branch behavior and data access remain hypotheses; do not tune an alignment
constant to this benchmark. Seven existing annotation-parser tests cover the
reader and every actual offset must match the separately disassembled function.
The helper removed only its own two temporary perf expansions,1183320440 bytes;
original compressed profiles and raw annotations remain local and hash-bound.

## Rank21 and memory

Corrected [6782](../runs/20260909-6782-rank21-trajectory-stop/manifest.json),
job1678801 since23:05:44, now passes [strict6784](../runs/20260909-6782-rank21-trajectory-stop/summary-6784.json).
Both exit0, clean scratch and exactly replay whole6751 decisions/prefixes/partial
proofs, including the native old10K proof prefix and bounded observer additions.
There are341 current and252 native uncapped detail rows. Failed6757 and6774
remain classified and unchanged; neither is causal evidence.

[Comparison6793](../runs/20260909-6782-rank21-trajectory-stop/comparison-6793.json)
uses the existing physical-clause metadata normalization: all five local
pre-split conflict/analysis transactions25098..25102 match, and all four
learned-stage size/order/set digests match across all six transactions.
The first selected decision difference is25102/D25451310/L14, focused,
current-402373/native-402380. This rules out a newly different learned-stage
digest in this local window, not an earlier latent queue or implication state.

Same-pin [6794 selected state](../runs/20260909-6794-rank21-choice-state/manifest.json),
job1678806 since23:29:23 EDT, watcher97472, observes external402380 at25102.
It uses the original6751 runner plus only existing choice-state controls, with
no global event trace or second endpoint. Compare assignment/reason and focused
queue/cursor columns around precedingD25451309 and splitD25451310, after
strict6796 verifies every original50K trace/proof and every selected state row
against its decision record. This distinguishes an implied/skipped variable
from an already-displaced queue entry; no cause is preselected.
New maintained `choice_state.py` has seven tests for assignment/unassigned
contracts, framing, numeric fields/selection, reasons, ordering and arguments.
All226 benchmark-tool tests now pass; solver binaries remain unchanged.

The existing [BVE memory ownership](2026-09-09-bve-memory-ownership.md) evidence
locates dense-vector growth/copying-compaction intervals before collection's
entry peak. The proposed private FlatLists redesign remains unapproved and is
not implemented. This prior ownership evidence is not a new current-source
memory measurement or a claim that the runtime investigation closes memory.

## Hygiene and continuation

The previous checkpoint2cd1b76d is clean. Its first landing attempt6780 stopped
before commit because three objdump outputs contained emitted trailing spaces.
Successor6785/6786 preserved those bytes locally and hash-bound, excluded only
those three raw files from staging, and retained the whitespace gate. No
successful audit, submitted source or frozen report was rewritten.

Checks remain asynchronous and published-results-only. No manual deletion,
source promotion, fuzz-artifact access, denied transfer or push retry occurs.
Preserve Rank4/32, strongerRank33, Rank49, compact stamps/activation, historical
Reduce semantics, unit-only lazy cleanup and the rule/strategy boundary.
Wind down September10 at05:45EDT; start no new tasks at06:00EDT, with no
automatic job cancellation. Andrew-facing TeX/PDF stays local and unsent.
