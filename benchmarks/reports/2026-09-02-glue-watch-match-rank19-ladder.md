# Glue and Substitute watch-match repairs: Rank-19 ladder and decision

Date: 2026-09-02

## Outcome

Two Kissat-faithful repairs found by differential tracing on Rank 27 were
measured across the homogeneous Rank 1--40 gate and the B28+C5 gate. Neither
stack beats the accepted `f3ac562` binary in aggregate, and every stack that
carries the glue repair blows up Rank 19 chaotically. Both repairs are backed
out of `main` and accepted as scheduling-parity deviations
(`SP-GLUE-SHRINK-2`, `SP-SUBSTITUTE-WATCH-1` in
`docs/scheduling-parity-ledger.md` section 13.3). `main`'s Release binary
after the back-out is byte-identical to the `4701` pin
(`b33031547de2aa1445b6c01ec567acddf20d3f84ea002c715550990cd260bb79`), so the
accepted gate evidence transfers exactly. The full stack is retained on branch
`hold/glue-watch-match` (commit `8b3b573`).

## The repairs

- **Glue repair** (`14f9417`): Kissat clamps redundant glue to size-1 only in
  search-time OTFS, Sweep, and Vivify; its fused forward strengthen and root
  flush shrink in place without touching glue. Tactix applied the clamp at
  both sites. The repair converges Rank 27's conflict-113130/118697 Reduce
  retention.
- **Substitute watch-match** (`6f5d329` partial, `dbadf8e` full): Kissat's
  canonical Substitute ends with `kissat_watch_large_clauses`, re-appending
  every large watch in arena order; Tactix retained the lazy in-place watch
  pose. The full rebuild makes the Rank-19 conflict-0 binary-scan journal a
  complete word-for-word match (campaign `4741`).

## Measured ladder (solver wall seconds, single draws)

| Stack (binary) | Rank 1--40 total | Verdicts | R17 | R19 | R27 (conflicts) | R34 | R35 | B28+C5 |
|---|---|---|---|---|---|---|---|---|
| Kissat 4.0.4 (`1930`) | 7,675.2 | 40/40 | 121.8 | 739.9 | 260.0 | 344.0 | 322.1 | authority |
| `f3ac562` accepted (`4701`) | 9,597.0 | 40/40 | 41.2 | 251.5 | 496.5 (7,441,126) | 1,080.8 | 453.5 | accepted |
| + glue (`4713`) | 10,420.7 | 39/40 | 41.3 | timeout 1,200 | 432.2 (5,795,310) | 796.3 | 614.9 | -- |
| + glue + binary-first reorder (`4736`/`4737`) | 10,267.7 | 39/40 | 27.9 | timeout (1,974.6 unbounded, `4735`) | 428.8 (5,795,310) | 794.0 | 604.9 | 66/66, 0.575 |
| + glue + full watch-match (`4740`/`4742`) | not gated | -- | -- | timeout 2,700 (`4742`) | 5,533,826 conflicts (`4740`) | -- | -- | -- |
| attach + full watch-match, no glue, `1a705dbf` (`4744`/`4745`) | 10,054.8 | 40/40 | 186.1 | 1,010.4 (989.2 in `4743`) | 481.4 (7,441,126) | 585.8 | 759.9 | 66/66, 0.668 |

Campaign directories: `benchmarks/runs/20260902-4713-tierd-ranks1-40-glue`,
`20260902-4736-tierd-ranks1-40-reorder`, `20260902-4737-BC-reorder`,
`20260903-4743-tierd-rank19-noglue-solve`,
`20260903-4744-tierd-ranks1-40-noglue`, `20260903-4745-BC-noglue`; the
Rank-27 boundary and binary-scan campaigns are `4733`/`4734`/`4740`/`4741`.
B28+C5 ratios are paired PAR-2 over 33 instances with the proof and model
checkers nonblocking; all 66 solver verdicts matched expectations in both
`4737` and `4745`.

## Reading

- The glue repair is the confirmed sole owner of the Rank-19 blowup: with
  glue, Rank 19 never finishes under the 1,200 s gate regardless of the watch
  layout (timeout, 1,975 s, 2,700 s); without glue it finishes in 989--1,010 s
  but still far from the accepted 251.5 s. Rank 19 is a chaotic SAT instance
  (pinned Kissat itself spans 302.7 s to 739.9 s across draws).
- The watch-match without glue leaves Rank 27's trajectory untouched (same
  7,441,126 conflicts as `f3ac562`), so its Rank-27 gain existed only in
  combination with glue. Across the other ranks it is a reshuffle: Rank 35
  +306 s, Rank 19 +759 s, Rank 17 +145 s against Rank 34 -495 s, Rank 37
  -162 s, Rank 40 -63 s, Rank 31 -53 s; UNSAT ranks net +77 s.
- Both repairs remain reference-faithful. They are declined on measurement
  only, which is why they are ledger deviations with reopening triggers and
  not rule-off configurations or instance gates.

## Next: the systematic unit-cost gap on the huge ranks

Decomposing the accepted `4701` records against Kissat's `1930` profiles on
the zero-conflict ranks (both solvers finish without search; Kissat solves them
inside probing), seconds:

| Rank | Tactix wall | front-end (wall minus solve) | Substitute | outside inprocessing | Kissat wall | Kissat parse | Kissat substitute | Kissat lucky |
|---|---|---|---|---|---|---|---|---|
| 5 | 42.1 | 38.6 | 0.0 | 3.5 | 30.1 | 20.8 | 0.0 | 3.2 |
| 7 | 66.2 | 16.6 | 14.0 | 8.7 | 48.6 | 10.3 | 6.0 | 2.9 |
| 8 | 82.4 | 24.6 | 15.0 | 9.9 | 58.4 | 12.4 | 7.0 | 2.9 |
| 10 | 117.1 | 32.8 | 23.8 | 15.1 | 82.4 | 17.2 | 10.0 | 4.7 |
| 13 | 92.5 | 26.8 | 19.0 | 11.2 | 64.4 | 13.7 | 8.1 | 3.8 |
| 14 | 91.1 | 27.3 | 17.1 | 10.9 | 63.5 | 13.6 | 7.7 | 3.5 |

Congruence is at parity on every one of these (for example 36.8 s versus
37.4 s on Rank 10). The remaining +190 s is unit cost with identical work:
DIMACS parse and import (+79 s), canonical Substitute (+50 s, about 2.3x),
scaffolding outside the inprocessing slot at zero conflicts (+38 s), Sweep
(+7 s) and unattributed time (+16 s). Rank 9, whose search is nearly converged
(48,361 versus 48,149 conflicts), adds BVE at 29.7 s versus Kissat's 13.4 s.
These are deterministic targets that involve no scheduling change.
