# c1 runs canceled as superseded by c2 (2026-10-07)

The user confirmed: "yes cancel them".
c2 = c1.1 (`bfaf65491`) plus the startup-FastEliminate guard removal (`524b7b48b`).
It changes the solver binary, so c1 qualification evidence does not transfer.

- 1755927: c1 full44, canceled mid-run. Rows already written (147, 263, 401 and 630
  verified) are reference-only, not c2 closures.
- 1755860 and 1755862: the remaining c1 Rank1–50 and WV2–89 gate tasks.

The fuzz owner stops its own c1 legs. `queue-before.txt` and `sacct-after.txt`
are the snapshots.
