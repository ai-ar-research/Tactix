# Graph-labeling recognition: region-bounded cost

Change: `8e953218f` (recognizer and profile) plus the follow-up that moved this
derivation out of the source comment. Graph labeling stays off in
DefaultPolicy. Every measurement below uses `--default-graph-labeling=on`.

## Problem

When graph labeling was enabled, authentication charged 45-100M Recognition
units on inputs where it recognized nothing:
- WV2-89 row 046 (`17.normalised`) used the full 100M and was exhausted.
- Rank1-50 rows 004/007/009/012/013 used 45-93M each.
- The 100M Recognition dose in `GraphLabelingModel` was an unbacked constant.

## Root cause

Authentication runs once, at startup. There is no repeated rescan across root
windows. The cost came from the recognizer reading far more of the formula
than its contract can use:

1. **Pass 1 scanned the whole clause arena** to choose a dimension `n`. It
   charged one unit per clause plus literal reads.
2. **Pass 2 rescanned the whole arena** and charged `n` per candidate clause
   for a full row reset. That is O(clauses * n).
3. **The edge pass looped over all `2*nvars` literals.**

The recognizer's contract already confines every usable clause to the primary
region:
- A column cover holds only primary variables, whose external identities lie
  below `n*n`.
- A plausible dimension satisfies `n <= maximum_vertices` and
  `n*n <= variables`.
- Edges are binaries between primary literals.

None of the scanning outside that region can contribute.

An offline census reproduced the c1 cost model on the raw CNFs (no solver run;
`benchmarks/runs/20261007-glrec-census/`, job 1756067). On every
non-applicable row, pass 1 finds no plausible width (`n = 0`), so the whole
cost is pass 1's linear arena scan of about 2 units per large clause:

| Input | Large clauses | Pass-1 units (raw CNF) | Region large clauses | Owner `infer_n` |
|---|---:|---:|---:|---:|
| 17.normalised (WV2 046) | 99,487,100 | 199,598,207 | 1,784,523 | 0 |
| 9.normalised (R1-50 004) | 49,730,100 | 99,772,207 | 904,523 | 0 |
| 32_325 (R1-50 007) | 30,362,646 | 60,792,591 | 398,743 | 0 |
| 128_125 (R1-50 009) | 40,924,732 | 81,922,239 | 534,305 | 0 |
| 128_100 (R1-50 012) | 32,722,982 | 65,504,189 | 438,580 | 0 |
| 64_200 (R1-50 013) | 32,946,446 | 65,958,116 | 426,494 | 0 |
| abw-K-dwt__234 (Rank991) | 38,376 | 76,770 (n = 117) | 38,277 | 117 |

On Rank991, pass 2 cost 4,659,876 units: 38,376 candidates times the n = 117
row reset. The owner's antibandwidth recognizer is LymphoSAT
`solvers/antibandwidth/solver/solver.cpp`, using `infer_n` and
`extract_instance`. It reads only the size histogram and the binaries among
variables up to `n*n`. Its cost is linear in what it parses, and it has no
separate recognition dose.

## Repair (`graph_labeling::recognize`)

1. **Pass 1 reads only the primary region.** It walks the watch lists of the
   externals below `min(maximum_vertices, isqrt(variables))^2`. Each large
   clause is read once, from the watch of `lits[0]`. The guard requires
   connected large watches, so every irredundant large clause is watched by
   `lits[0]` and `lits[1]`. Any plausible clause has all literals below
   `width^2 <= side^2`, so the region is complete for the existing contract.
2. **Pass 2 walks the watch lists below `n*n`.** It restores rows from a
   touched list, so a candidate clause costs its literals rather than `n`.
3. **The edge pass walks only the `n*n` negated primary literals.**

The accept set and the published `Shape` are unchanged. The result does not
depend on visit order: `n` is a maximum, and the column, entry and edge sets
are order-independent.

