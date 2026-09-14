# Graph-symmetry WV57 screen — 2026-09-14

Job `1689304` measured immutable graph-symmetry snapshot
`3bed3dd12240b99182a6688dbeb2ea09a95e67c7` on all 57 Satsuma-owned WV100
rows. It used `cpu-dense-hwulab-q`, eight logical CPUs, 32 GiB, one solver
thread, a 300-second solver timeout, and the combined typed program
orbitopal-columns → graph-symmetry → scoped fixpoint → DefaultPolicy. No
reference solver was rerun.

All inputs, DSR certificates, elaborated LSR, checker intermediates, and
captured streams used anonymous node-local files. All 57 final records report
formula/certificate/intermediate deletion with no pending cleanup. Peak solver
RSS was 69,164 KiB.

## Result

- 57/57 final records; nine checked UNSAT solves and 48 clean timeouts.
- Every solve emitted native DSR accepted by `dsr-trim`; the resulting LSR was
  accepted by pinned Trestle.
- The nine solves used 8,474 effective structured-SR transitions in total.

| WV100 rank | Family | Solve seconds | SR transitions |
|---:|---|---:|---:|
| 2 | PHP Sudoku | 0.117 | 103 |
| 3 | PHP Sudoku | 0.417 | 188 |
| 4 | clique colouring | 1.669 | 70 |
| 17 | channel routing | 14.140 | 928 |
| 24 | channel routing | 11.836 | 818 |
| 30 | channel routing | 31.071 | 1,303 |
| 32 | channel routing | 75.845 | 1,648 |
| 36 | channel routing | 35.177 | 1,273 |
| 38 | channel routing | 115.352 | 2,143 |

## Consequence

The general graph producer is sound and effective on several families but does
not reproduce most of Satsuma's transition. The earlier official-binary screen
classified 51/57 rows as structured row/row-column, five as Johnson, and one as
generic Schreier. The next general increment therefore targets structured
row/row-column action discovery and fixing, including typed clique-guided
column ordering and alternating root rounds. Johnson actions follow. This is a
mechanism classification, never an instance or family dispatch.
