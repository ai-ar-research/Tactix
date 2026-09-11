# Compact Vivify: completed short-gate assessment

The user explicitly chose to **retain and commit the measured improvement**
after reviewing the missed stretch target and Rank 35 memory increase. Use
frozen 7144 as the next investigation baseline; its implementation is already
committed in `cd03a9bc`. This is incremental retention, not completed
consolidation or final production promotion. Production `c2a95a6` is unchanged.
The frozen JSON below records the strict short-gate decision before this user
instruction; its failed gate values remain true and are not rewritten.

## All predeclared measurements

[Assessment JSON](2026-09-10-vivify-compact-assessment-7158.json) hash-binds the
[18-outcome runtime summary](../runs/20260910-7150-vivify-compact-runtime/solver-checkpoint-7155.json)
and [three-wave complete-function summary](../runs/20260910-7153-vivify-compact-samples/three-wave-summary-7156.json).
All three paired waves used immutable candidate/control binaries and the
campaign's fixed 12 logical CPUs, 48000 MiB, 1200-second solver budget,
separate checking/cleanup budget and full-width SLURM placement.

| Rank | Whole-runtime paired median | Full paired range | Peak-RSS paired median |
|---|---:|---:|---:|
| 31 | 0.989816788 | 0.988042138–0.995620582 | 0.979971477 |
| 35 | 0.993130629 | 0.990818002–0.999867061 | 1.024660357 |
| 38 | 0.999135475 | 0.993661284–1.004572163 | 0.833240869 |

Ratios are candidate/control; lower is better. All within-arm runtime ranges
are below 2%. Complete Rank 35 Vivify sampled-period ratios are
0.926830212, 0.927225193 and 0.927789328: median 0.927225193, a **7.27748%**
improvement. This includes every target-containing sample once, with all
descendants and startup/search ancestry retained; printed rule timers do not
replace this envelope. Sampling is not exact invocation timing.

The 15% complete-Vivify target fails. The 1% whole-runtime target passes on
Rank 31 but fails on Rank 35 (0.68694% gain). Median runtime nonregression
passes across all three witnesses, but RSS nonregression fails: Rank 35 is
2.46604% higher in every wave. The user accepts these local performance/memory
tradeoffs for retention, not a claim that the original gate passed. No
full-fifty, correctness, fuzz or overall memory-competitiveness gate is waived.

## Why the gain is smaller than the stretch target

[7161 explanation](2026-09-10-vivify-gain-explanation-7161.json) measures the
complete scheduling function and every descendant in all six saved streams,
with the existing six-test complete-function reader. The paired median
schedule ratio is **0.728968849**, a **27.1031%** improvement. Scheduling
originally occupies **27.6566% of Vivify**; Vivify itself occupies
**10.4682% of total sampled work** on Rank 35. Saving roughly 27% of roughly
28% of Vivify therefore produces the observed 7.28% complete-Vivify gain,
not 27% faster Vivify. The median per-wave Vivify saving is 10.262 billion
sampled periods; scheduling saves 10.580 billion, accounting for essentially
all of that improvement. These inclusive envelopes overlap, not add.

Holding other work constant, each wave's Vivify saving divided by baseline
total periods predicts a median **0.76417% whole-solver gain**, close to the
**0.68694% unprofiled runtime gain**. The observed whole-profile sampled gain
is 0.56673%; the remainder has small cost fluctuations despite exact work.
Cycle shares are an explanatory model, not a substitute for unprofiled timing.
This is an Amdahl-law limit, not evidence that the changed scheduling path
failed to improve. Reaching 15% whole-Vivify improvement from scheduling alone
would require removing roughly 54% of its original cost. Earlier localization
7140 explicitly cautioned that mandatory counting and ranking remained; the
15% threshold was a materiality target, not an established speedup forecast.

