# Complete BVE pilot: stop, preserve evidence, reassess the larger Probe cost

## Decision

The original complete prototype misses its acceptance premise. Do not promote
7116, lower the 20% complete-Eliminate target, tune constants or code placement,
or run another full50 view to select noise. Retain all frozen sources, pins,
tests and measurements as unpromoted investigation material. Production
c2a95a6 is unchanged. This is a completed negative performance decision, not a
solver correctness failure or completion of the convergence goal.

[Assessment7135](2026-09-10-bve-prototype-assessment-7135.json) binds the complete
7127 unprofiled and 7130/7133 sampled evidence, the checked7098 full50 records,
and the reviewed current/native sources. Successor audit7136 owns later
published proof/cleanup and scheduler states without rewriting old snapshots.

## All three predeclared pairs, without exclusions

|Rank|Whole-solver candidate/control median [full range]|Peak RSS ratio, all three pairs|Printed BVE median|
|---|---:|---:|---:|
|31|0.997722246 [0.993637246, 1.004337686]|1.011158649|0.880595752|
|35|0.994889567 [0.992723157, 0.999466252]|1.001603537|0.880563495|
|38|0.991836007 [0.990918513, 0.992092652]|0.951502543|0.948717949|

Whole-runtime paired gains are only 0.23%, 0.51%, and 0.82%. All six
within-arm runtime ranges are below 2%; no observation is excluded. Rank31
has one slower candidate repetition. RSS regresses by 1.116% on31 and 0.160%
on35, consistently across every wave. Lower allocation counts or removed
vectors do not establish lower whole-process memory. The physical mapped
import reserve identified in7124 is one possible explanation, not a proved
cause of the RSS increases. Do not turn that small accounting lead into a new
tuning campaign after the complete pilot has failed its materiality premise.

[7133 lifecycle summary](../runs/20260910-7132-bve-lifecycle-samples/three-wave-summary-7133.json)
contains all three complete Rank35 pairs:

|Complete scope|Paired sampled-cycle median [full range]|
|---|---:|
|Initial Probe|0.998037656 [0.990366809, 0.998050499]|
|Search Probe|1.001390970 [1.000045971, 1.002881175]|
|Eliminate|0.924798561 [0.920828980, 0.925152694]|
|No observed phase frame|1.001305620 [0.997432668, 1.003370043]|

Complete Eliminate improves 7.520%, not the approved 20%. Its candidate/control
median sampled periods are109.916/118.853 billion. All18 runtime outcomes have
exact repeated and7030 cross-arm work/certificate fingerprints. All6 profiles
match their exact unprofiled work/certificates, have zero lost samples and
conserved phase attribution. Sampling is not exact per-operation latency.
The narrower printed BVE reduction of approximately12% on31/35 is not a
replacement for the complete-Eliminate gate. Rank38 has negligible BVE time;
do not attribute its whole-runtime difference to faster elimination.

7122 B28+C5 now has66/66 final checked-clean outcomes and33 exact7116/7087
work/proof pairs, confirmed by the read-only7123 validator. At their frozen
solver summaries,7127 had4/18 checked and7130 had0/6. Subsequent checks continue
asynchronously. Exact proof bytes permit provisional development, not a final
checked gate; any reported invalidity interrupts dependent work.

## Larger remaining opportunity, with scope caveats

The median7116 search-Probe envelope is357.125 billion sampled cycles versus
245.489 in the frozen native7082 diagnostic, a111.636-billion difference.
Remaining Eliminate difference is54.273 billion. This points toward Probe as
the larger inprocessing investigation, not more incremental BVE preparation.
The native observation is cross-window, not a fresh timing denominator.
No-phase ancestry must not be labeled CDCL, and wrapper/component differences
must not be treated as identical operations or summed as independently
removable overhead. Major7059 work correspondences strengthen Rank35 as a cost
control but do not prove full trajectory or every scheduling/budget parity.

Re-reading every checked7030 result, rather than relying on an older candidate,
ranks named-rule time over the complete approved50: Vivify621.193s,
Congruence423.926s, Sweep416.374s, BVE300.365s, Substitute264.682s,
Factor216.708s. These are current workload envelopes, not native excess cost
or predicted rewrite savings. Rank31 remains the largest individual gap;
its excess search work cannot be explained away by a Rank35 cost profile.

Vivify scheduling plus all candidate-introsort self symbols costs
33.886/33.957/33.966 billion sampled cycles across the three Rank35 profiles.
These are disjoint self samples, not added inclusive costs. The whole Vivify
Probe component is approximately140.751 billion candidate versus115.690
native. Some scheduling cost is indispensable. The component difference is
not a proven removable amount because entry responsibilities differ.

