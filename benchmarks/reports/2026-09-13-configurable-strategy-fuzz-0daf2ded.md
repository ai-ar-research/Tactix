# Configurable-strategy fuzz checkpoint — candidate `0daf2ded` (job 1686458)

**Verdict: FAIL.** No model, proof, differential, determinism, or replay oracle
rejected anything. But Walk runs chosen by random typed programs never
terminate and allocate without bound. That produced 16 GiB memory-cap kills,
missed SIGTERM deadlines and, through memory pressure, cgroup OOM kills
everywhere else in the job. The checkpoint cannot close.

Campaign: `benchmarks/runs/20260913-configurable-strategy-fuzz-0daf2ded`.

## Pinning and cleanup

- One non-array job with 50 CPUs, 200000M memory and a 1 h limit, on node017.
  Slurm state **OUT_OF_MEMORY** (0:125) after 35:58. The batch step's MaxRSS
  was 195 GiB, and Slurm recorded 353 `oom_kill` events. `work.sh` itself
  finished on its own (`work exit=0`, 2157 s), before its own timeout.
- The 117-entry harness manifest verified before the work started. The pinned
  Release `tactix-release` SHA-256 is `3d0e0eec…4385`, the same as the closure
  report's Release pin.
- Cleanup: the scratch directory was removed (`scratch_exists=no`) and the
  node-local residue audit found `residue_entries=0`.

## Results by surface

| Surface | Result |
|---|---|
| Focused tests: 11 binaries plus trace_replay and micro_proofs, both builds | 26/26 exit 0, no FAIL lines |
| `selftest.sh` (known-bad model/proof/conversion controls) | SELFTEST OK |
| `test_modelcheck.py` | exit 0 |
| Default policy fuzz, DRAT and LRAT, 3000 cases each | 0 failures; 49 and 40 non-terminating at 10 s |
| Corpus (30 entries per leg) | 10–12 `crash exit -9` per leg, none CAPKILL-logged (see below) |
| Random policy fuzz, DRAT and LRAT, 4000 each | 17 and 15 `crash -9`, none CAPKILL-logged; 47.4% and 45.0% non-terminating |
| Fresh typed programs (`audit.py fresh`, 915 of 2000 done, 1085 skipped at deadline) | 443 failing cases |
| Square Factor corpus (246 of 600 done) | 201 failing cases |

Oracle counts from the fresh audit. None of these failed:
- 561 models accepted.
- Proofs: 300 DRAT and 236 LRAT verified.
- Replay: 463 of 463 identical in Debug and 463 of 463 in Release.
- Determinism: 463 of 463 identical, and all 341 `seed+1` comparisons differed.
- Cross-build: 463 of 463 identical.
- Negative controls:
  - 230 corrupted models rejected.
  - LRAT: 116 empty-hint and 110 truncated proofs rejected.
  - DRAT: 4 constructed no-conflict and 1 truncated proof rejected.
  - Inapplicable mutations were counted separately, not accepted.

The square audit found no oracle failure either: 73 models and 21 proofs.

Every fresh and square failure is one of `memory-cap`, `interrupt-*` (no exit
within 10 s of SIGTERM, or exit -9 afterwards), `crash exit=-9`, or
`release-run exit=-9`.

## Reachability and effective-transition coverage

- Fresh and square both report `missing required reachability []`.
- Every Reduce, Walk, Reorder and `Factor:exact_grid_pass` setting was reached,
  in 45 to 400 effective cases.
- Square factorings came only from exact passes: 7 cases, 39 factorings.
- Fresh: 5983 effective program transitions across 563 cases; `rules_not_applied`
  and `legal_rule_level_missing` are both empty.
- The default fuzz legs never fired Probe, HyperBinaryRes, BCE, CCE,
  Instantiate, HyperTernaryRes or SubsumeForward. The random legs reached all of
  them. `root_only_rules_at_search_level_poststate` lists `Lucky@search`: that
  is the post-state level label and was not investigated further.

This coverage is incomplete. A large share of the typed-program space was never
checked by an oracle: 450 of 915 fresh cases timed out, 1085 were skipped at the
deadline, and about 46% of random-policy cases did not terminate.

## Actionable finding: non-terminating Walk score table

`src/policies/ops/walk/walk.cpp:169-170` builds the ProbSAT table as
`for (epsilon = 1; next; next = epsilon * base) table.push_back(...)` with
`base = 1 / cb`. The loop only ends once `next` underflows to 0, which requires
`cb > 1`.

- The random typed-program generator draws `even_break_base_milli` uniformly
  from [1, 2000] (`src/strategy/random_online.cpp:224`).
- It also draws the curve's break bases upward from 1.
- `WalkParameters::valid()` (`src/strategy/rule_parameters.cpp:38`) only rejects
  zero.

So any break base ≤ 1000 milli (cb ≤ 1.0) passes validation. On such a Walk
call the table grows until the 16 GiB cap or the cgroup OOM killer ends the
process, and no termination check runs. Near-1 values above 1.0 would still
finish, but with very large tables.

Evidence:
- 440 of 443 fresh failures contain a Walk step. The 3 without one are isolated
  interrupt-latency cases.
- No zero-program case hit the memory cap.
- Failing traces carry sub-1000 bases, for example `walk(…,232,…)` in seed
  913100012 and `walk(…,25,…)` in 913100039.
- Default-policy legs, which use the reference 2000 base and the reference
  curve, have 0 failures.
- Median Release peak RSS on square cases was 16.78 GiB. In fresh cases it was
  28 MiB, but the p99 sat at the cap.

The corpus and random-leg `-9` crashes have no CAPKILL record. They line up
with the batch-wide memory exhaustion that these runaway Walk processes caused
(concurrent solvers each growing toward 16 GiB inside a 200 GiB allocation), so
they are secondary symptoms. They are not independent corpus regressions. Some
missed SIGTERM deadlines are also consistent with memory pressure. Whether a
separate SIGTERM-latency defect exists cannot be decided from this job.

Failure artifacts (original CNF, trace, `repro.json`) are preserved under
`out-1686458/fresh/failures/`, `out-1686458/square/`, and
`out-1686458/fuzz-random-{drat,lrat}/`. Nothing was shrunk or added to the
corpus, and no job was submitted or altered.
