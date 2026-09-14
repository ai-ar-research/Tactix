# Symmetry follow-up screens — 2026-09-14

Two focused negative controls followed the checked 9/57 graph-symmetry screen.
Both used `cpu-dense-hwulab-q`, eight logical CPUs, 32 GiB, one solver thread,
a 300-second solver timeout, native DSR, and anonymous `O_TMPFILE` scratch. No
reference solver was rerun.

## All-clause graph scope

Job `1689361` reran the 48 residual Satsuma-owned WV100 rows with the graph
producer's typed clause scope changed from unique-literal clauses to all live
clauses. The immutable binary SHA-256 was
`0e5625f0d52b8fa3cc30f75a1f4189447e29ac09dc3e0312251bf1f9b44ad923`;
the manifest SHA-256 was
`6b2444f1f49976359df0aeb3d0415c99c94a3eea85888a032e77124c68024f40`.

All 48 tasks reached the solver timeout and added no checked solve. Every final
record reports formula, certificate, and derived-artifact deletion, with no
pending cleanup. The largest sampled process VM peak was 1,199,140 KiB. This
rejects all-clause scope as the next standalone scheduling change under this
budget; it does not remove that legal typed scope from X7.

## Connected-column orbitopal alignment

Job `1689409` tested a general detector repair on the six remaining
clique-colouring rows. The repair composes alignments through the complete
connected mutual-exclusion compatibility graph instead of requiring every
column to align directly with one seed. It retains the exact whole-formula
automorphism check. The immutable binary SHA-256 was
`93a7c209891484367454db0e99a7ca190826aaa4c48bdd49b66963dff91d9b37`;
the manifest SHA-256 was
`2e5d681f3b4b23bdd0aab04dbc6d3cc6ec57a3dbbc7b60aa93abf7abc31da6db`.

All six tasks reached the solver timeout and added no checked solve. Every final
record reports complete cleanup, with no pending artifact. The repair remains
supported by a causal synthetic regression for a three-column compatibility
path, but this screen shows that connected alignment alone does not close the
six rows.

## Consequence

The next bounded experiment is the independently implemented Satsuma mechanism
that orders structured columns by an exact maximum clique in their clause
co-occurrence graph. Its order, vertex limit, search-work limit, interruption,
random generation, and replay must be typed and validated before the screen.
The retained canonical order remains the reference default.
