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
| C | ~150 SC instances, Kissat ≤ 300 s | 3000 s | weekly; before merging heuristic/scheduling changes | few CPU-h |
| D | Full SC 2024 + SC 2025 main tracks (400 each) | 5000 s, 16 GB cap | milestone gates (plan §9) and external claims only | days |

Hard rules:
- **Never tune heuristic defaults on Tier A** (random 3-SAT rewards the wrong heuristics). Tiers A–B answer "correct and not embarrassingly slow"; only C–D answer "competitive".
- Tier lists are frozen, versioned files in `benchmarks/tiers/*.list` (instance name + expected status + reference runtime). Changing a list requires a commit explaining why — never silently.
- Instances live in `benchmarks/cache/` (untracked); `tools/bench/fetch.py` downloads by hash from the Global Benchmark Database (gbd.iti.kit.edu) / competition mirrors.

## Building/refreshing Tier B or C lists

1. Run Kissat 4.0.4 (`../kissat/build/kissat` or wherever built) over candidate SC main-track instances with the tier's reference timeout, one instance at a time, wall-clock measured.
2. Select instances in the tier's reference band (Tier B: 5–30 s — the lower bound keeps trivial instances out; Tier C: ≤ 300 s), stratify ~50% SAT / 50% UNSAT, cap instances per benchmark family (Tier B: 2; Tier C: 3).
3. Record: instance, status (from competition results or Kissat), Kissat runtime, hardware string.

## Running an evaluation

- Environment (plan §8.1): a single local workstation — no HPC/StarExec. The runner may execute up to **8 single-threaded solver jobs in parallel** (`--jobs N`, N ≤ 8); each solver run itself is strictly sequential. Record the concurrency level in every report and only compare numbers taken at the same concurrency; drop to ~4 jobs for Tier C/D gate runs where timing precision matters.
- **Sequencing rule (plan §8.1 rev 18): do not evaluate Tier B while any
  Tier A instance is unsolved within its 30 s budget** — clear Tier A
  completely first.
- Runner: `tools/bench/run.py --tier B --solver ./build/tactix [--baseline kissat] [--jobs 8]` (if the runner doesn't exist yet, build it to this interface). It must: record wall time + exit code + verdict, verify verdict against the expected status in the list, and **check the proof for every UNSAT answer** (drat-trim; a rejected proof counts as unsolved, competition-style).
- Score: PAR-2 = mean over instances of (runtime if solved, else 2 × timeout). Report PAR-2 and #solved, split SAT/UNSAT, plus proof-check time and proof size.
- Also track microbenchmarks when relevant: propagations/sec, conflicts/sec, peak RSS, proof bytes/conflict (measured on Tier B).

## Reporting

Write reports to `benchmarks/reports/YYYY-MM-DD-<topic>.md`: hardware, git SHA, tier + list version, **jobs concurrency**, table (solver × {PAR-2, #solved, SAT/UNSAT split}), regressions vs previous report, and an honest interpretation. Milestone gates T1–T5 (plan §8.3) are judged only from these reports.

Any verdict mismatch or proof rejection found during benchmarking is a **correctness bug**: stop the performance work, shrink it via the fuzz skill's procedure, and file it in the regression corpus first.
