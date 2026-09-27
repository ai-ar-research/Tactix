# AM516: immediate-MAB CDCL screen is terminal, 2/4 checked recoveries

Array `1732016_{50,52,53,57}` is terminal on the unchanged current source/binary and one uniform `--mab --auto-proof` option. SLURM `sacct` reports tasks 50/57 `COMPLETED 0:0` and 52/53 `FAILED 1:0`; each state matches its frozen result JSON under `benchmarks/runs/20260927-am515-wv2-cdcl-immediate-mab-diagnostic/results/`. The diagnostic manifest SHA-256 is `052519032c3e167f99d919f2de8d3ba11cea6c00659040b41b9dd1bf98fa07c3`, with all 17 pin hashes and four input hashes validated before launch. It is a causal screen, not an acceptance gate or a production change.

| Task | Row | Shipped delayed-MAB default | Immediate MAB | Independent result |
|---|---|---|---|---|
| 50 | `snw_16_9_preOpt` | UNKNOWN at 600.005 s | SAT in 70.474 s | Original-CNF model verified; cleanup complete |
| 52 | `Circuit_multiplier24` | UNKNOWN at 600.004 s | UNKNOWN at 600.005 s | No model; timed out |
| 53 | `ntil-90d-39` | UNKNOWN at 600.005 s | UNKNOWN at 600.005 s | No model; timed out |
| 57 | `lockchart...L19` | UNKNOWN at 600.005 s | SAT in 195.117 s | Original-CNF model verified; cleanup complete |

The policy mechanism is explicit in current source: delayed MAB normally admits EVSIDS/CHB after `active_variables × ceil(log2(active_variables))` conflicts, admits at search entry only after a completed material-minority FastEliminate pass, and suppresses later activation entirely if structured SR applied. The immediate-MAB winning `snw` run applied zero SR actions and solved in 69,706 conflicts; it bypassed a potentially much larger default delay. The winning `lockchart` run applied 254,450 SR actions, so the shipped default would suppress MAB admission; immediate MAB solved in 1,559,481 conflicts. These are mechanism-consistent observations, not matched estimates of the default's hidden conflict trajectory: timed-out default records have no final solver counters. The old scheduling rationale was to avoid stacking incompatible startup and search trajectory changes; this two-row gain is not enough to discard that safeguard globally.

Two of four CDCL misses remain, and the full WV2 gate still has ten solve timeouts. A shared activation policy must be assessed on representative non-miss rows and the retained Rank/B28+C5/WV100 gates before promotion; no row name, rank, or input hash may choose an arm. This screen supports further policy work, not a blanket `--mab` default or any goal pass. Independent WV2 large-proof checkers and WV100 `ncc` remain active.
