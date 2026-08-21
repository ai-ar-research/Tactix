# Tier-D rank 24: restore unconditional Vivify admission

## Decision

Accept the general Vivify-admission repair. Formula classification may still
dose BVE, Factor, forward work, and Vivify effort, but it no longer disables
the Vivify slot for any variable/density range. The explicit typed and
environment diagnostic controls remain available.

The accepted cumulative pin is `b6c991e0...fb6f`, built from source commit
`b7df39f8...b6985` plus source diff
`180bf8a8...9f1`. It closes frozen Tier-D ranks 21, 23, 24, 25, 28, 29, 34,
38, and 39 under the 16 GiB/1200 s contract. Eleven ranks remain open.

## Earliest portable divergence

The immutable rank-24 formula has 167,755 variables and 473,169 clauses. The
old classifier selected the broad 120k--200k `NoVivify` profile, and
`DefaultInprocessScheduler::build` consequently omitted Vivify from every
search-time Probe phase.

Kissat has no corresponding formula-shape admission policy. In `probe.c`, its
search-time pipeline calls `kissat_vivify` unconditionally between the early
Backbone and Sweep calls. The `vivify` option, the 100-per-mille effort limit,
the 3/3/1 redundant-tier shares, the irredundant share, and the irredundant
productivity delay govern work inside that opportunity. Kissat's reduced
startup pipeline intentionally omits Vivify. Thus the first portable
divergence is policy admission at the first due search-time Probe, not a
Vivify rule-side guard.

Diagnostic job `1636836` compared the frozen rank-24 input with instrumented
Kissat and the preceding Tactix source under symmetric trace controls. Both
tasks reached their 1200 s diagnostic timeout and left no certificate residue;
the trace establishes Kissat's search-time Vivify boundary, while the source
classifier establishes why Tactix could never take it. No frozen reference
solver result was rerun.

## Scheduling, candidate order, and rule interleaving audit

The repair is admission-only because the remaining Vivify machinery is already
source-aligned and regression-pinned:

- startup Probe: Congruence, Substitute, Backbone, Sweep, Substitute, Factor;
- search-time Probe: Congruence, Substitute, Backbone, Vivify, Sweep,
  Substitute, TransitiveReduce, Backbone, Factor;
- focused dynamic tier limits and 3/3/1 redundant shares before the
  irredundant share;
- occurrence-ranked candidate/literal order with Kissat's descending raw-
  literal tie, leftover priority/re-arm, committed clause pose, rebuilt large
  watches, and cross-candidate trail-prefix reuse;
- earliest implied-literal selection, deduction precedence, and the
  irredundant productivity delay.

`test_scheduling` now pins the exact enabled startup and search-time sequences.
`test_strategy_config` sweeps every formula-profile boundary and requires
Vivify admission while also proving that an explicit caller-selected disable
is preserved. The focused Vivify scheduling, tier, order, deduction, N1, and
proof tests remain unchanged and pass.

## Repair

`DefaultPolicy::classify_formula_strategy` collapses every `*NoVivify` profile
onto the same profile with only its independently supported BVE/Factor/dose
dimensions retained. It contains no benchmark name, hash, rank, deadline,
literal, conflict number, per-instance arm, or new tuned constant.

The stale Vivify comment was corrected to describe the already implemented
`better_implied` refinement; this is documentation-only and changes no rule
semantics.

## Validation

### Local correctness

Release configuration passes 72/72 tests. This includes native LRAT and DRAT
micro-proof replay, exact scheduling membership/order, formula-profile
coverage, Vivify semantic tests, and N1 tests. No fuzz or unit test exposed a
soundness defect or crash, so the plan's Claude repair delegation was not
triggered.

### Required iteration guard

Matched job `1636852` ran current shipped and candidate binaries in the same
window on all 28 Tier-B fixtures and the predeclared fixed five
mechanism-relevant Tier-C fixtures:

| tier | shipped | candidate | shipped wall | candidate wall |
|---|---:|---:|---:|---:|
| B | 28/28 checked | 28/28 checked | 1228.741 s | 649.578 s |
| C | 5/5 checked | 5/5 checked | 1790.271 s | 1586.259 s |
| total | 33/33 checked | 33/33 checked | 3019.012 s | 2235.837 s |

There is no solve loss. The largest gain is `mod4block`, 718.396 s to
95.820 s. The largest slowdown is `SC25_Timetable`, 18.908 s to 67.615 s;
it remains a checked SAT solve. The hard `T105.2.0` UNSAT guard is neutral at
807.998 s versus 811.605 s. All SAT models and UNSAT proofs verify, and all 66
tasks report deleted scratch/certificate artifacts.

Manifest:
`benchmarks/runs/20260820-2306-BC-vivify-admission/manifest.json`
(`f230612e...6216`).

### Targeted rank 24

Job `1636927` proves the immutable rank-24 instance UNSAT in 356.205 s.
Vivify runs 208 times, checks 702,223 candidates, and performs 177,032
vivifications. `drat-trim` verifies the 1,398,139,701-byte proof in 94.099 s
with zero RAT lemmas in the core. The proof is deleted after checking.

Manifest:
`benchmarks/runs/20260820-2322-tierd-rank24-vivify-admission/manifest.json`
(`a7bc9530...e6be`).

### Cumulative ranks 21--40

Job `1636928` returns nine independently checked answers: ranks 21, 23, 24,
25, 28, 29, 34, 38, and 39. It preserves all six inherited closures, adds
rank 24 UNSAT, rank 34 SAT, and rank 39 UNSAT, and has eleven clean timeouts.
All 20 tasks finish without infrastructure failure or cleanup residue.

Manifest:
`benchmarks/runs/20260820-2331-tierd-ranks21-40-vivify-admission/manifest.json`
(`65e2c85f...550`).

## Reopening condition

Reopen if any formula classification can again clear `vivify_enabled`, if the
enabled startup/search Probe sequences differ from the source-pinned orders,
if a Vivify order/deduction regression violates its existing ledger reopening
condition, if a checked predecessor is lost, or if proof/model validation or
certificate cleanup fails.
