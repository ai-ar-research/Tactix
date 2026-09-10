# BVE data-path stage2: mapped import and consumer-local indexes

The approved prototype is now implemented in isolated7110, with boundary/test
successor7116. Production remains c2a95a6. This is unfuzzed investigation
material, not a promoted repair or a measured performance win. The fixed50
checked control remains8130.686/7239.010=1.123176512x Kissat; no reference rerun.

## Structure and preserved contracts

[7110 build](../runs/20260910-7110-bve-import-build/manifest.json) preserves the
7087 antecedent views and changes the following preparation path:

- SubSolver accepts a synchronous read-only physical clause and outer map,
  omits the pivot, and maps directly into its existing owned import scratch.
  The generic vector entry uses the same compile-time-specialized body. Sweep
  keeps that generic entry; there is no runtime mode branch in its import loop.
- The first physical scan assigns final compact ids immediately. The existing
  reverse map doubles as the touched list, removing a duplicate vector and a
  later map-fill pass. All variables are still counted before resetting Kitten;
  lazy variable creation would change queue/generator/capacity behavior.
- The caller's intermediate mapped clause vector and its extra write/read
  traversal are gone. Physical root-false literals remain definition variables;
  root-filtered resolution views must not replace this physical input.
- Signature indexes are constructed only when the XOR consumer is reached.
  ITE scans clauses independently. No gate kind is disabled, candidate order
  changed, budget retuned, or public rule instance removed.

Clause ids, tautology short-circuiting, deduplication, ticks, clause/core order,
generator lifetime, proof lifting and reconstruction remain the cost-only
premise. Input views cannot survive import; SubSolver retains owned copies.
The engine method is not a new kernel primitive, proof vocabulary or public
SAT C API. E1's abstract calculus is unchanged. The implementation notes own
the representation/lifetime invariant. Kissat definition import and CaDiCaL
XOR occurrence traversal were reviewed; no reference code was copied verbatim.

Mapped scratch reserves the physical input length rather than the old
pivot-stripped length; it can retain an extra literal of capacity. Eliminating
other vectors does not itself prove RSS improvement: measure the whole process.

## Validation and compiler diagnostic classification

Both [7110](../runs/20260910-7110-bve-import-build/manifest.json) and
[7116](../runs/20260910-7116-bve-import-bounds-build/manifest.json) passed
114 Release,114 independent B/C and113 trace tests. Each Release/B/C pair has
identical binaries. Candidate7116 Release/BC SHA256 is
`76ce318a94102e0345c2387b1b061966f35474295d3617ec0dc653e0743feff9`.

- [7111 focused gate](../runs/20260910-7110-bve-import-build/focused/summary.json):
  full SubSolver suite under ASan/UBSan/LSan, including252 new fixed mapped/copy
  cases. Independent brute-force models and LRAT replay remain active. Compare
  ids, ticks, generator, phase capacity, conflicts, original count, models,
  derivation/core lineage and shrink/shuffle/re-solve behavior. Overwrite caller
  input/maps after import to test ownership. This engine source and fixture are
  byte-identical in7116, explicitly verified by7117.
- [7113 definition proofs](../runs/20260910-7113-bve-import-proofs/summary.json)
  and [7119 successor](../runs/20260910-7119-bve-import-bounds-proofs/summary.json):
  each96 verified proofs,98 rejected negative controls, exact7087 proof bytes
  and definition ticks. One-sided cores require learned-clause lifting;
  root-false physical padding, duplicates, reversed order and dense/sparse
  N1 entry are exercised. Inputs have no initial UP refutation.
- [7115 Sweep proofs](../runs/20260910-7115-bve-import-sweep-proofs/summary.json)
  and [7120 successor](../runs/20260910-7120-bve-import-bounds-sweep-proofs/summary.json):
  each120 verified proofs and122 rejected negatives, exact7087 outputs/proofs.
  These exercise the shared engine's unchanged generic import consumer.
- [7114](../runs/20260910-7114-bve-import-sanitizers/summary.json) and
  [7118](../runs/20260910-7118-bve-import-bounds-sanitizers/summary.json) each pass
  fully instrumented Debug BVE, antecedent-view and BVE-memory tests outside
  ptrace, with normal leak detection. No solver, sanitizer or proof failure.
- [7121 boundary controls](../runs/20260910-7121-bve-import-bounds-fixtures/summary.json)
  run identical complete BVE/view fixtures against7110 and7116. New496 cases
  span XOR arities2..6, dense/sparse entry, reversed literal order and every
  input assignment. Arity5 remains recognized, arity6 is safely unrecognized,
  elimination/reconstruction and all original clauses validate in both arms.

