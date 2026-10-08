# Checker time for fast-solving UNSAT rows under the current proof pipeline

Small evidence copies are in `data/`. Run data: `benchmarks/runs/20261007-proofroute-fast-unsat/` (scripts, pin hashes
in `pins/SHA256SUMS`, per-stage stdout and `/usr/bin/time` files, `perf`
samples, `jobs.tsv`). Certificates came from the b26 Release pin
(`b26efc0f…`, `--auto-proof --default-graph-labeling=on`) and were measured
with the fixed44 checker pins: drat-trim `f2cb7d45…`, dsr-trim fixed64
`30207e6f…`, dsr-compress `8ddab830…`, Trestle `19bf79f4…`, CakeLPR `ab9dc6c4…`.
Proofs, formulas, DRAT, LRAT and LSR were deleted. Only their SHA-256, byte
counts, line statistics and 4 KiB prefixes are kept.

The user decided to give the checker a longer window, so this report records
only stage costs and window estimates. On wrap-up instruction, the jobs still
running were cancelled at about 5,480 s of wall time.

## Per-row stage times and window needed

The current route is selected by content:
- witness-free DSR goes DSR→DRAT (Python conversion), then drat-trim, then CakeLPR;
- witnessed DSR goes through no-option dsr-trim, then dsr-compress -d, then Trestle.

