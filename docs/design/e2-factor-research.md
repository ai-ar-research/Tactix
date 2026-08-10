# E2 Factor reference study — frozen compact record

**Status:** historical source study. It does not own current semantics, defaults,
or work items.

## Reference conclusion

Kissat's Factor is BVA over a rectangular clause grid
`{{f_i} ∪ R_j}`, not merely a binary-clause rule. The matcher grows a factor
chain, intersects remainder sets, and accepts a positive reduction under the
current elimination bound. Shipped `factorsize=5` makes clause remainders part
of the default behavior. CaDiCaL provides complementary BVA evidence but Kissat
is the DefaultPolicy tie-break.

## Durable implications

- The abstract E2 rule must quantify over clause remainders.
- Divider → quotient → grid-deletion proof order is load-bearing.
- Candidate order, structural scoring, delay, effort, and the BVE-coupled bound
  are policy concerns and belong in the scheduling ledger.
- The earlier binary-only and pre-implementation status statements are stale.
  The implemented remainder interning and size-five scope are described in
  `docs/implementation-notes.md`.

Measurements and exact source-line evidence remain in the dated Factor reports.
No live task is owned by this snapshot.
