# Historical watermark: layout-only cost ablation

## Decision and isolated change

The source-backed historical Reduce watermark remains required. The preceding
[full50 follow-up](2026-09-09-watermark-full50-and-next-gaps.md) records an
adverse Rank38 cost: three exact-work pairs give median1.058875784x parent,
with negligible instruction-count change and6.06% more cycles in the first
checked pair. The compiled layout also shifted watched-search fields by eight
bytes. That combination motivates an ablation, not a claim of cause.

Isolated6763 moves the unchanged four-byte `first_reducible` member into the
existing padding after `large_watches_connected`. Its initializer and every
use remain identical to6730. No rule, guard, public instance, proof primitive,
collector operation, scheduling decision or benchmark-conditioned control
changes. Production remains unchanged; this source is experimental/unfuzzed.

The [6763 manifest](../runs/20260909-6763-watermark-layout-build/manifest.json)
owns the complete source and incremental/cumulative patches. Only
`src/kernel/kernel.hpp` differs from6730. Release and independent B/C binary
SHA is `ec37ee524ea287ef8b7e530dfdd5202514d416ddc407c336e83b2e92e4fccbdf`;
trace SHA is `735b7a716b1f5d7a2eacabe4ac0cce10fa306bdc76cf3c0424ed7f746dfcdebe`.

## Fixed and broad correctness evidence

Both Release suites pass112/112; trace passes111/111, including all288
historical-watermark states. The new fixed [6765 layout check](../runs/20260909-6765-watermark-layout-control/summary.json)
requires every previously measured6654 offset and total size to return, and
the new marker to fit entirely inside the old padding. Kernel size is7864,
watches352, flag408, marker412, next pointer416, delayed512, heuristics664 and
statistics1640. This proves the intended placement, not a runtime effect.

[6769](../runs/20260909-6769-watermark-layout-proof-control/summary.json)
rechecks eight independent DRAT/LRAT proofs and inherited root, nonroot,
pending-reason and model controls. All fixed input/proof bytes match6730.
No new semantic cases were needed for a member-only move; the existing288
cases cover its behavior and the new compiler-layout oracle covers the change.
The calculus and implemented guard contract are unchanged.

Full [B28/C5 gate6766](../runs/20260909-6766-watermark-layout-BC-gate/summary-6768.json),
job1678721 since22:23:55 EDT, has66 expected outcomes and all33 pairs exactly
match in non-timing statistics and certificates. Sealed6768 records64 checked
and two pending proofs. Candidate/current totals946.177/946.376s are neutral
in this single gate window, not a competitiveness claim. Checks continue
asynchronously; later published states belong to the successor audit.

## Targeted cost and retained repair

[6770](../runs/20260909-6770-watermark-layout-runtime/manifest.json) predeclares
three sequential four-task waves, each containing concurrent candidate/6730
pairs on Ranks38 and46. The middle wave reverses arm order. Both arms must
match the same6730 per-rank work and certificate. Resources remain12 logical
CPUs,48000MiB,1200 solver-only seconds, proof-on, without node constraints or
throttling. Wave0 is1678787 since22:32:29 EDT. First Rank46 times are
83.369/83.306s. Rank38 is362.370/362.427s: the first pair shows no layout gain.
Wave1 is1678793 since22:39:21 EDT. No result from one pair is acceptance.
Wave2 is1678797 since22:47:08. Sealed6772 hasall12 expected exact-work/proof
outcomes, eight checked at snapshot. Final Rank38 pair361.797/361.407s gives
paired median**0.999842727x**, range0.999217757--1.001079116. Rank46's final
83.357/83.151s gives median**1.000756248x**, range0.997674028--1.002477421.
Complete arm spreads are0.2413%/0.2819% on38 and0.1883%/0.3061% on46,
all below2%. This is neutral: the layout move does not recover the earlier
5.89% regression. Do not adopt6763 as a performance successor;6730 remains
the semantic baseline. No full50 array is justified for this rejected cost
hypothesis. The isolated source/pins and negative evidence remain available.

Do not infer data-layout causality from restored offsets or a changed binary
alone. Retain this negative result and investigate generated code/execution
efficiency without removing the historical
watermark semantics. Full50, memory and source-fuzz acceptance remain separate.