The main propagation leaf still accounts for about 62.4 billion sampled
periods within candidate wave-zero Vivify, while watch rebuilding, compaction
and watch selection account for another 25.4 billion disjoint self periods.
Those operations were not the target of this patch. Occurrence counting,
redundant sorting and root handling also remain necessary. The new radix path
still builds and moves rank records, rather than making ordering free.
Source shows two retained 24-byte-per-entry rank buffers plus histogram
scratch; these are plausible contributors to the Rank 35 RSS increase, but
no allocation-peak attribution proves that they explain the entire increase.

## Correctness and retained work

All 18 runtime and six profiled outcomes have exact expected results and
frozen-control non-timing work/proof fingerprints. Independent checker
completion is separate; the successor audit 7159 owns its timestamped
published-record snapshot. Pending proofs are unverified and cannot authorize
promotion. Independent work may continue, with any invalidity pausing its
dependents.

[Implementation evidence 7151](2026-09-10-vivify-compact-implementation-7151.md)
retains full suites 115/115, 115/115 and 114/114; six sanitizer fixtures twice;
96 independent DRAT/LRAT proofs and 192 negative controls. The new fixture has
**1728**, not 3456, transaction/model/proof cases; frozen build-manifest prose
contains that known miscount. Its exact parent digest is `11603ba2e1d600bb`.
Current-source Claude-owned fuzz remains unavailable/uninspected. No new
solver edit occurred during assessment; new offline caller attribution has
its own synthetic positive/negative controls.

The three sample extractions removed only their own verified raw expansions:
1,207,197,916 + 1,207,054,452 + 1,213,013,760 = **3,627,266,128 bytes**.
Compressed originals and extracted streams remain. No active checker residue
was removed. Two unchanged raw disassembly exports with tool-emitted trailing
whitespace remain local and hash-bound, not staged.

## Next decision: larger structural inprocessing costs

Rank 35 Eliminate still costs roughly 119.5 billion sampled periods; the
earlier rejected BVE import prototype did not close this whole-phase gap.
In the current first-wave profile, dense-occurrence rebuilding alone has
13.31 billion self periods. Forward partner discovery has 10.86 billion, but
native forward subsumption also has substantial cost: its presence is not
by itself evidence of inefficient implementation.

Source inspection of frozen 7030 establishes two maintained occurrence
images during elimination: `OccIndex::large` and `OccIndex::ordered`.
`rebuild_dense_large_occurrences` counts and fills both, including the
binary prefix of the mixed image. Collection rebuilds these images;
`strengthen_reconnect_and_flush` rebuilds them again at the post-forward
boundary. Native `eliminate.c` also requires post-forward reconnection and
end-of-round dense collection. Removing those boundaries is not a justified
optimization. The new [7160 caller localization](2026-09-10-dense-rebuild-localization-7160.json)
separates measured rebuild ancestry across all three frozen-control streams
before a representation/lifecycle redesign is selected. It does not measure
invocation counts or prove every reconstruction redundant.

Congruence and Factor remain other large leads: the assessment JSON records
coarse cross-window component differences of about 22.60 and 21.28 billion
sampled periods against frozen native 7082. Those are hypothesis-ranking
observations, not matched unit prices: gate work and component responsibilities
are not yet equal. Current code already has in-place clause shrinking and
compile-time probing specialization; neither is a missing mechanism.

The next action is direct Codex mechanism-localization and an evidence-backed
structural design targeting the largest removable whole-phase cost, with a
mini-design update, tests considered on every change, immutable controls and
unchanged acceptance gates. No routine approval pause, no new full-fifty run,
no resumed Rank 21 or rejected 7116 tuning. Retain user-selected 7144 as the
investigation baseline without treating it as a completed full gate. Preserve N1–N4 and closures
4/32/33/49/46. The fixed [7098 checked-fifty comparison](2026-09-10-collect-retry-checked50-7098/comparison.md)
remains 1.123176512× native, with 819.2859 seconds still needed to reach 1.01×;
memory, consolidation and current-source fuzz remain open.
