---
name: benchmarks
description: Run performance evaluation on the tiered benchmark sets (A–D), build or update tier lists, compute PAR-2 scores, or compare against baseline solvers (Kissat, CaDiCaL, MiniSat). Use whenever measuring solver performance, deciding which tier fits a change, or writing a benchmark report.
---

# Benchmark tiers and evaluation

Authoritative spec: DEVELOPMENT_PLAN.md §8.1 and §8.3. This skill operationalizes it.

## Tier selection — pick the cheapest tier that answers the question

| Tier | Contents | Per-instance timeout | When to run | Total cost |
|---|---|---|---|---|
| A | ~200 generated instances, known status: seeded random 3-SAT (n=50–300; phase-transition m/n≈4.26, plus easy m/n≈3 SAT and m/n≈5.5 UNSAT), PHP(5..8), XOR chains, small colorings | 30 s | every commit; any correctness-adjacent change | < 2 min |
| B | 10–15 SC 2023–2025 main-track instances with Kissat 4.0.4 runtime in the 5–30 s band here | 300 s | on every meaningful change; performance claims during development (small set — repeat runs, compare per-instance medians; plan §8.1 caveat) | 2–8 CPU-min |
| C | ~30 SC instances, Kissat ≤ 300 s (rev 23) | 1200 s (rev 29) | weekly; before merging heuristic/scheduling changes | few CPU-h |
| D | Full SC 2024 + SC 2025 main tracks (400 each) | 5000 s, 16 GB cap | milestone gates (plan §9) and external claims only | days |

Hard rules:
- **Never tune heuristic defaults on Tier A** (random 3-SAT rewards the wrong heuristics). Tiers A–B answer "correct and not embarrassingly slow"; only C–D answer "competitive".
- Tier lists are frozen, versioned files in `benchmarks/tiers/*.list` (instance name + expected status + reference runtime). Changing a list requires a commit explaining why — never silently.
- Instances live in `benchmarks/cache/` (untracked); `tools/bench/fetch.py` downloads by hash from the Global Benchmark Database, host **`benchmark-database.de`** (`/file/<hash>`; the old `gbd.iti.kit.edu` alias no longer resolves).

## Building/refreshing Tier B or C lists

0. **Candidate pool:** `curl https://benchmark-database.de/getdatabase` → GBD's *meta* sqlite (`features` + hash→value tables `track`, `filename`; join, don't read `features.track` directly), `…/getdatabase/base` → structural features (`clauses`, `variables`, `bytes`). `track ∈ {main_2023, main_2024, main_2025}` is 400 instances each; drop `result = unknown`. Worked example (1186-instance pool → 721 screened): `benchmarks/reports/2026-07-29-tier-b-expansion.md`.
1. Run Kissat 4.0.4 (the pinned `tools/solvers/bin/kissat`) over the candidates as a job array (one task per instance, **sbatch only**), proof logging on so the numbers match a real tier run.
2. Select instances in the tier's reference band (Tier B: cluster-measured [3.5, 19] s, the span of the core 14 — the lower bound keeps trivial instances out; Tier C: ≤ 300 s), stratify ~50% SAT / 50% UNSAT, cap instances per benchmark family (Tier B: 2; Tier C: 3), and confirm the verdict against the competition status.
3. Record: instance, status, Kissat runtime as a **median of ≥ 3 repeats in one window**, per-instance spread (an instance whose spread survives `--throttle 5` is unusable — it will forge A/B verdicts), and the allocation shape.

## Running an evaluation

- Environment (plan §8.1, rev 64): the SLURM cluster, partition `cpu-dense-hwulab-q`. Tier runs are **job arrays — one single-threaded solver run per task**, 8 GB/core, candidate + baselines in the same array: `run.py ... --slurm [--throttle N]`. Record the **allocation shape** (partition, cpus/mem per task, throttle) in every report and only compare numbers taken at identical shapes; use `--throttle` for Tier C/D gate runs where timing precision matters. **No solver runs outside submitted jobs (rev 65):** every solver execution — including one-off probes and spot checks — goes through `sbatch` (`run.py --slurm`, or `sbatch --wrap` with `--partition=cpu-dense-hwulab-q --mem-per-cpu=8G`); never on the login node, never via `srun` from the session shell (it becomes a step of the session's own job). The local `--jobs` mode is for harness debugging only. Submission patterns, `--time` sizing, monitoring, and cleanup discipline: **slurm-jobs skill**.
- **Sequencing rule (plan §8.1 rev 18): do not evaluate Tier B while any
  Tier A instance is unsolved within its 30 s budget** — clear Tier A
  completely first.
- Runner: `tools/bench/run.py --tier B --solver ./build/release/tactix [--baseline kissat] --slurm` (submits the array + a dependent aggregate job; `--aggregate <rundir>` re-merges any time; `--dry-run` inspects without submitting; local smoke: `[--jobs 4]`). It must: record wall time + exit code + verdict, verify verdict against the expected status in the list, and **check the proof for every UNSAT answer** (drat-trim; a rejected proof counts as unsolved, competition-style).
- **Proof-generation protocol (plan §8.1, rev 72 — user decision 2026-07-27): every arm writes proofs, ours and every baseline.** Proof-free baseline numbers are not comparable to ours and must not be reported as a ratio (the tax is solver-specific: Tier B 2026-07-27 — tactix +6.9 %, kissat +4.4 %, CaDiCaL +5.0 %). Two deliberate asymmetries, restate them in the report: a failed/missing proof scores **our** instance unsolved, while a **baseline's** failed check is recorded and never scored against it; and the checker gets the competition-scale budget (§2), not `2 ×` the solve timeout. An arm whose command carries `--lrat` writes native LRAT checked by cake_lpr; otherwise binary DRAT checked by drat-trim. DRAT stays the scored default (what the competition pipeline consumes; LRAT costs 15–30 % solve time and 4–28× proof size). Extra proof-checked arms beyond the solver under test: `--checked-arm label=<cmd>`.
- Score: PAR-2 = mean over instances of (runtime if solved, else 2 × timeout). Report PAR-2 and #solved, split SAT/UNSAT, plus proof-check time and proof size.
- Also track microbenchmarks when relevant: propagations/sec, conflicts/sec, peak RSS, proof bytes/conflict (measured on Tier B).

## Reporting

Write reports to `benchmarks/reports/YYYY-MM-DD-<topic>.md`: hardware, git SHA, tier + list version, **allocation shape** (partition, cpus/mem per task, throttle — the rev-64 replacement for the jobs count), table (solver × {PAR-2, #solved, SAT/UNSAT split}), regressions vs previous report, and an honest interpretation. Milestone gates T1–T5 (plan §8.3) are judged only from these reports. Numbers pinned before 2026-07-23 are from the retired workstation — never compare against them directly.

Any verdict mismatch or proof rejection found during benchmarking is a **correctness bug**: stop the performance work, shrink it via the fuzz skill's procedure, and file it in the regression corpus first.
