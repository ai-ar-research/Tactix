# Tier B/C against original Kissat and CaDiCaL — 2026-08-12

## Outcome

Both T3 conditions hold on both tiers, measured with all three arms in one
array per tier. Tactix solves every instance either reference solves, and its
PAR-2 is within 1.10× original Kissat's on each tier.

Binary `build/pin-t3-{b,c}/tactix`, commit `6f44f23`, SHA-256
`7f952ff3750c05d691d8504ebbd3ee7b897f6a2250779733d47ee2eee30737b7` (both pins
byte-identical). Kissat 4.0.4 and CaDiCaL 3.0.0 from `tools/solvers/bin`, the
clean original builds. Allocation: partition `cpu-dense-hwulab-q`, four CPUs at
8 GB/core (32 GB), no array throttle, arrays 1617449 (B) and 1617534 (C).
Every arm wrote proofs, so all three time the same workload.

| Tier | Arm | Solved | SAT/UNSAT | PAR-2 | ratio |
|---|---|---:|---|---:|---:|
| B (28 inst, 300 s) | **Tactix** | **28/28** | 14/14 | **9.85** | **1.036** |
| B | Kissat 4.0.4 | 28/28 | 14/14 | 9.51 | 1.000 |
| B | CaDiCaL 3.0.0 | 27/28 | 14/13 | 39.75 | 4.180 |
| C (30 inst, 1200 s) | **Tactix** | **30/30** | 15/15 | **151.83** | **1.067** |
| C | Kissat 4.0.4 | 30/30 | 15/15 | 142.34 | 1.000 |
| C | CaDiCaL 3.0.0 | 29/30 | 14/15 | 299.13 | 2.102 |

`correctness.clean` is true for both runs: no verdict mismatch against the tier
lists, no invalid model, no rejected certificate, no checker give-up. Tactix's
UNSAT proofs were checked and scored (B: 14 proofs, 435.5 MB, 188.4 s; C: 15
proofs, 10.9 GB, 2508.3 s). CaDiCaL misses one instance per tier:
`unsat-set-b-fclqcolor-10-07-09` on B and `128_125` on C.

Raw per-instance evidence:
`benchmarks/runs/20260812-232508-B-tactix-6f44f23/results.json` and
`benchmarks/runs/20260812-232510-C-tactix-6f44f23/results.json`.

## Two standing blockers closed

- **`128_125`** was recorded as a 1200 s Tactix timeout at 32 GB against a
  verified Kissat SAT. It is now a verified SAT in **131.5 s** (Kissat 88.8 s,
  CaDiCaL times out). Not a memory exemption and no longer a solve-set gap.
- **The Tier C early-exit incident** required a real-instance proof-checked
  rerun on an immutable Release pin before performance interpretation could
  resume. This is that rerun: 30/30 with every proof checked and no nonterminal
  exit, and `goldcrest-and-9` — the instance that produced the two optimized
  backtraces — is a verified UNSAT in 175.3 s.

## Where the remaining PAR-2 gap sits

Paired per-instance ratios, the extremes of each tier.

| Instance | Verdict | Tactix | Kissat | CaDiCaL | T/K |
|---|---|---:|---:|---:|---:|
| `veer_axi…p23_transition` (B) | UNSAT | 21.07 | 3.00 | 4.24 | 7.02 |
| `2013113162201nw…sat03-1668` (B) | SAT | 16.52 | 5.88 | 19.43 | 2.81 |
| `DivS_862_11` (B) | SAT | 25.57 | 15.90 | 5.08 | 1.61 |
| `minxor128` (C) | UNSAT | 419.96 | 202.34 | 267.07 | 2.08 |
| `goldcrest-and-9` (C) | UNSAT | 175.31 | 87.39 | 147.19 | 2.01 |
| `T87.2.0` (C) | UNSAT | 375.30 | 231.69 | 377.42 | 1.62 |
| `oddball_13_5_ttf` (B) | UNSAT | 1.12 | 18.92 | 25.32 | 0.06 |
| `SCPC-1000-18` (C) | SAT | 28.22 | 163.84 | 25.62 | 0.17 |
| `Ptn-7824-b18` (C) | SAT | 67.70 | 231.11 | 9.79 | 0.29 |

The aggregate is close because the tails cancel, not because the trajectories
agree: `veer_axi…p23_transition` is still 7× and remains the sharpest single
divergence witness on Tier B, while `oddball_13_5_ttf` and `SCPC-1000-18` run
an order of magnitude faster than Kissat. A PAR-2 inside the gate does not
retire the divergence-closure loop; it changes which instances lead it.

## What this run does not establish

- It is a T3 measurement, not N4 verification: the scheduling-parity ledger is
  reconciled separately.
- Tier D is unmeasured, so no competitiveness claim follows from it.
- The two tiers are 28 and 30 instances. Single-instance movement of a few
  seconds moves Tier B's PAR-2 by a tenth; treat 1.036 as "inside the gate",
  not as a margin.
