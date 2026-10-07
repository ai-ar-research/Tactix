# Persistent affine root maintenance after Sweep (am-rel-23)

A well-formed persistent `AffineMatrixInitialize` root-maintenance request
(profile `TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1`, op 3) returned
`InvalidRequest` after a root Sweep (handoff
`benchmarks/runs/20261007-firstopen-original-seed-fuzz-owner/handoff/am-rel-23/`).
It is repaired at the matrix event owner. The original driver now reaches
public `Unsat` there, with a checked DSR refutation.

## Root cause

A Debug gdb trace of the handoff driver (`benchmarks/runs/20261007-affine-rootmaint-diag/`,
jobs 1755548 and 1755549) showed this path. The persistent matrix admitted
four rows `0 = 1` at initialization: the input's XOR system is inconsistent,
and the selected sources cancel every variable. Initialization publishes
nothing. Above-root SearchSteps correctly decline. Sweep then fixed root units,
so the matrix trail was `-5 -6 8 9 10 -20`. Root maintenance ran the service
step, which selected the unconditional conflict row with sources
`[1,2,5,6,9,10,14,17]`.

`make_reason` built the reason from every assigned variable in the union of
the sources' supports. That gave `[5,6,-8,-9]`, although each of those columns
cancels in the source sum. The transaction certificate refutes this sum while
it accumulates the sources. It emits the empty clause, and `export_reason` then
declines any target. The failure surfaced as `UnsupportedProof`, which
`affine_matrix_search_refresh` maps to `AffineMatrixInitStatus::Invalid`, and
from there to public `InvalidRequest`.

The lifecycle contract already says the reverse. An authenticated empty reason
requires a certificate refutation (`docs/implementation-notes.md`, X9 row), and
the transaction accepts a refuted certificate only with an empty reason. So
the producer broke the invariant that the empty reason belongs exactly to the
unconditional contradiction. Sweep's substitution and retirement were not the
cause. Sweep only made the cancelling support columns root-true.

## Repair

`src/rules/semantic_extension/affine_matrix.cpp` and `.hpp` change in two places.
- `make_reason` now tracks each support variable's parity in the source sum,
  using `std::map<External,bool>` plus an odd-entry count in place of
  `std::set<External>`.
- The sliced synchronizer (`Continuation::support`/`residue`) gets the same
  tracking.

A conflict whose sources leave no odd variable is the unconditional `0 = 1`,
and its reason is the empty clause. Every other event keeps the existing
union-support reason. Implications, conditioned conflicts, cache keys and
search-time reason widths are unchanged.

`AffineMatrix::authenticates` recomputes the reason through the same
`make_reason`, so prepare/publish authentication stays consistent. Reason work
and byte reserves are unchanged: a map node `(int32,bool)` occupies the same
aligned tree node as a set node of `int32`.

## Regression

`tests/test_affine_matrix_persistent_lifecycle.cpp` adds
`root_maintenance_after_sweep_refutes`. It runs in three formats: DSR binary,
DSR ASCII and DRAT ASCII. The test uses three sources whose sum is `0 = 1`
while columns w=5 and v=7 cancel, plus backbone clauses for w and v. It runs
this sequence:
1. Persistent initialize.
2. A decision and a SearchStep above root, which returns Inapplicable.
3. A restart to root.
4. Two real `sweep_try_with_limits` calls at depth 1. These prove w and v as
   backbones. Sweep also proves the equivalences a≡¬b and c≡d, which are
   visible in the proof, and substitutes them while the persistent session is
   attached.
5. Root maintenance through `affine_matrix_search_refresh(..., false)`.

The test asserts that no round is `Invalid`. It also asserts `AppliedPartial`,
`Unsat`, an inconsistent kernel, an empty installed reason, and no leaked
premise bytes or unbacked deletions. `affine_matrix_persistent_lifecycle_proof_model.sh`
checks the three refutations: DSR through `proof_oracle.py check-dsr
--every-lemma`, and DRAT through `proof_oracle.py check` (drat-trim + CakeLPR).

## Validation

All runs used SLURM on base `9adfa6295`
(`benchmarks/runs/20261007-affine-rootmaint-validate/`). Archive `after.tar.gz`
has sha256 `37fac7f6…` and `before.tar.gz` has sha256 `cc0a5e89…`. The checker
binaries are hashed in `results/*/checkers.sha256`.

| job | arm | result |
|---|---|---|
| 1755610_0 (1755611) | after, Debug | 40/41 affine CTest entries passed; 5/5 online/strategy suites passed; direct regression passed; 3/3 refutations VERIFIED UNSAT; amdrv2 repro gave `rootmaint:Unsat` and `s UNSATISFIABLE` twice, byte-identical, with the proof VERIFIED (drat route) and by check-dsr every-lemma |
| 1755610_1 (1755612) | after, ASan/UBSan | same as Debug, with no sanitizer report |
| 1755610_2 | before, Debug (regression only) | aborts at `maintained.status!=AffineMatrixInitStatus::Invalid`, so the regression reaches the defect |
| 1755651_0/1 | corrected proof-model script on the Debug/ASan pins | passes: 16 models and prefixes plus 3 root-maintenance refutations |
| 1755674 | negative control | the Sweep-only 14-line prefix is rejected by dsr-trim; the full proof is VERIFIED UNSAT |

The one failing CTest entry in 1755610 was
`affine_matrix_persistent_lifecycle_proof_model`. The new script section
assumed the `ProofFormat` numbering 0/1/2 for the three formats; the actual
numbering is DsrBinary=0, DsrAscii=1, DratAscii=3. Only the script changed
after that run (sha256 `4f26016a…`), and 1755651 validated it against the same
immutable binaries. The direct C++ regression binary passed in 1755610.

1755651 also ran a truncated-refutation control that drops only the `0` line.
That control is vacuous: the remaining certificate lemmas are already
UP-refutable. 1755674 replaced it with the prefix control.

Attempt 1 (1755554, on `3fc9adfa7`) failed to compile the test because of a
misplaced namespace brace. Its logs are in `attempt1-compile-error/`.

## Unresolved

- For non-contradictory events, the reason is still the assigned union
  support, not the minimal residue. This is a weakening the certificate
  accepts. Changing it would alter search-time reasons, so it is out of scope.
- `affine_matrix_search_refresh` still maps a service `UnsupportedProof` to
  `AffineMatrixInitStatus::Invalid`, which becomes public `InvalidRequest`.
  A direct SearchStep maps the same case to public `Inapplicable`. No path
  reaches it after this repair, but the asymmetry remains.
