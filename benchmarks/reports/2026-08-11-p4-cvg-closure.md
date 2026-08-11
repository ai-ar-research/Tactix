# P4-CVG closure — 2026-08-11

## Outcome

P4-CVG is closed on the release binary
`build/pin-p4cvg-final-v3/tactix`, SHA-256
`c3ecdf38d6f7cebb45f378db50c36d80027a49d508e225ecbd89c07866528c14`.
The fixed three-fixture P4A set and five-fixture P4B set ran sequentially in the
prescribed order with a 300-second per-solver limit. Every Tactix and Kissat
answer passed the wrapper's proof or model validation.

| # | Job | Fixture | Verdict | Tactix | Kissat | Validation |
|---:|---:|---|---|---:|---:|---|
| 1 | 1607665 | `tr-n300-s055` | UNSAT | 27.83s | 22.65s | both verified |
| 2 | 1607666 | `tr-n260-s002` | UNSAT | 4.90s | 3.84s | both verified |
| 3 | 1607667 | `tr-n280-s052` | UNSAT | 16.42s | 10.39s | both verified |
| 4 | 1607668 | `mod4block_2vars_10gates_u2_autoenc-sc2009` | SAT | 163.96s | 18.50s | both verified |
| 5 | 1607669 | `mp1-klieber2017s-0500-023-t12` | SAT | 195.08s | 12.20s | both verified |
| 6 | 1607670 | `or_randxor_k3_n540_m540` | UNSAT | 14.41s | 12.36s | both verified |
| 7 | 1607671 | `unsat-set-b-fclqcolor-10-07-09` | UNSAT | 39.38s | 16.36s | both verified |
| 8 | 1607672 | `veer_axi_yosyshq_appnote_123_veer_axi-p23_transition` | UNSAT | 101.62s | 2.98s | both verified |

The canonical metadata is under
`benchmarks/runs/20260811-p4cvg-final-v3-01-n300` through
`benchmarks/runs/20260811-p4cvg-final-v3-08-veer-axi`.

## Accepted repairs

1. Randec admission now distinguishes a compact startup universe at level one
   from a large reused level-one restart prefix. This restores the 30-second
   `tr-n300-s055` cumulative guard without changing the large-prefix rule.
2. Vivify implements the complete precedence-ordered deduction terminal state
   machine: reason subsumption/promotion, strict shrink, independently implied
   deletion, asymmetric deletion, and instantiation/no-op. The former `mp1`
   conflict-2505 trace frontier is closed.
3. The eager Substitute follow-up consumes freshly emitted Sweep equivalences
   directly with union-find and locates connected large clauses through the
   dense occurrence index. The final Substitute slot still performs complete
   whole-BIG SCC discovery.
4. `factor_active_ceiling=100000` is a named serialized scheduler parameter.
   Once crossed, the large-formula classification is latched until solve reset;
   preprocessing cannot shrink below the ceiling and re-admit Factor's first
   700M-tick dose.

## Veer failure and closure

The first current-population-only ceiling pin timed out at 300.12s in job
`1607664`. The formula initially crossed the ceiling, so Factor was declined,
but later BVE/Substitute shrinkage reduced `active_variables` below it and
re-admitted the expensive first dose. A no-Factor diagnostic had already shown
the remaining path was viable: job `1607656` verified UNSAT in 115.81s. Latching
the large classification produces the final proof-verified 101.62s result in
job `1607672`. A focused scheduler test models the above-ceiling to
below-ceiling transition and proves Factor remains declined until reset.

## Correctness and limits

The release build passes all 69 configured CTest entries; the two optional
external proof-tool tests are skipped by configuration, with no failures.
Focused tests cover every new Vivify terminal arm, the Substitute bridge,
compact-versus-reused randec level one, and the Factor solve latch.

The previously held sparse-GC candidate is not in this pin. At the user's
direction, tracked corpus cases 004/005 were not inspected or executed because
that path triggers a security warning. They remain separately deferred and are
not claimed fixed by this report.

## Next action

Complete N4 verification against the reconciled scheduling ledger. After N4,
the phase order is Tier D shakedown/campaign and then T3 evaluation.
