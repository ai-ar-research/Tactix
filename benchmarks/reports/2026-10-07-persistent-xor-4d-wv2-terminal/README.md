# XOR-enabled 4d WV2–89 terminal rejection — 2026-10-07

All 89 array tasks in job `1752443` are terminal. The maintained scorer rejects candidate `4d7069a4c`: 75 independently verified results, two allowed bounded proof censors, and 12 solver timeouts. Lost retained solves are task 37 (600.005s vs 482.234s baseline), 44 (600.005 vs 298.374) and 49 (600.005 vs 585.823). Exact scorer output, manifest and the relevant raw result rows are frozen here.

Task 44 and 49 admit zero equations but report 372.352s and 71.949s affine overhead. Task 37 admits 27 equations, runs 2.82 million search services, publishes no reasons, reports 358.268s affine overhead and 27 usefulness-disable cycles. The last case requires a nonempty-session scheduling repair in addition to empty-census skipping. No fix success or promotion is established by this diagnosis.

The task 14/51 proof outcomes reach the shared 1,800-second regression checking deadline without a reported proof rejection and are allowed by the bounded comparison contract; they are not independently verified closures. Rank 569 is a separate unresolved live experiment. All four fresh 4d regression sets now reject performance; prior acceptance on a different candidate does not transfer.
