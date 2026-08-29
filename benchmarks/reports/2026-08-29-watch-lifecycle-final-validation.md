# Disconnected-watch lifecycle repair: final validation

## Decision

Accept the general disconnected-large-watch lifecycle repair together with the
in-place clause and Congruence import-order repairs. The exact production
binary solves all 40 Tier-D ranks with the expected verdict. It uses one
homogeneous default strategy and no formula, density, size, classification,
benchmark, rank, literal, identity, or conflict selector.

The final candidate's PAR-2 is 11,215.788 seconds. This is 0.9945x the exact
pre-sanitizer candidate's 11,277.844 seconds (0.55% lower), and 0.9043x the
earlier 39/40 accepted baseline's 12,402.620 seconds (9.57% lower while also
recovering Rank 19). Against frozen Kissat, PAR-2 is 1.465x and the all-rank
geometric mean is 1.431x.

The complete per-instance runtime and peak process-tree memory table is frozen
in
`benchmarks/runs/20260829-1853-tierd-ranks1-40-watch-lifecycle/comparison-solver-final.md`.
It compares the exact candidate with the immediate 40/40 predecessor and
frozen Kissat without rerunning either comparator.

## Mechanism

Pinned Kissat carries a global `watching` state. Vivify disconnects every large
watch before root simplification and reconnects the full live population only
after scheduling. Tactix's first in-place rewrite implementation assumed large
watches were always connected, so a root-shortened clause was transiently
unwatched and reinserted during the disconnected interval. Release assertions
hid the invalid lifecycle; ASAN/Debug exposed it.

Tactix now carries `large_watches_connected`, keeps binary watches live in both
states, and makes every surviving in-place large rewrite respect the global
state. Vivify rebuilds the complete large-watch population before root-unit
propagation. The direct kernel regression flushes large watches, rewrites a
large clause in place, proves no large entry was inserted, rebuilds, and checks
the restored pair and full invariants. No consumer outside the existing kernel
and Vivify lifecycle was added.

## Validation

- Affected ASAN/Debug gates: 7/7 pass.
- Full Release suite after the repair: 76/76 pass.
- Uniform Slurm array `1656053`: 40/40 expected solver verdicts.
- Rank 19: SAT in 769.186 seconds, versus 775.077 immediately before the
  lifecycle repair and 729.272 for Kissat.
- Rank 36: SAT in 385.461 seconds, versus 387.570 immediately before the
  lifecycle repair and 368.940 for Kissat.
- Immediate-predecessor geometric mean: 0.9897x; the repair is slightly faster
  overall rather than merely tolerated for fidelity.

At the solver-completion checkpoint, 30/40 final checked result records were
present. Independent proof checking continues for UNSAT Ranks 22, 27, 31, 32,
33, 35, 37, 38, 39, and 40. The preceding campaign is 39/40 checked with only
Rank 22 still running. These checkers are retained; provisional UNSAT solver
timing does not wait for them, and no certificate or scratch cleanup claim is
made until each task writes its final record.

## Revisit outcome and next leads

The revisit removed completed Substitute/root-owner diagnostics and their
Kissat overlay injections, removed dead Vivify position inheritance for clauses
that now genuinely remain in place, narrowed the rule-boundary allowlist, and
withdrew content-profile/preset recommendations from the strategy survey.
Stable invariants live in the implementation notes and parity ledger; causal
chronology and measurements remain in the reports/campaigns.

All ranks are now solved. The remaining largest general runtime gaps versus
Kissat are Rank 15 (6.43x), Rank 31 (2.80x), Rank 20 (2.47x), Rank 4 (2.25x),
and Rank 38 (2.16x). Future work should restart from the earliest common-state
divergence in reconstruction, propagation order, inprocessing output, clause
tiers, heuristic/heap state, or search trajectory. It must not restore
CaDiCaL ordering or introduce a formula/rank gate.
