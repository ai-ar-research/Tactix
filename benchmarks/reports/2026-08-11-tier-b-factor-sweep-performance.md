# Tier B Factor/Sweep representation performance closure

**Date:** 2026-08-11

**Decision:** ship. The repair closes all three remaining Tier B timeouts and
improves the full paired gate from 25/28 at 114.19 s PAR-2 to **28/28 at
28.69 s PAR-2**, with every model and proof independently checked.

## Causal localization

The selected divergence set was SAT `mod4block`, UNSAT `FmlaEquivChain`, and
SAT `PRP_30_38`. Two independent host-representation costs explained the gap.

`FmlaEquivChain` spent almost all of its time inside Factor. Tactix's pass-wide
remainder pool can contain hundreds of thousands of interned ids, but each
current quotient had only a few dozen distinct remainders. The greedy loop
nevertheless constructed `vector(pool.count())` buckets for every popped seed.
The repair uses a sparse rid map containing only the current quotient's
`distinct_rids`; remainder encounter order and selected grids are unchanged.

The `PRP_30_38` profile put 60.9% of samples in arena collection, rank sorting,
and watch selection. Sweep substituted every proved pair separately, so each
pair crossed Tactix's variable-width representation bridge and rebuilt the
complete formula. Consequences are now committed once at the environment
boundary: later queries in that environment use only the already-built
embedded formula, while the next main-formula seed sees the same ordered
consequences. Clause-position restoration is linear in the arena rather than a
stable comparison sort.

Pair batching exposed a second scheduling defect. Kissat's connected-clause
substitution is in-place and its Sweep allowance can price only Kitten ticks;
Tactix must additionally allocate replacements, collect the arena, restore
clause pose, and rebuild indices and watches. A productive Sweep batch now
charges its pre-rewrite arena words to `sweep_ticks_repr`, and the scheduler
spends `sweep_ticks_sub + sweep_ticks_repr`. Environment construction and
deduplication remain diagnostic-only. This representation-aware meter also
recovers `mod4block` without a fixture-specific rule switch.

## Focused closure

Release job `1609711`, under
`benchmarks/runs/20260811-tierb-repr-meter/`, used a 120 s limit and checked the
SAT models or UNSAT proof as applicable.

| Instance | Result | Time | Relevant evidence |
|---|---:|---:|---|
| `mod4block` | SAT | 25.53 s | model clean; Sweep 1.051 s, 13 rounds |
| `FmlaEquivChain` | UNSAT | 50.39 s | 65.6 MB DRAT clean; Factor 16.006 s |
| `PRP_30_38` | SAT | 47.34 s | model clean; Sweep 2.462 s, 150,209,014 representation words |

The sparse Factor arm alone changed `FmlaEquivChain` from a 120 s timeout to
proof-verified UNSAT at 65.30 s (`1609700`). The Sweep profile is `1609701`.
Pair batching without representation-aware scheduling still timed out after 17
Sweep rounds and 9,833 equivalences (`1609704`), establishing that batching and
pricing address distinct costs.

## Full Tier B gate

Run: `benchmarks/runs/20260811-105218-B-factor-sparse-sweep-repr/`

Array / aggregate jobs: `1609714` / `1609715`

Manifest: `benchmarks/tiers/B.list`, SHA-256
`5130866032ae706e48f17f0db431bb66e08536f69640b5a62cb90fd3e9c6bcef`

Shape: `cpu-dense-hwulab-q`, 1 CPU/task, 8 GiB/CPU, 300 s/instance

| Arm | Binary SHA-256 | Solved | SAT / UNSAT | PAR-2 |
|---|---|---:|---:|---:|
| Factor sparse + Sweep representation meter | `da51ecc1…f575c1` | **28/28** | 14 / 14 | **28.69 s** |
| previous `b20df47` | `1a0c6a4a…320a` | 25/28 | 12 / 13 | 114.19 s |

The candidate gains 85.50 s PAR-2 and 2,393.9 aggregate penalized seconds. Its
largest mover is `mod4block` at +570.6 s. The sign survives removal of any eight
instances. The three former timeouts now finish at 29.35 s, 53.14 s, and 56.85
s respectively. Other large recoveries include `square` (270.30→13.26 s),
`summle` (232.79→12.29 s), `b22_1` (115.97→18.36 s), `qwh` (82.06→9.66 s),
and `veer` (113.03→41.69 s).

Measured regressions remain visible: `201311` is 81.65 s versus 10.07 s,
`mp1-klieber` is 130.94 s versus 102.50 s, `oddball` is 37.42 s versus 27.80 s,
and `combined-crypto1` is 9.06 s versus 4.50 s. None loses a solve, and the
paired gate remains robust after removing the eight largest favorable movers.

Correctness is clean: 14/14 SAT models and 14/14 UNSAT DRAT certificates pass;
the latter total 666.2 MB and 451.1 s of checker time. There are no verdict
mismatches, invalid artifacts, checker give-ups, or unverified scored results.
The final benchmarked binary retains SHA-256 `da51ecc1…f575c1`; its complete
Release suite passes 69/69 under SLURM job `1609772`, including the focused
one-productive-environment representation-meter assertion. The documentation
size and whitespace gates pass as well.

Reopen the Factor repair on any selected-grid/order mismatch, proof failure, or
work again scaling with pass-wide `RemPool::count()` rather than the current
quotient. Reopen the Sweep repair on a seed-boundary formula mismatch,
proof/model failure, lost Tier B solve, or evidence that arena-word charging
systematically starves a productive resumed tail.
