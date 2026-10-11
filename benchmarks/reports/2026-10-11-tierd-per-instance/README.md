# Tier D per-instance results: Tactix and the reference solvers

All seven arms of the [final Tier D comparison](../2026-10-11-tierd-fastel-final/README.md), one
row per instance and solver (1,166 × 7 = 8,162 rows). `build.py` regenerates every file from the
campaign records; its docstring states the merge rule. The totals reproduce the final report.

| File | Shape |
|---|---|
| `results.csv` | long: one row per (instance, solver), every column below |
| `results.json` | the same rows as a JSON array |
| `wide.csv` | one row per instance; per solver: status, solve s, proof-check s, solve peak MB, PAR-2 s |

| Solver | Solved | Verified | Verified PAR-2 sum |
|---|---|---|---|
| tactix-0f7c10174 | 1,006 | 999 | 2,110,652 |
| tactix-no-mab (4 h checks) | 1,005 | 986 | 2,230,498 |
| satsuma-kissat | 965 | 946 | 2,634,766 |
| kissat-sc2024 | 890 | 883 | 3,299,728 |
| kissat-4.0.4 | 881 | 872 | 3,371,597 |
| ae-kissat-mab | 875 | 863 | 3,428,275 |
| lymphosat | 947 | 852 | 3,501,270 |

## Columns

- `status`: `verified`, `unverified` (UNSAT, proof not accepted), `model-ok-nonprotocol` (correct
  model, non-competition output; LymphoSAT 68), `timeout`, `solver-memout`, `no-answer (exit N)`.
- `solve_wall_s`, `solve_peak_rss_mb`, `solve_exit_code`, `solve_timed_out`: solver run (5,000 s,
  32 GB, 4 CPUs). Peak memory is recorded only for runs that ended before the time limit.
- `proof_pipeline`, `proof_check_result`, `proof_check_wall_s`, `proof_check_peak_rss_mb` (max over
  stages), `proof_check_stages` (`stage:result:seconds`), `proof_check_failure` (`check-timeout`,
  `check-memout`, `check-failed (stage exit N)`), `certificate_bytes`.
- `model_check_result`, `model_check_wall_s`, `model_protocol_ok`: SAT answers.
- `source_campaign`: the record used (under `benchmarks/`); `recheck_campaign` / `recheck_status`:
  the 64 GB / 8 h recheck attempt, when one exists (substituted only when it verified).
- `par2_s`: solve time if verified, else 10,000.
