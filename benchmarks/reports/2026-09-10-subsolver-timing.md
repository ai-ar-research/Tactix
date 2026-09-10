# Shrink-only runtime decision and binary-watch continuation

Candidate6857 is NOT adopted. Three predeclared waves preserve exact6730 work
and certificate bytes, but fail to show a useful overall speedup. The full50
authority stays6730 at8300.164/7239.010=1.146588277; production is unchanged.

## Completed measurement

[Strict6866 checkpoint](../runs/20260910-6864-subsolver-shrink-runtime/solver-checkpoint-6866.json)
contains all18 UNSAT solver results, no timeout or solve loss, and4 checked
certificates at that snapshot. [Cost6887](../runs/20260910-6864-subsolver-shrink-runtime/timer-cost-6887.json)
prices every pair; later proof outcomes belong to audit6891. No pending proof
is called accepted. Candidate/previous median ratios across three waves:

|Rank|Total runtime|Sweep|BVE|Peak RSS|
|---|---:|---:|---:|---:|
|32|1.001835393|1.004046597|0.994295882|1.000000000|
|39|1.008781507|1.007435161|0.994847042|1.000000000|
|38|0.999759284|1.026136364|1.000000000|1.000000000|

All individual arm spreads are below2%.39 is slower in all three total-runtime
pairs, with ratios1.007342480--1.014823788.32 and38 are mixed/near neutral.
The small BVE reduction on32/39 does not establish an overall gain.38's BVE
timer is tiny; integer-millisecond rounding matters. Generated-code size fell
2130 to1224bytes, but that premise did not translate into useful runtime.
Do not adopt6857, compose it into another experiment, or repeat to select noise.
No causal claim that one particular instruction or code-layout shift explains
the regression: unrelated phase timers also move. Same work is not the same
as identical physical placement or per-instruction execution cost.

The maintained cost aggregator now validates full task/pair cardinality,
declared ranks, both arms, rule application equality, positive finite runtime
and RSS, and requested timer presence. It retains all wave ratios and ranges.
An unpriced zero denominator does not silently select the remaining waves.
Eight new tests cover these cases, including reversed middle-wave order;
the five original timer-parser tests remain. All252 benchmark and22 SLURM
tests pass. The helper cross-checks every total-runtime ratio against6866.
Exact complete statistics and certificate checks remain upstream obligations,
not replaced by application-count agreement.

## Approved binary prototype is still experimental

6869 B/C retains all33 exact pairs; full own-source/control/proof evidence is
owned by [the previous report](2026-09-10-subsolver-cost.md). Its first timing
wave is adverse on32/39:330.114/328.094seconds and433.949/426.378seconds.
38 is360.629/360.590seconds.32 peak RSS is651592/587236KiB, a10.96% increase.
These are the complete first-wave observations, not a final three-wave result
or a reason to select/drop ranks. Complete remaining predeclared6882 waves via
6883, then run6884 and6887 binary once. Require full runtime/Sweep/BVE/RSS
tradeoff; no adoption, layout tuning, packing or scope expansion inferred.
Independent proof checking continues without blocking exact-work timing.

## Next cost localization, without a new solver run

[Annotation6888](../runs/20260910-6888-vivify-propagation-annotation/summary.json)
reads the existing checked6730 Rank32 profile:3020 samples,57032576476 periods
in private propagation under Vivify. The full function owns4.59% self samples
in the original profile. [Location6889](../runs/20260910-6888-vivify-propagation-annotation/location-6889.json)
binds reviewed assembly blocks and both reference implementations.
Watch decoding/blocker tests carry15.3141% of function periods (0.702617% of
program); large-clause address/garbage testing11.3155% (0.519161% of program);
the other-literal test4.86434% (0.223179%). Useful work and instruction skid are
included. These percentages are not removable-cost ceilings or evidence of
branch misprediction.

Tactix dispatches binary/large watches before its two satisfied-blocker tests;
Kissat consumes the optional tail then uses one blocker test before dispatch.
CaDiCaL has separate binary/large sweeps, which is not a behavior-neutral
replacement. Tactix already hoists values/arena, retains output-sensitive
delayed storage, and preserves native search/ignore points. Its bounded true-
level reason scan is stronger than the native full scan and should stay.
If the embedded prototype fails its complete metric, inspect the generated
code for a faithful same-layout dispatch shape before proposing a new runtime
candidate. Preserve scan order, reasons, root proofs, ignore location, ticks
and scheduling. Main search shares the template and needs its own exact guard.
No solver source edit, new native run or representation approval occurs here.

## Handoff and hygiene

Audit6891 binds the previous checkpoint, immutable campaigns, current published
states, live deadline monitor and aggregate checks. No successful writer was
rerun. Annotation6888 removed557169956bytes of helper-owned temporary profile
expansion; compressed original and raw hash-bound annotations remain intact.
No fixed proof cleanup is newly counted; previous helpers own those deletions.
Live checker scratch, failures, pins and reference authorities are retained.

Current-source fuzz, full50 convergence, memory and global scheduling/CDCL
parity remain open. Rank21 probes stay shelved; established4/32/33/49/46 and
N1--N4 protections remain. No FlatLists, promotion, denied transfer/push retry
or fuzz artifact access. Andrew's new report is local/unsent. PROGRESS owns
current job IDs and exact next action. Winddown05:45/no-new-task06:00EDT Sep10
and12CPU/48000MiB/1200solver-second contract remain unchanged.
