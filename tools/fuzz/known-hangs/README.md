# Known non-terminating fuzz cases

CNFs recovered from orphaned `tactix --random-policy --lrat` processes: the fuzz
parent was killed abruptly, the children were reparented to init and spun
forever. The *orphaning* is fixed — `tools/fuzz/fuzz.py` and
`tools/bench/run.py` set a kernel-enforced `RLIMIT_CPU` backstop
(`preexec_fn`), so no child can outlive its budget even if the parent dies.
These CNFs are kept as repros for the *underlying* non-termination; they are
NOT in `corpus/regressions/` (they do not pass yet).

Two distinct issues (all cases are tiny, ≤ 89 vars / ≤ 2057 clauses):

- `hang-{59957,60003,60005,60006,60033}.cnf` — random-policy
  livelock/non-termination: each solves instantly under DefaultPolicy but does
  not terminate under `--random-policy`. An N1 robustness concern (a valid
  policy schedule that fails to make progress — subsume rescheduled thousands of
  rounds for zero subsumptions). Not a soundness bug; DefaultPolicy unaffected.
- `hang-60019.cnf` — genuinely hard (89 vars, clause/var ≈ 23); Kissat is
  comparably slow on it. Not our bug, kept as a stress instance.
