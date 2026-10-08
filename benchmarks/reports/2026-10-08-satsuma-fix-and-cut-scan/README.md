# Satsuma fix_and_cut clause scan (battleship cut-point gap)

Date: 2026-10-08. Base commit 3149de667. Release pin `build/pin-satscan15-rel`
(sha256 90943be3…; source = commit tree minus the later comment shortening and
tests). Validation sources: `benchmarks/runs/20261008-sbwin-build/pins17/`.

## 1. Divergence against the owner

Owner: `winners/satsuma-iter-kissat-sc2026/.../satsuma-dev/src/symmetries.h`
`fix_and_cut` (≈3060–3250), `cnf.h` `is_amo` (405), `common_clauses` /
`mark_clauses` (477–524).

| Point | Owner | Tactix c4 (`FirstAmoThenForwardRemainder`) |
|---|---|---|
| Clause order | clause database in input file order | binaries from watch lists, then large clauses in arena order |
| Selection | first applicable clause; later clause replaces only if AMO (scan ends) or `common >= score_v` and smaller orbit | ranks all candidates: AMO, common score, smallest orbit |
| `score_v` | per level 0; the inner `int score_v` shadows the outer; `score_i = common(i) >= score_v` is a bool, so `score_v` becomes 1 after the first pick | numeric common score |
| Base point | clause's first literal | literal with best common score |
| AMO preference | ends permanently once a scan reaches the last clause (`there_is_no_amo`) | always on |
| `is_amo` | every pair has its complement binary | same (`EligibleClause.amo`) |

The 60 vs 33 generator difference does not matter: both group sets give the
same orbits at the levels taken (13/13 owner clauses below).

## 2. Repair

New `SymmetryBaseSelection::SatsumaFixAndCut` (backend value 7, C constant
`TACTIX_ONLINE_SYMMETRY_BASE_SATSUMA_FIX_AND_CUT`, replay schema `sr31`) does the
owner loop exactly. When it is selected, eligible clauses are ordered by clause
ID: `FormulaSnapshot.clause_ids` comes from `kernel_binary_id` and `clause->id()`,
which is the owner's source order. DefaultPolicy's structured stages select it,
with `first_literal_fix_anchor` on. Remaining deviation: binary literal
orientation is by literal code, and large clauses keep arena literal order
(implementation-notes, FixAndCut paragraph).

## 3. Clauses against Satsuma (`20261008-cutpoint-sbp`, `tools/sbp.py`)

| Row | Satsuma | c4 shared | satscan15 shared |
|---|---|---|---|
| battleship-14-26 | 13 | 2 | **13** (+3 lines on proof-only extension var 365 > 364 vars, also in c4) |
| mp1-Nb5T15 | 16 | 16 | 16 |
| or_randxor_k3_n540_m540 | 529 | 418 | 418 (unchanged, separate gap) |

Battleship trace: depth 0 is clause 0 `(-1 -2)` orbit 56, depth 1 is `(-2 -4)`
orbit 6, depth 2 is `(-15 -16)` orbit 28, then 10 cuts with orbit 2. This is
the owner's log.

## 4. Same-CPU runs (`20261008-satscan-pair-*`, `20261008-satscan-shuf-battleship-s*`, hwu -c 8 / 32 GiB)

| Row | c2 | c4 | scan |
|---|---|---|---|
| battleship orig | 0.82 s (33.5k conf) | 13.9 s (662k) | 34.5 s (1.34M) |
| battleship s1..s5 | 4.5, 15.0, 1.3, 2.0, 44.0 | 5.7, 37.7, 17.5, 32.8, 21.9 | 3.1, 1.7, 14.9, 48.8, 3.7 |
| battleship median of 6 | 3.25 | 19.7 | **9.3** |
| mp1-Nb5T15 | 27.2 s | 132.0 s (3,764,636 conf) | 133.6 s (3,764,636, same trajectory) |
| or_randxor | 13.2 s | 15.1 s (730,761 conf) | 14.8 s (730,761, same trajectory) |

Satsuma+Kissat on the same six battleship copies: 6.7, 16.1, 6.2, 25.6, 16.7, 6.4
(median 11.4). All verdicts and models are valid.

Original battleship is slower than c4. Profile (`20261008-veer-perf`, TAG
bship): structured startup is 0.034 s in both arms, and the flat profiles have
the same shape (kernel_propagate 43% vs 48%). The extra time is 2.0x conflicts
at a similar per-conflict cost. This is search trajectory, not a long pass. Over
the six copies, the median of the owner scan is about 2x better than c4's and
close to Satsuma+Kissat.

## 5. Validation

pins17 (final tree): focused Debug 101/101 + 4 fixtures verified; ASan/UBSan
focused 101/101 + 4 fixtures; full Debug 355/355. pins16 failed on the C-API range test
(fixed) and comment_hygiene (fixed). New tests: `satsuma_fix_and_cut_takes_first_applicable_clause`
(owner scan picks the first triangle clause, the ranked mode picks the smaller edge orbit) and
the sr31 round-trip/legacy rejection test.
