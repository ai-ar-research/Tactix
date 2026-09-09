# Current Substitute collapse: full Rank 1–50 checkpoint

Source authority: [6305](../runs/20260909-6305-substitute-collapse-build/manifest.json).
Full solver outcomes: [6329](2026-09-09-collapse-complete-checkpoint-6329.json).
Per-rank measurements and parent-work comparison: [6334 TSV](2026-09-09-collapse-full50-6334/comparison.tsv),
[hash-bound summary](2026-09-09-collapse-full50-6334/summary.json).

All 50 expected outcomes (25 SAT, 25 UNSAT), no solve loss or timeout;
36 independently checked and clean at the scoring checkpoint. Pending checks
remain provisional. Tactix totals 8598.949 solver seconds against the unchanged
7239.010-second Kissat vector: **1.187862567×**, not the ≤1.01× target.
The original40 totals 7397.795/6810.066 = 1.086302981×; added10 totals
1201.154/428.944 = 2.800258309×. Added10 contributes 772.210 of the net
1359.939-second gap. The fixed vector is cross-window: these are comparison
scores, not isolated causal timing estimates.

## Changed work and limitations

Relative to6269/full6278, certificate hashes change only at ranks
24/27/31/32/33/37/39/40. The other42, including all added10, preserve proof
hashes. Rank32 conflicts drop11219629→5411982; Rank33 13549300→8792234.
Rank40 worsens3319566→4782925. These are actual pinned work observations,
not a claim that matching more of Kissat's trace necessarily improves every
full run. Earlier6169 remains a better full50 cross-window score (1.160931951×);
no current candidate is promoted.

Rank50 remains426.847 versus49.047 seconds and Rank49 159.168 versus22.555.
Their work/proof does not change in6305. Rank21 remains a separate memory
priority:3034308 versus1436392KiB peak solver RSS. Runtime improvement does
not discharge this ownership gap or authorize the proposed private allocator
redesign. The separate Rank50 subsumption-material tracker still needs approval.

## Repetition and diagnosis

[6332](../runs/20260909-6332-collapse-three-wave-runtime/manifest.json)
predeclares three six-task paired waves comparing6305 against6269 at
Ranks32/33/40 (two productive cases and an adverse control). Wave0 is
job1677553 since2026-09-09T04:49:13. Advance via6333 only after the prior
wave's six expected solver outcomes, not after proof completion. Each arm must
replay its own source-bound full-run statistics and certificate, then receive
an independent check and cleanup. Report three paired ratios per rank,
median/range and per-arm spread; ≤2% spread is required for an absolute timing
claim. Reverse arm order in the middle wave, no node constraint or throttle.

[6327 diagnostic build](../runs/20260909-6327-collapse-schedule-build/summary.json)
ports the previously tested6206 schedule-snapshot continuation into actual6305.
Only diagnostic source and its focused test change:105Release/104trace tests
pass, ordinary Release remains byte-identical33a94aec8df9a3be4f2a902e13d59dcc1527a412302f646140bab2ec5c4193e4.
Trace6d7f0c0d564823108da81ee63beeab5ff86b315dfc9043b43d66037dba30e33b
is not yet real-instance observer-neutrality evidence.

[Current6305 Rank32 replay6321](../runs/20260909-6321-collapse-rank32-prefix/summary.json)
is now COMPLETED and validated by6323: inherited96083379-row hash, one threshold
overlap and complete trace framing pass. The first108987868 decisions match;
the next differs at conflict1405411/decision108780509/level2872:
T33105 versusK103431. The previous1368117 split closes. At1366932 both BVE
ends now agree on portable active/irredundant/binary counts (first65019/422193/96295,
second64863/418775/96077); this does not prove whole-formula identity.

At the following1368343 phase, both enter TransitiveReduce with95938 binaries;
T ends95937 and K95936. This is the next diagnostic target, not a proven cause
of the later selected-decision split. The earlier19375 Sweep count discrepancy
also remains in the complete trace. No native macro file exists in the cached
6303 result: only its actual recorded streams are validated. Both partial
certificates were hash-bound and deleted, not accepted as final proofs.
The obsolete unexecuted6330 BVE preparer was removed before landing; no job or
campaign directory was created for it. Any next changed-observer run must
replay ordinary streams and the per-arm partial proof before interpretation.

No solver policy/rule semantics changed in this increment; diagnostic tests
cover exit/continue, conflict/ordinal filters, no overwrite and state
preservation. Harness tests39+13 pass. Source fuzz remains Claude-owned and
unverified here; no fuzz jobs/artifacts were inspected, no denied transfer or
push was retried, and no live checker scratch was manually deleted. Existing
campaigns, baseline manifests and pins remain frozen. Final checks, current-source
fuzz, repeated timing and memory convergence remain open.