Recognition work is now linear in the region's watch occupancy and clause
literals, plus O(n*n). It does not depend on the rest of the formula. The
100M dose is removed: Recognition takes the remainder of the representable
profile ceiling, so the stage sum is exactly `UINT64_MAX` and the profile stays
`valid()`. The kernel work budget and interrupts still apply. Recognition
consumes no formula change, and proof behavior is untouched.

## Validation

Job 1756128 (`benchmarks/runs/20261007-glrec-validate/`) built from the
archived `8e953218f` source. Debug and ASan/UBSan Debug ran 15 tests each:
- `test_graph_labeling_{recognition,model,actions,completion,search,window_cursor,repair}`
- `test_graph_interval_equivalence`, `test_strategy_config`,
  `test_native_policy_feedback`, `test_random_online`,
  `test_random_controller_policy`, `test_lucky`, `test_online_api`,
  `test_online_inprocess`

All 15 tests exit 0 in both modes. `test_lucky` took 840 s under Debug and
2,229 s under ASan. Results are in `results/{debug,asan}/summary.tsv`. The Release pin is
`build/pin-glrec-rel-8e953218f` (tactix sha256 `77a20d17…`).

New tests:
- An indexed rectangle with up to 20,000 unrelated clauses beyond the region
  gives identical work and an identical shape.
- Each region clause that indexes no column costs at most 7 units, below the
  old `n`-row reset.
- A 50,000-clause formula without a rectangle is rejected after exactly one
  unit per region variable.
- The default profile is `valid()`, its stage sum is exact, and it
  authenticates a fixture.

## Same-window A/B (job 1756160)

Campaign `benchmarks/runs/20261007-glrec-ab/` was prepared by
`benchmarks/runs/20261007-glrec-ab-prep/prepare.py`. That preparer follows the
`controller_regression.py` shape: `runtime_task.py`, anonymous scratch, and
canonical checkers. The setup:
- Arms: c1 Release (`build/pin-candidate-c1-rel`, `c85bf484…`) and glrec
  Release (`77a20d17…`), both run with
  `--auto-proof --default-graph-labeling=on`.
- c1's source archive equals `8e953218f` except for the two changed files, so
  the A/B isolates this change.
- Allocation: cpu-dense, 12 CPUs, 32 GiB, solver pinned to one CPU, 1,200 s
  solve limit.
- "Recognition" below is `graph-schedule authentication_work`. Times are wall
  seconds, with the solver's own `c timing … solve` value in brackets.

| Row | Exp. | c1: verdict, wall [solve], node | glrec: verdict, wall [solve], node | Recognition c1 → glrec | Recognized (n) c1 / glrec | Graph SAT | Check c1 / glrec |
|---|---|---|---|---:|---|---|---|
| fixed44 991 `abw-K-dwt__234.mtx-w50` | SAT | SAT, 22.8 [22.7], node017 | SAT, 26.6 [26.5], node017 | 4,928,386 → 522,636 | 1 (117) / 1 (117) | 1 / 1 | verified / verified |
| WV2-89 192 `17.normalised` | SAT | SAT, 123.6 [59.6], node017 | SAT, 133.8 [68.0], node018 | 100,000,000 (exhausted) → 146,473 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| Rank1-50 5 `9.normalised` | SAT | SAT, 59.6 [28.4], node017 | SAT, 59.4 [28.2], node017 | 93,344,662 → 146,473 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| Rank1-50 8 `32_325.sanitized` | SAT | SAT, 93.7 [71.2], node018 | SAT, 78.4 [60.1], node017 | 55,916,880 → 130,830 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| Rank1-50 10 `128_125.sanitized` | SAT | SAT, 114.7 [88.1], node018 | SAT, 113.5 [88.1], node017 | 75,501,085 → 196,952 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| Rank1-50 13 `128_100.sanitized` | SAT | SAT, 88.9 [68.9], node017 | SAT, 88.6 [68.6], node017 | 60,362,435 → 196,952 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| Rank1-50 14 `64_200.sanitized` | SAT | SAT, 86.7 [66.8], node018 | SAT, 86.3 [66.2], node018 | 60,753,994 → 147,470 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| Rank1-50 12 `hwmcc17miters-xits-iso-6s281b35.sa` | UNSAT | UNSAT, 12.8 [11.6], node017 | UNSAT, 12.4 [11.5], node018 | 13,327,858 → 20,684 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| B28 8 `hwmcc17miters-xits-iso-oski15a08b0` | UNSAT | UNSAT, 3.9 [3.4], node018 | UNSAT, 3.9 [3.4], node018 | 3,443,440 → 218,786 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| C5 31 `circuit_32in32out_with_500gates_6i` | SAT | SAT, 0.9 [0.4], node018 | SAT, 0.9 [0.3], node018 | 4,148,928 → 6,167,953 | 0 (0) / 0 (0) | 0 / 0 | verified / verified |
| near-family 0 `fixedbandwidth-eq-31_shuffled` | UNSAT | UNSAT, 990.8 [990.8], node018 | UNSAT, 991.2 [991.1], node018 | 1,080 → 1,643 | 0 (0) / 0 (0) | 0 / 0 | pending / pending |

