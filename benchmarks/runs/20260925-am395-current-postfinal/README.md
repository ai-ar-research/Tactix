# AM395: current-source late-addition diagnostic

The exact AM380 public C-API two-clause reproducer is linked against current
Release source objects (commit `83ee5124`, solver source identical to AM393
`c58c1f68`). The SLURM job runs the original control plus each input-finalizing
call. `MODEL-BAD` is an independently checked original-clause violation, not
a solver return-code interpretation. This is diagnosis only: no API contract
choice, solver edit, or N1/N2 acceptance follows. `sha256.txt` pins the source,
driver and solver binary; `out/` retains individual results.

Job `1720247` completed in under one second. The no-query control reports
`MODEL-OK` with `1 -2 3`; `cand`, `status`, `applicable`, and `instances`
each report `MODEL-BAD` with `-1 -2 3`, falsifying the late `(1 ∨ 2)` clause.
The wrapper intentionally returns zero after recording all five driver exit
codes; four `exit=1` rows are confirmed solver failures, not a passing job.
