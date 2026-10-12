# Tier D across the milestone versions (proof-free)

The first commit that met each development milestone, run on all 1,166 Tier D instances, beside the
current version's proof-checked run.

| Milestone (first met) | Commit | Date | Campaign (`benchmarks/runs/`) | Job |
|---|---|---|---|---|
| Tier A | `dcc3ecbd6` | 2026-07-14 | `20261011-tierd-noproof-1-tierA-dcc3ecbd6` | 1771815 |
| Old Tier B, 14/14 | `546298924` | 2026-07-20 | `20261011-tierd-noproof-2-tierB14-546298924` | 1771816 |
| Tier C | `6f44f23d6` | 2026-08-12 | `20261011-tierd-noproof-3-tierC-6f44f23d6` | 1771918 |
| Rank 1–50 | `f50e46c0c` | 2026-09-18 | `20261011-tierd-noproof-4-rank1-50-f50e46c0c` | 1771919 |
| WV100 | `0a9063387` | 2026-09-27 | `20261011-tierd-noproof-5-wv100-0a9063387` | 1771920 |
| Current (reference) | `0f7c10174` | 2026-10-10 | `20261010-tierd-fastel` + 64 GB recheck | 1770131, 1771469 |

The five milestone runs were made with cpu-q, 4 CPUs, 32 GB and a 5,000 s limit, using
`--certificate none`: no proof path, no proof file, and no proof check. SAT models are checked;
UNSAT answers are unchecked. Each array was throttled to `%125` (user request). The runs left no
residue: every task has `certificate_bytes` null and `artifact_deleted` true, and the five run
directories total 93 MB.

The current version ran in proof mode with every proof checked. Its "solved" column includes 7
solves whose proofs were not verified. Proof mode is not a no-op: proof-only startup features (for
example the Ramsey startup refutation) run only when the solver has a proof path.

## Results

| Version | Solved | SAT | UNSAT | Unsolved | Solver memout | PAR-2 sum |
|---|---|---|---|---|---|---|
| Tier A `dcc3ecbd6` | 625 | 313 | 312 | 541 | 0 | 5,906,120 |
| Old Tier B `546298924` | 735 | 372 | 363 | 431 | 0 | 4,873,735 |
| Tier C `6f44f23d6` | 818 | 433 | 385 | 348 | 1 | 4,021,024 |
| Rank 1–50 `f50e46c0c` | 977 | 459 | 518 | 189 | 1 | 2,331,993 |
| WV100 `0a9063387` | 979 | 483 | 496 | 187 | 1 | 2,277,101 |
| Current `0f7c10174` | 1,006 (999 verified) | 481 | 525 (518 verified) | 160 | 1 | 2,110,652 |

PAR-2 counts an unsolved instance as 10,000 s; the current row counts its 7 unverified solves as
unsolved.

Solver memouts (32 GB): `17.normalised` (Tier C, 53 s) and `pj2002_k500` (Rank 1–50, WV100,
current).

## Step to step

| Step | Gained | Lost |
|---|---|---|
| Tier A → old Tier B | 147 (89 SAT, 58 UNSAT) | 37 (30 SAT, 7 UNSAT) |
| Old Tier B → Tier C | 126 (76 SAT, 50 UNSAT) | 43 (15 SAT, 28 UNSAT) |
| Tier C → Rank 1–50 | 183 (49 SAT, 134 UNSAT) | 24 (23 SAT, 1 UNSAT) |
| Rank 1–50 → WV100 | 39 (33 SAT, 6 UNSAT) | 37 (9 SAT, 28 UNSAT) |
| WV100 → current | 42 (12 SAT, 30 UNSAT) | 15 (14 SAT, 1 UNSAT) |

Tier C → Rank 1–50 is the largest step: +134 UNSAT. Rank 1–50 → WV100 traded 28 UNSAT, five of
them sbdp4, for 33 SAT; the current version recovers the UNSAT count. The last row compares a
proof-free run with a proof-mode run.

## By competition (solved)

| Version | SC2024 (374) | SC2025 (385) | SC2026 (373) | Shared (34) |
|---|---|---|---|---|
| Tier A | 222 | 230 | 152 | 21 |
| Old Tier B | 263 | 270 | 178 | 24 |
| Tier C | 302 | 283 | 209 | 24 |
| Rank 1–50 | 328 | 343 | 278 | 28 |
| WV100 | 330 | 349 | 271 | 29 |
| Current (verified) | 329 | 352 | 289 | 29 |

"Shared" means instances that were in more than one competition (SC2024+SC2025 7, SC2024+SC2026 19,
SC2025+SC2026 8).

## The old Tier B instances in Tier D

10 of the 14 old Tier B instances are Tier D instances. Each entry is the solve time in seconds.

| Instance | Exp | Tier A | Old B | Tier C | R1–50 | WV100 | Current |
|---|---|---|---|---|---|---|---|
| circuit_48in64out_with_800gates | SAT | 92 | 10 | 14 | 10 | 14 | 10 |
| summle_X4044_steps7 | SAT | 50 | 26 | 6 | 11 | 8 | 8 |
| battleship-14-26-sat | SAT | TO | 80 | 2 | 3 | 3 | 16 |
| qwh.50.1250 | SAT | 6 | 5 | 6 | 14 | 9 | 13 |
| combined-crypto1-wff-seed-1 | SAT | 4 | 19 | 2 | 14 | 10 | 11 |
| hwmcc17miters-xits-iso-oski15a08b00s | UNSAT | TO | 217 | 7 | 4 | 4 | 4 |
| FmlaEquivChain_4_6_6 | UNSAT | 63 | 39 | 19 | 32 | 0 | 1 |
| mp1-blockpuzzle_5x10_s7_free4 | UNSAT | 466 | 164 | 10 | 6 | 7 | 8 |
| clique_n2_k10 | UNSAT | 3,590 | 301 | 10 | 0 | 0 | 0 |
| x9-08014 | UNSAT | 46 | 30 | 11 | 12 | 24 | 13 |

The old Tier B version solves all 10 at 5,000 s, but four of them take more than 60 s on this
cluster. That is why it scored 9/14 in the 60 s screen
(`benchmarks/runs/20261011-tierb14-noproof-*`).

## Files

- `results.csv`: one row per (instance, version), 1,166 × 6 rows. It has status, verdict, solve time,
  peak memory and exit code, model and proof check results, competition and old-Tier-B
  membership.
- `wide.csv`: one row per instance; per version: status, solve time, PAR-2.
- `build.py`: regenerates both files and the tables above.

## Limits

- One run per version, on the shared cpu-q nodes at different times. Near-limit instances flip.
- The milestone runs are proof-free and the current run is proof-checked, so the last column
  differs in mode as well as in code.
