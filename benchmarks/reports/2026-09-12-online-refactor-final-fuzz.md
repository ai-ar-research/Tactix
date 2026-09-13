# Online-refactor final fuzz checkpoint — 2026-09-12

**Verdict: PASS.** There were no model, proof, differential, crash, assertion,
replay, determinism, interruption, memory-cap or negative-control failures.
One harness gap is disclosed below.

## Candidate and pins

- Source: HEAD `0060215a` plus working-tree diff SHA-256
  `4f21a6cf32f262e252528d5b1c1693d6b57e59c9fb0f2e84836ecdc442517982`.
  The diff was retained at `build/pin-online-final-4f21a6cf/working-tree.diff`.
  Rebuilding Release from that source reproduced the pin byte-for-byte.
- Release pin: `benchmarks/runs/20260912-2105-online-refactor-final-BC/pins/tactix`,
  SHA-256 `916eae4d7e87f5602649035ac15a8c2985e83deaec2250e338a47972b998e9bf`.
- Debug pin (assertions, invariants and `TACTIX_ENABLE_PROOF_SHADOW=ON`), built from
  the same source: `8ee90216b54fcb5ff445a574eac9c57a25232d3dea35fed6a069898960f97b29`.
- Checkers and oracle: drat-trim `f2cb7d45…`, cake_lpr `ab9dc6c4…`,
  Kissat 4.0.4 `3aa8e41f…`.
- Read-only harness: `benchmarks/runs/20260912-online-final-fuzz-4f21a6cf/`, with
  manifest `HARNESS_SHA256` (SHA-256 `d0f3c111…`). The pinned `fuzz.py` differs from
  `tools/fuzz/fuzz.py` in one place: the checker timeout is 300 s instead of 120 s.
  `audit.py` checkers also use 300 s, and a checker timeout counts as its own class,
  never as a pass.

## Slurm

Job `1685021` was one non-array job on `cpu-dense-hwulab-q`, with no node
constraint. It ran on node017 with 50 CPUs, 200000M and a 1 h limit. It
**COMPLETED 0:0** in 14:38, using 6:33:37 of total CPU and 2.64 GiB peak batch RSS.
All CNF, proof, trace and model scratch lived in the node-local directory
`/tmp/onlfinal-1685021-*`. A parent trap removed it on exit or any signal
(USR1 240 s before the limit, TERM, INT or HUP). The in-job audit afterward found
`scratch_exists=no`, and no `/tmp`, `/var/tmp` or `/dev/shm` entries owned by the
user had been created during the job. Every solver process ran under a 16 GiB RSS
watchdog, which killed nothing (0 CAPKILL).

## Results

| Leg | Cases | Checked verdicts | Timeouts (no oracle) | Failures |
|---|---:|---:|---:|---:|
| Saved seeds (Debug+Release audit) | 19 | all non-interrupted | — | 0 |
| Fresh audit, seeds 912190000–912191999 | 2000 | R 1740 (SAT 1068 / UNSAT 672), D 1761 | 259 Debug at 20 s | 0 |
| `--random-policy` DRAT, 912200000+ | 4000 | 3531 | 469 (11.7%) | 0 |
| `--random-policy` native LRAT, 912210000+ | 4000 | 3550 | 450 (11.2%) | 0 |
| DefaultPolicy DRAT, 912220000+ | 3000 | 2956 | 44 | 0 |
| DefaultPolicy native LRAT, 912230000+ | 3000 | 2954 | 46 | 0 |
| Regression corpus (4 policy/format legs) | 30 × 4 | — | — | 0 |

- **Saved queue-corruption seeds** 912150000+{25, 38, 233, 245, 400, 536, 562, 684,
  956, 1164} were run on their original CNFs. Each had a traced Debug run, Debug and
  Release replay, and SIGTERM at Release 0.5 s and Debug 1.0 s. None raised an
  assertion, including the former `heuristics_check` queue abort.
- **Saved interruption seeds** 912150000+{6, 28, 37, 54, 55, 71, 78, 93} got SIGTERM
  at 0.5 s and 3.0 s on both builds. Across the saved set, all 51 signalled runs exited cleanly within the
  10 s grace period, with a maximum latency of 181 ms.
