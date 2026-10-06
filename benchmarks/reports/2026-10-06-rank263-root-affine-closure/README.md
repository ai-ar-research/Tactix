# Rank 263 root affine closure

This report isolates why Tactix's current authenticated GF(2) matrix does not
refute Rank 263 at the root.  It is a diagnosis of retained job `1750986`, not
a recovery claim and not a minimum-support proof.

The input has 2,475 variables and 8,172 clauses.  Every clause belongs to an
affine equation: 7,596 clauses are the exact CNF encodings of 1,899 ternary XOR
equations, and 576 clauses are unit equations.  Ordinary Boolean unit
propagation fixes those 576 variables without finding a conflict.

The retained matrix session admitted all 1,899 sources into one 1,899-row
basis.  Reconditioning that exact basis under the root assignments and reducing
it in its recorded order produces a contradiction with 670 source equations.
The largest intermediate row has width 45, while the largest intermediate
source set has 1,041 equations.

The current service profile allows width 68 but only 160 sources.  Mirroring
the implementation's immediate cap check discards 24 intermediate rows and
misses the root contradiction.  The first five discarded rows have source
counts 168, 164, 172, 161, and 161.  Thus the measured obstruction is the
proof-source bound rather than affine recognition, connectivity, or row width.

This result does not establish that 670 sources are necessary.  It is the
support produced by the deterministic basis and elimination order captured in
the retained provenance.

## Bounded causal run

CPU-Q array `1751024` first raised the source allowance from 160 to 2,048 while
retaining the 10-million synchronization-work limit.  Both the DSR and
no-proof arms stopped cleanly after consuming 9,998,637 work units; neither
entered CDCL or emitted a proof.  This exposed the next independent guard.

CPU-Q array `1751026` retained the 2,048-source allowance and raised the
explicit synchronization-work allowance to 100 million.  Its DSR and no-proof
arms both returned UNSAT at root in 0.75 and 0.74 seconds respectively, with
zero decisions, one conflict and 577 propagations.  The matrix used 11,137,510
synchronization-work units.  Its exact conflict contains 670 authenticated
source equations and a 200-literal Boolean reason.  Planning produced 243,947
proof additions, 243,149 deletions and 13,997 helper variables.  The DSR is
4,991,343 bytes; structural provenance is 43,203,865 bytes.

CPU-Q job `1751028` checked that exact DSR through the repository's witnessed
DSR/Trestle pipeline with the every-lemma audit.  It returned
`s VERIFIED UNSAT` in 45.80 seconds with 131,660 KiB peak RSS, within the
30-minute proof-checking limit.  Thus this profile is a checked Rank-263
recovery using the existing certified matrix, and it is faster than the
7.07-second no-proof CryptoMiniSat reference run.  It is not yet a general
default-scheduling result: the limits were selected from the exact causal
diagnosis, and other affine components still need derived admission and
usefulness criteria.

The longer-term design should compact or lazily materialize large derivations
and choose limits from observable component/proof budgets rather than adopt an
unexplained global constant.  The retained-row implementation in
`9ea8261c4` is sound infrastructure, but job `1751022` showed that partial
conditioning under the old 160-source cap forced all 677 later services to
rebuild.  High-support root closure therefore precedes watched rows, local
pivot repair and usefulness scheduling in the measured implementation order.

## Reproduction

From the repository root:

```sh
python3 benchmarks/reports/2026-10-06-rank263-root-affine-closure/analyze.py \
  benchmarks/cache/tierD24-26/32e344a5ab3af245f982678d2b751c6d-lightsout_sat_24_unbounded_totalizer_corner_0_unsat.sanitized.cnf.xz \
  build/pin-r263-proof-cache-target30-extent1m-575760399-v1/result-1750986/provenance.jsonl \
  --output benchmarks/reports/2026-10-06-rank263-root-affine-closure/result.json
```

The input compressed SHA-256 is
`64a2eba5d44ff850789de5fa7ff854c3ff90c428c5c25a5ece08f91bd6057ac4`;
the decompressed SHA-256 is
`fec9c446068a306641493d88daa4bf6b1cbb1adc0252d2712902c308824a570b`;
the retained provenance SHA-256 is
`0fb2dbfba2783b64d58284c3fef9e97ef6b0f494092fddc8ace4df9a80b5fe`.
The successful profile, proof, provenance and checker-output SHA-256 values are
recorded in `experiment.json`.
