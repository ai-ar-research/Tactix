# Known non-terminating fuzz cases (captured 2026-07-20)

Recovered from 6 orphaned `tactix --random-policy --lrat` processes that ran
**13.5 h at 100 % CPU** (PPID 1 — the fuzz parent was killed abruptly and the
children reparented to init and spun forever). Root cause of the *orphaning* is
fixed: `tools/fuzz/fuzz.py` and `tools/bench/run.py` now set a kernel-enforced
`RLIMIT_CPU` backstop (`preexec_fn`) so no child can outlive its budget even if
the parent dies. These CNFs are kept as repros for the *underlying* issues; they
are NOT in `corpus/regressions/` (they don't pass yet).

Two distinct issues (all cases are tiny, ≤ 89 vars / ≤ 2057 clauses):

- `hang-{59957,60003,60005,60006,60033}.cnf` — **random-policy livelock/non-term**:
  each solves *instantly* under DefaultPolicy (SAT/UNSAT < 1 s) but does not
  terminate under `--random-policy`. An N1 robustness concern (a valid policy
  schedule that fails to make progress — subsume rescheduled thousands of rounds,
  0 subsumed). Not a soundness bug; DefaultPolicy unaffected. → task.
- `hang-60019.cnf` — **genuinely hard** (89 vars, clause/var ≈ 23). Kissat also
  exceeds 25 s (1.3 M conflicts; we reach 903 k — comparable). Not our bug; a hard
  tiny UNSAT the generator produced. Kept as a stress instance.