| Rank | Solve s | Proof | Route | Measured stages (s) | Dominant stage | Total checker time needed |
|---:|---:|---|---|---|---|---|
| 541 | 71.5 | 229.7 MB, 0 SR | DRAT | route scan 46 + conversion 77; drat-trim **unfinished at 5,334 s**, 58.2 % of additions | drat-trim `checkRAT` (96–98 % of perf samples) | **Extrapolated ≈ 34,000 s.** The bound is 15,400 s from the measured late rate of 2.5 %/600 s. 34,000 s comes from the owner Kissat 4.0.4 proof of the same row, which has the same shape (33,958 s drat-trim + 32 s CakeLPR) |
| 1129 | 76.9 | 223.1 MB, 0 SR | DRAT | 45 + 76; drat-trim **unfinished at 5,330 s**, 54.4 % | drat-trim `checkRAT` (97 %) | **Extrapolated ≈ 36,500 s.** The bound is 17,200 s from the linear rate. 36,500 s comes from the owner Kissat 4.0.4 proof (36,473 s) |
| 291 | 275.4 | 464.9 MB, 20 SR | DSR | dsr-trim 974 (VERIFIED) + compress 14; Trestle **unfinished at ≈4,350 s** on a 1.95 GB LSR | Trestle | **Extrapolated > 44,000 s, upper bound unknown.** Measured lower bound ≈ 5,350 s. The owner Satsuma's Trestle timed out at 44,228 s on its own 419 MB proof |
| 1075 | 134.2 | 677.0 MB, 32 SR | DSR | dsr-trim 4,750 (VERIFIED) + compress 10 + Trestle 263 (VERIFIED) | dsr-trim backward UP (`perform_up_for_backwards_checking` 53–56 %, `mark_dependencies` and `mark_and_store_up_refutation` about 21 % each) | **Measured: 5,023 s, VERIFIED** inside 14,400 s. The historical timeout was a different 1.09 GB proof from generator `2fc7421f` |
| 82 | 1,302 (d170) | 2.06 GB, DRAT route | DRAT | conversion ≈ 1,250 (14,400 − drat-trim's 13,148 s allowance); drat-trim timed out | drat-trim | **Extrapolated > 41,500 s:** conversion 1,250 + drat-trim > 40,000 + CakeLPR ≈ 380. The owner Kissat 4.0.4's 2.06 GB proof did not finish drat-trim in 40,000 s. kissat-sc2024 took 6,491 + 381 s |
| 489 | 836 (d170) | 2.52 GB | DRAT | conversion ≈ 1,410; drat-trim timed out at 12,988 s | drat-trim | **Extrapolated > 41,600 s.** Kissat 4.0.4 > 40,000 s; sc2024/AE 8,181–8,907 s |
| 1085 | 1,447 (d170) | 2.69 GB | DRAT | conversion ≈ 1,570; drat-trim timed out at 12,830 s | drat-trim | **Extrapolated > 41,800 s.** Kissat 4.0.4 > 40,000 s; AE 11,748 + 256 s; sc2024 17,049 + 226 s |

Ranks 82, 489 and 1085 were not regenerated. Their numbers come from the saved
d170 records (`benchmarks/runs/20260928-am546-no-mab-tactix-hybrid-tierd-cpuq`,
results 081/488/1084) and the frozen owner baseline. c2's full44 job 1756297
will supersede them.

## What dominates

- **541, 1129: drat-trim's RAT check.** drat-trim's `checkRAT` collects RAT
  candidates by walking every watch list, so each core RAT lemma costs time
  proportional to the whole live database. These formulas have 11.1 M
  clauses, almost all binary. Factor's definitions are about 2.27 M
  first-time fresh-variable RAT lines (124.7 k and 130.1 k extension
  variables). Kissat 4.0.4's proof of 541 has the same shape: 118,624
  factored variables, 2.25 M definition lines, and a scan proxy of 1.16e13 for
  both solvers. The drat-trim progress bar shows 25 % in the first 600 s, then
  about 2.5 % per 600 s. This is a property of drat-trim combined with Factor
  on this family, not of Tactix's emission.
- **291: Trestle on a Factor-heavy LSR.** The proof has 1.54 M Factor
  definition lines and 1.65 M RAT-hinted LSR lines. The route is fixed to
  Trestle by 20 startup symmetry-breaking SR lines with 25 k–110 k-literal
  permutation witnesses, at proof lines 2–67 and about 26.5 k.
- **1075: dsr-trim backward unit propagation** on a 16.5 M-addition proof.
  Fresh-pivot RAT is negligible here (scan proxy 6.3e10). The 32 SR lines are
  root units proved by a two-variable sign flip.
- **82, 489, 1085: drat-trim.** Python DSR→DRAT conversion costs about
  0.53–0.61 s/MB, which is 1,250–1,570 s of the window on 2–2.7 GB proofs.
  All three sc2024/AE owner proofs verify. These two Kissat releases have
  identical Factor options and differ in startup `fastel`: 1 in sc2024, 0 in
  4.0.4. Tactix's `fastel` eliminated 0 variables on all three rows, and each
  row then ran Factor (556, 143 and 1,429 factorings). The resulting
  hypothesis is the same `checkRAT` cost on 15–34 M-clause live databases. It
  is **unconfirmed**: no Tactix certificate of these rows was available.

## Other measurements (not current-route evidence)

- The no-option dsr-trim → dsr-compress -d → CakeLPR chain, which is the
  pinned `proof_oracle.check_dsr(witness_free=True)` path, verified 541 in
  391.8 s end to end (job 1755824). It also verified 1129 in 343 + 7 + 72 s.
  On rank 541, CakeLPR rejected three corrupted LSR controls:
  - empty clause dropped;
  - one RAT hint group dropped;
  - one hint of the empty-clause line dropped.

  The user declined route changes, so this chain is recorded only as
  measurement.
- Trestle on the witness-free LSRs of 541 and 1129 was still running when
  cancelled, at about 5,150 s.
- The pre-fixed64 dsr-trim `3b5c…` took 273 s on 541 and 307 s on 1129, so the
  fixed64 patch does not change speed.

## Jobs

| Job | Purpose | Outcome |
|---|---|---|
| 1755558 | generation array | completed |
| 1755624 | Kissat 4.0.4 541 proof stats | completed |
| 1755616, 1755619 | DRAT route | cancelled at about 5,480 s |
| 1755617, 1755620 | dsr-trim fixed64 chain | dsr-trim and CakeLPR verified; Trestle cancelled |
| 1755618, 1755621 | dsr-trim 3b5c | completed |
| 1755622 | 1075 | completed, VERIFIED |
| 1755643 | 291 | dsr-trim verified; Trestle cancelled |
| 1755824 | 541 end-to-end run and controls | completed |
| 1755825 | | cancelled while pending |

Pending jobs 1755616–1755622 and 1755624 were moved from cpu-dense-hwulab-q to
cpu-q with `scontrol`, because other gate arrays had filled the lab queue.
After the user rule that requires the runner, no further hand-rolled solver
job was submitted.
