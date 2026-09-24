# Near-unit Walk score base

Original AM345 bounded owner handoff: seed `345501271`, 97 variables and
562 clauses, SHA256 `f4c29f97d977ba553e3a8713070b017edb22ed58ad9c9d315315a277532192f6`.
Preserve the original bytes, including the legal noncanonical DIMACS layout.
Run `tactix --dsr --random-policy=345501271 case.cnf proof.dsr` under SLURM.
The AM341 Debug pin reaches 33.5 GB RSS and is killed in about 11 seconds.

Main GDB job 1716780 identifies the growing allocation as Walk's score table,
not the last fully printed rule in the trace. A legal fitted curve can have
base just below 1, so constructing scores through underflow is unbounded in
practice. Scores above the maximum literal occurrence-list length can never
be queried. The repair stores only the reachable prefix, retaining the exact
multiplication recurrence, underflow/fixed-point tail, API and budgets.

`test_walk::test_near_unit_interpolated_base` supplies a causal three-variable
fixture: average 9/4 and curve `(0,136), (3000,1288)` interpolate to CB one ULP
above 1. It tests zero, one and canonical step allowances. Linked to the old
library it aborts with `std::bad_alloc` under a diagnostic 256-MiB address-space
limit; no memory limit or parameter restriction is part of the repair.