- **Seed 912130865:** the DRAT proof (37,186,334 bytes) passed drat-trim in 161 s
  under the 300 s limit.
- **Replay:** 1740 of 1740 fresh traces replayed identically on Debug and on Release
  (same exit, proof bytes and output lines), including 449 traces with no program.
  All 11 of 11 saved traces also replayed identically.
- **Determinism:** the same seed in two processes gave identical results in 1761 of
  1761 runs. Seed+1 differed in 1760 of 1760. Debug and Release proofs were identical
  in 1740 of 1740.
- **Proofs and models:** in the fresh audit, drat-trim verified 940 proofs and
  cake_lpr verified 641 native LRAT proofs. The largest was 56,986,553 bytes; the
  slowest check took 32 s. 2557 independent model checks passed.
- **Interruption:** 260 Debug SIGTERMs at the time limit, 77 Release SIGTERMs at
  0.5 s and 39 Debug SIGTERMs at 1–5 s all ended cleanly: either `s UNKNOWN` with
  exit 0, or a checked verdict. The worst exit latency was 4.5 s, under the 10 s
  grace.
- **Effective transitions:** all 26 registry actions were applied, with 2,508 to
  395,949 applications each. There were 144,908 typed programs and 138,018
  effective program transitions; 1420 cases had at least one. No inprocessing rule,
  Walk or Lucky applied from above root. The 1007 `Lucky@search` post-states are
  all Lucky's own SAT or UNSAT outcomes.
- **Variety:**
  - Sequence and step repeats: `once`, `count` 1–4 and `fixpoint` 1–4.
  - Sequence lengths 1–8, forming 93,539 distinct rule orders.
  - All four granularities, including 23 rules seen at every granularity.
  - Bounded and unlimited budgets for all three work units.
  - All three rule-work modes and all nine material settings.
  - 66,083 distinct parameter tuples.
  - Decide phases split 33,055 positive and 33,153 negative, and Rephase ran at
    every granularity.
  - Explicit instances: 212,169 steps across 23 rules. 61,687 selected batches
    were not in ascending order, since the generator strides through the complete
    enumeration rather than taking a prefix.
- **Random-leg path rows:** `VariableCompaction` never fired. It is a path row, not
  a registry rule, and is known to be unreachable under random schedules. In the
  DefaultPolicy legs, zero counts mean rules the policy declined to schedule.

## Negative controls

Every control where it applied was rejected. None was accepted.

- Falsified models: 1076 of 1076 rejected.
- Nonempty DRAT proof with no conflict: 118 rejected.
- Real DRAT proof truncated to half its lemmas, where unit propagation guarantees
  no conflict: 87 rejected.
- LRAT with the empty-clause hints removed: 315 rejected.
- Real LRAT truncated before its empty clause: 308 rejected.
- Not applicable, and counted: UP-refutable formulas (239), input empty clause (2),
  proof over 8 MiB (1), and 37 truncation edge cases.
- `selftest.sh` passed: kissat and cadical ran clean. The lying-SAT and
  negated-model wrappers were flagged by the model oracle, and the truncated-proof
  wrapper by the proof oracle. The ddmin shrink worked, the duplicate-literal
  conversion skip was confirmed, and native LRAT on the Debug candidate passed.
- `test_fuzz_proof_oracles.py` (run inside `selftest.sh`) and `test_modelcheck.py`
  passed.

## Gap

The job's focused-test runner had a quoting defect (`shift 2` dropped the command).
As a result `test_random_online`, `test_random_policy` and `trace_replay.sh` did
**not** execute in this job (exit 125, or the binary ran with no arguments). Their
trace negative controls are therefore not part of this evidence: truncated trace,
unknown rule and malformed program. Replay, determinism and zero-program replay are
covered by the audit above. No follow-up job was launched.

## Retained artifacts

`benchmarks/runs/20260912-online-final-fuzz-4f21a6cf/` holds the harness, pins, the
19 saved CNFs, `slurm-1685021.log` and `out-1685021/`. The output directory is
156 KiB: summaries, logs and `nonterminating.json` seed lists, with no formulas or
proofs. The pin object trees and source copy were removed. The diff and build logs
remain in `build/pin-online-final-4f21a6cf/`.