Reviewed `vivify.cpp:286,440-794` stores40-byte candidate records and copies
every scheduled clause's literals to an additional retained-capacity pool.
It sorts those copies, commits redundant clause poses back to the arena, sorts
the large candidate records, then emits references. Kissat
`vivify.c:307-465` sorts redundant clauses in place and sorts a reference stack;
its irredundant path uses compact count records and radix ranking. CaDiCaL
`vivify.cpp:1510-1578` also uses compact occurrence-ranking records and stable
leftover partitioning. Reference semantics are not interchangeable: preserve
Tactix's established Kissat ordering and logical arena-position repairs.
The prior first-count cache and persistent scratch already exist; repeating
those small edits is not the proposed work.

## Proposed next structural pilot — not yet approved or implemented

- Problem: material full50 Vivify workload and unnecessary canonical schedule
  representation traffic, with repeated Rank35 source-localized evidence.
- Structure: specialize only the mutating canonical homogeneous schedules.
  Preserve pass-time membership while collecting compact references; after
  root-rewrite passes finish, sort redundant literals directly in their final
  arena records and compare those records. For pure irredundant schedules use
  compact maximum-count/size/priority/logical-position keys without literal
  snapshots. Preserve the generic const query, mixed-scope, non-reuse and
  prefix-flush paths as required fallbacks, not removed N1 capabilities.
- Invariants: unchanged root-simplification/count timing, glue-band membership,
  priority rearming, budget cutoffs, logical-position ties, candidate order,
  clause pose, watch rebuilding, probing transaction order, ticks, proof bytes
  and model reconstruction. No new rule, policy threshold or kernel admission
  rule. References are reacquired after mutations; never retain raw arena
  pointers through rewriting, growth or collection.
- Migration: fork the frozen7030 control, not the rejected combined7116 pilot;
  first prove exact schedule/transaction parity on fixed fixtures, then run
  B28+C5 and three paired31/35/38 waves with unchanged allocation/checkers.
  Measure complete Vivify envelopes with matching profiles. Cut over only if
  the gates support the representation; keep a clean rollback to7030.
- Proposed materiality gate: at least15% lower complete-Vivify Rank35 sampled
  cycles and at least1% lower whole-runtime paired median on31 and35, with no
  whole-runtime or RSS regression on31/35/38. These are a new pilot's proposed
  thresholds, not replacements for the failed BVE contract. No forecast of
  meeting them is asserted. If the short premise fails, stop the pilot.
- Tests considered before edits: extend actual scheduling/transaction fixtures
  for rewritten cross-band clauses, equal count vectors and final-literal
  ties, retained logical positions, priority cuts/rearms, large-irredundant
  skip-sort, repeated scratch across kernels, const-query purity and every
  fallback scope. Require baseline-exact state/proof digests, independent
  model enumeration, proof negatives, sanitizers and complete suites. These
  are new tests to add with the rewrite, not claimed completed work.
- Risk: directly sorting live clauses at the wrong point changes propagation
  antecedents even if candidate order agrees. Indirect reference comparisons
  may trade copying for cache misses; retained scratch can negate memory
  savings. Full50 transfer and current-source Claude-owned fuzz remain required
  before promotion. No Claude debugging or review delegation.

Return this evidence and proposal to the user under `revamp` before implementing
a different structural scope. Read-only investigation and existing proof
follow-up may continue; do not silently broaden the failed prototype.

## Hygiene and validation

This assessment changes offline evidence and the current handoff only, not
solver sources or maintained measurement logic. Existing15 callstack tests
cover the conserving reader; existing timer/pair tests cover full matrices,
bad values and denominator handling. The assessment asserts exact input
hashes, all50 unique checked ranks, all three sampled waves, conserving
component/self partitions and the original failed thresholds. No additional
solver unit test is needed without a solver change.

Only verified new wave1/wave2 offline raw-profile expansions were removed in
the carried-through work:1,198,324,188+1,201,269,852=2,399,594,040 bytes.
All three waves total3,613,423,340 bytes including the previously reported
wave0. Compressed original profiles and compressed extracted streams remain;
no active checker scratch, old campaign or Claude-owned fuzz artifact was
inspected or modified. No new solver jobs or native baseline rerun this turn.
The original full50 score remains1.123176512x, with819.2859s still needed.
Current-source fuzz is unavailable, not waived. No denied transfer/push retry.
Andrew's new report is local/unsent in
`report-to-andrew/20260910-1537-bve-prototype-assessment/`.