GCC13 emitted `-Warray-bounds` inside XOR probe `std::sort` after index
construction moved into its consumer. The builder already caps signatures at5;
7116 explicitly re-establishes `2 <= m <= BVE_SIG_MAX` at the consumer. The
warning persists, even with that bound. Source inspection plus boundary and
sanitizer evidence supports an optimizer diagnostic limitation, not a
demonstrated out-of-bounds execution; this is not a blanket proof of safety.
Do not suppress warnings or tune code layout to chase this diagnostic.
Both warning-bearing builds and their logs remain frozen. A separate ambiguous
ReservedTrail equality in the repaired view test is removed by an explicit
vector snapshot; the older unrelated Reduce-test warning remains untouched.

Every implementation change considered new tests: mapped ownership/parity
gets252 cases; the explicit XOR bound gets496 boundary/model cases; the vector
snapshot uses the existing full reconstruction fixture. Strong proof controls
cover emitted evidence beyond the direct engine tests. None substitutes for
current-source fuzz or final cumulative gates.

## Asynchronous campaigns and next action

[7122 B28+C5](../runs/20260910-7122-bve-import-BC-gate/manifest.json) is job1679933,
submitted14:47:28EDT. It compares7116 with7087 using7103's exact66-task
membership, frozen runner/checkers,12CPU/48000MiB/5h allocation and unchanged
tier solver/checker budgets. Full width, no throttle or node constraint, on
cpu-dense-hwulab-q. Strict7123 requires33 exact reported-work/certificate pairs.
Expected solver outcomes permit dependent continuation while checks run;
pending proofs remain unverified and any invalidity pauses dependent work.

[7105 stage1 timing](../runs/20260910-7105-bve-views-runtime/manifest.json) has
all three six-task waves submitted:1679910 at14:15:53,1679919 at14:25:59 and
1679926 at14:38:41EDT. Wave2 was submitted after all12 prior expected outcomes
matched the frozen7030 work/proof fingerprints, without waiting for checkers.
7103 B/C remains66 expected/33 exact pairs,64 checked-clean and two pending
at the last observation; audit7124 records later published states without
rewriting its frozen7104 seal.

The [complete7107 solver snapshot](../runs/20260910-7105-bve-views-runtime/solver-checkpoint-7107.json)
now contains18 expected outcomes, exact own-repeat and7030 cross-arm work/proof
fingerprints;4 checked-clean and14 pending at seal. Do not rerun its writer.

| Rank | Whole-solver paired median [full range] | Printed BVE median | RSS median |
|---|---:|---:|---:|
|31|1.001670666 [0.998120361,1.005242551]|0.990164073|0.985418623|
|35|1.000832058 [0.997092328,1.007761702]|0.985267315|0.973626929|
|38|0.993287076 [0.992354655,0.997369379]|0.973684211|0.923116043|

All six within-arm ranges are below1%, hence below the2% instability limit.
Stage1 alone is neutral on the main31/35 runtime gaps, not a substantial win.
The Rank38 difference is mostly outside the inprocessing slot; its BVE time
is only37--39ms, so do not attribute its whole-solver gain to faster BVE.
RSS is consistently lower, but this does not establish full50 memory parity.
No repeat is excluded and no repeat-to-select is authorized.

Next: revisit pending proofs. After7123 exact-work gate, measure the combined
prototype in three paired31/35/38 waves and compare
complete Eliminate cost. Printed BVE time is not whole Eliminate. The approved
20% whole-Eliminate Rank35 target remains unproven; do not lower it to make a
small stage look successful. A missed full-prototype premise requires
reassessment, not constants/layout tuning or a substituted easier benchmark.

Audit7124 binds source, pins, logs, fixed proof evidence and published SLURM
results, and runs aggregate tooling/documentation checks. Its first attempt
stopped before writing: it expected another CTest version's summary wording.
All six actual logs report exactly `100% tests passed out of N`; the reader
now matches that full line, with one positive and four rejection fixtures.
An overfull line in the draft Andrew PDF was reworded and recompiled; neither
formatting issue was a solver or correctness-gate failure. No material deletion,
live checker scratch inspection, Claude fuzz-artifact access, fuzz transfer,
denied push retry or promotion. Original7090/7095 failed fixtures remain frozen;
Codex owns debugging per plan§8.2 and Claude debugging was not invoked.
Andrew TeX/PDF is local/unsent in
`report-to-andrew/20260910-1450-bve-import-stage2/`.
