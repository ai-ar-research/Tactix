# AM408: exact competition-proof witness for LymphoSAT's general PB rules

Date: 2026-09-25. This is a bounded, pinned winner replay and proof-rule
classification, not a Tactix implementation change or N1/N2 acceptance.

## Exact evidence

The frozen SC24--26 baseline task 1659 (`benchmarks/baselines/sc24-26-32g-5000s/results/01659.json`) records LymphoSAT on
`tierD24-26/42f1311ce504db8cc0cb590a568da20a-tseitin_n188_d3.cnf.xz`:
UNSAT, valid, 0.034 solver seconds, 80,981-byte PBP, SHA-256
`dd557484c088074bc972e9f2a82f15a3a53410ecb10d7d5232f5aa22058bf1fd`.
The original VeriPB elaboration SHA-256 is
`8cd6ed3756cbc55935fe79dab66556d02b4ae1d98eb0f52c53ac8e7411c495d4`;
both VeriPB and CakePB verified. The baseline deleted both artifacts after
checking and did not record the internal selected producer. This limited the
earlier AM402 source-level audit.

AM408 replayed the exact cached compressed input with the retained baseline
`pins/lymphosat/run.sh` and checkers on hwulab job 1721206 (two CPUs, 8 GiB,
ten-minute ceiling). The job completed 0:0. The composite's stderr identifies
`matcher tseitin-formulas accepted formula` and `Tseitin UNSAT detected by
parity sum`. Its PBP output is **byte-identical to the baseline certificate**:
80,981 bytes and the same SHA-256. VeriPB `-u --cnf --elaborate` again reported
`s VERIFIED UNSATISFIABLE`; CakePB again reported `s VERIFIED UNSAT`. The
re-elaborated 86,304-byte kernel PBP also has the exact baseline SHA-256.
Thus this is the competition-winner solver's exact baseline-run proof, not
merely another formula matched by the source. The baseline marks LymphoSAT
UNSAT as `vbs_eligible: SAT-only`, so this is **not** evidence that this UNSAT
row contributed to its VBS score. The frozen replay script, input, proof, logs and hashes
are in `benchmarks/probes/am408-lymphosat-pbp-producer/`.

The proof begins with `red` constraints introducing extension variable x283,
then a local `proofgoal` with `pol` addition and integer division (`2 d`),
and clausal `rup` steps. Direct line-prefix counts in the entire proof are
376 `red`, 1,509 `pol`, and 564 `rup`; these are syntactic counts, not a count
of independent semantic applications. The selected producer's source is
`winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/tseitin-formulas/solver/solver.cpp`;
the baseline's pinned copy is in `.../pins/lymphosat/solver/solvers/`.

## Disposition

This upgrades AM402's qualified source finding to direct baseline-use
evidence: at least one verified SC24--26 run of a competition-winner solver
relied on general PB
redundancy with fresh variables, local subproofs and arithmetic. Tactix has
those PBP operations in complete Lucky 19/20 startup refutations, but no
general registered live PB action/proof-event path after that boundary.
It does **not** establish that Tactix must copy this particular derivation,
that a clausal DSR alternative cannot verify the same formula, or that an
undeclared action already violates N1. A general typed PB derivation surface
and an explicit format/state transition would be needed before integrating
such a technique without family-specific proof code. Native DSR remains the
canonical search path; no route was changed.

The AM400/401 fuzz and full Debug checks, H2 no-option DSR issue, and
post-finalization C-API SAT-model issue remain open. No cumulative N1--N4,
Rank/B+C/WV or Tier-D acceptance follows from this single proof witness.
