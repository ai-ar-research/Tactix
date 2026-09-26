# AM469: latest-source Phase 1 gate launch

Date: 2026-09-26. These are submitted campaigns, not completed or accepted results.

The candidate solver source is commit `1d81d353acbef6fcb6e2b9400d2d3bbf148eab2d`. The source snapshot under `benchmarks/runs/20260926-am469-phase1-source-authority/source/` was made with `git archive` and all 205 production source files are hashed in its manifest. `cumulative.patch` records the solver-source delta from handoff `92d3fc73`. Two independent plain Release builds, `build/am466-perf-release/tactix` and `build/am468-independent-release/tactix`, are byte-identical at SHA-256 `b7962004aedab21bc613b3fab7f6c466e692a8ae6761b60ca9b41c7034458af4`; current CMake/source/headers/third-party files match the archived snapshot. No proof-shadow option is enabled.

After the AM468 preparer repair, both campaign manifests passed `fixed_runtime.verify_campaign` and `runtime_task.validate_runtime_pin_contract` before submission. They copy the five exact frozen AM369 DSR/DRAT checker pins from `benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/pins/checkers/`. Their native DSR route remains DSR→LSR→Trestle; SAT models are checked against the original CNF. Each job uses `cpu-dense-hwulab-q`, 12 CPUs, 32 GiB, a 1,200-second solver cap and a 16,000-second checker cap per row. Neither reference solver is rerun.

| Campaign | SLURM array | Cases | Bound reference | Required candidate solver total |
| --- | ---: | ---: | --- | ---: |
| `benchmarks/runs/20260926-am469-rank1-50/` | `1728672` | 50 | frozen Rank 1--50 Kissat reference | below 8,115.735 s |
| `benchmarks/runs/20260926-am469-b28c5/` | `1728693` | 33 | frozen B28+C5 Tactix reference | below 952.263 s |

Acceptance still requires all 83 rows to finish with independently checked original-CNF models or canonical proofs, the per-case resource limits, both aggregate time bounds, and the remaining N1--N4, fuzz and Debug gates. The latest-source AM465 Opus campaign and full Debug `1728137` are running independently. No Tier D launch follows from this preparatory checkpoint.

Later checkpoint: both AM469 arrays were stopped after a completed witness-free Rank UNSAT row showed the frozen v4 runtime used only DRAT/CakeLPR for that proof, despite this goal's canonical DSR requirement. These runs are diagnostic, not accepted gates. [AM470](2026-09-26-am470-fixed-gate-dual-proof.md) records the v5 dual-oracle correction and fresh gate preparation.