- **Certificates.** Every row's certificate sha256 is identical across the two
  arms. Every SAT model passed `checkmodel`, and every finished UNSAT proof
  check verified.
- **Near-family control.** `fixedbandwidth-eq-31` is UNSAT in about 991 s in
  both arms, with an identical 1,714,752,731-byte certificate. Its two proof
  checks were still running when this report was written; their outcome lands
  in `results/020.json` and `results/021.json`.
- **Rank991.** The trajectory is unchanged. Both arms recognize the same
  117-vertex shape and finish SAT through graph completion. Total graph work
  differs only by the recognition saving (5,063,668,149 vs 5,059,262,399
  units). Post-recognition work is 5,058,739,763 units in both arms.
- **Instructions on Rank991 are equal.** Both arms ran on node017:
  351.434e9 vs 351.472e9 retired instructions (+0.01%). The measured
  22.7 s vs 26.5 s solve time comes from cycles: 86.3e9 vs 101.0e9, an IPC of
  4.07 vs 3.48.
- **Same-node non-applicable rows (c1 vs glrec).** These show the recognition
  saving directly:

  | Row | Instructions | Solve time |
  |---|---|---|
  | 9.normalised | −3.085e9 | 28,416 → 28,173 ms (−243 ms) |
  | 128_100 | −1.290e9 | 68,863 → 68,624 ms (−239 ms) |
  | 64_200 | −2.335e9 | 66,823 → 66,247 ms (−576 ms) |
  | oski15a08b00s | −0.109e9 | 3,397 → 3,379 ms |

- **WV2-89 row 046 (17.normalised).** Recognition fell from 100,000,000 units
  (exhausted) to 146,473. The arms ran on different nodes (node017 vs
  node018). The measured values are as follows:

  | Arm | Solve time | Wall | Instructions |
  |---|---:|---:|---:|
  | c1 | 59,625 ms | 123.6 s | 1033.989e9 |
  | glrec | 68,025 ms | 133.8 s | 1058.896e9 |

  The certificates are identical. The recognition saving on this row is the
  100M units shown in the table.
- **circuit_32in32out.** This is a small formula whose region covers it
  entirely. Recognition units rose from 4.15M to 6.17M because the watch walk
  also reads binary watches. Solve time was 354 ms vs 342 ms, and wall time
  was 0.868 s in both arms.

Graph labeling recognized only Rank991 in the recorded campaigns from
2026-10-05 to 2026-10-07. No other suite row is an applicable positive. The
near-family `fixedbandwidth-eq-31` row is a non-applicable control.

## Open

- Small formulas whose region is the whole formula can now charge more units
  than the arena scan did, because binary watch entries are read. The
  measured case is 6.2M units on circuit_32in32out.
- `src/strategy/native_feedback.hpp` sums stage allowances without
  saturation. That is harmless now that the profile sum is exact, but
  saturation would be more robust.
- The default profile's other stage doses remain explicit experimental
  inputs. Production enablement still needs the multi-instance SAT cohort and
  the four enabled gates (`2026-10-07-lymphosat-overfit-audit.md`).