[6777 static code](../runs/20260909-6777-watermark-propagate-code/summary.json)
and its strict [6778 relocation accounting](../runs/20260909-6777-watermark-propagate-code/relocations-6778.json)
show that `kernel_propagate` remains4129 bytes and933 instruction rows.
Before the watermark its address is0x413790; both6730 and6763 use0x413730.
Every instruction-relative PC and nonrelocated operand in6763 matches6654.
The only differences are ten direct calls to the same symbols and two
read-only16-bit loads, whose relocated bytes independently match0100/0101.
Thus restoring member offsets restores this function's instructions, but not
its placement. This supports investigating code placement/execution efficiency
after the neutral timing result; it does not prove an instruction-cache or
branch-predictor cause or justify a benchmark-tuned alignment constant.

## Rank21 diagnostic failure and bounded successor

The preceding broad6757 search-window observer is invalid for causal use.
[Classification6773](../runs/20260909-6757-rank21-search-window/classification-6773.json)
records Tactix's explicit1200-second timeout, clean scratch and missing final
prefix. All2,272,347,245 emitted decision bytes match the corresponding6751
prefix; the last complete decision is atC49344/D74628533. Its partial proof
is not validated. SLURM reports its wrapper FAILED1:0, not a solver crash.

Kissat reaches50K with exact prior decisions/prefix/partial proof and six
permitted analysis/end boundary additions. However, its detail file exhausts
250000 records during preprocessing at conflict0: it has no rows in the
requested25098--25103 window. The strict parser rejects its truncation marker.
The old6759 validator and both unusable streams are preserved, not relaxed.

Existing `SEARCH_ONLY` admits Sweep records and macro snapshots. On this
multi-million-variable input, those bypass the intended narrow detail scope.
Unchanged-pin [6774](../runs/20260909-6774-rank21-light-window/manifest.json),
job1678791 since22:35:27 EDT, uses existing trajectory-only filtering plus
finite event-stop mode one conflict beyond the independent50K prefix stop.
This suppresses Sweep noise and intermediate ranking/macro snapshots while
retaining selected conflict/learn/decision records. Implication and ranking
snapshot coverage is explicitly not claimed. Both arms are needed because
native6757 had no usable selected detail; this is not a reference timing refresh.
Validator6776 still requires exact whole50K decisions and partial proofs.

That successor also failed its declared contract. [Classification6781](../runs/20260909-6774-rank21-light-window/classification-6781.json)
records missing prefix checkpoints in both arms: the event observer stopped at
analysis_end50001 before the independent prefix writer could flush its requested
50000 checkpoint. Source confirms both prefix writers require conflicts at least
requested+1. Tactix exited0 with its complete original decision stream; native
raised SIGTERM and lost the final buffered suffix. Both partial proofs exactly
match6751, and every available decision byte matches its prior prefix. Both
wrappers correctly failed1:0. This is an observer stop-ordering error, not a
solver crash or soundness defect. Neither detail stream is accepted causally;
strict6776 and the submitted sources remain unchanged.

Corrected [6782](../runs/20260909-6782-rank21-trajectory-stop/manifest.json),
job1678801 since23:05:44 EDT, keeps the same pins and solver limit. It removes
the unnecessary SEARCH_ONLY admission and uses UINT64_MAX-1 for the existing
finite-mode snapshot suppression, without a reachable second stop in this
bounded run. UINT64_MAX itself disables that mode. The independent50K prefix
alone owns termination. Strict6784 still requires whole prior decision streams,
prefixes, partial proofs, native old10K proof prefix, and uncapped selected detail.
It is pending, not accepted. Tool-owned watcher27055 follows that job.
No semantic unit-test addition is needed for this configuration-only correction:
existing strict prefix/window fixtures plus complete original replay are the
integration oracle; the separate truncation regression covers the uncovered cap.

A new maintained truncation-marker regression ensures a capped native file is
rejected even if the reader's numeric cap is larger. All219 benchmark-tool
tests and19 watcher tests pass. The classification helper's initial file-size
attribute typo was corrected before any summary existed; its later strict
parser failure exposed the separate native cap and is now explicitly classified.

No manual deletion, fuzz access, source promotion, FlatLists rewrite or denied
push/transfer retry occurs. Preserve all submitted sources, fixed references,
cpu-q campaign5784 and prior reports. The session cutoff remains September10
06:00EDT, with wind-down05:45 and no automatic job cancellation.
