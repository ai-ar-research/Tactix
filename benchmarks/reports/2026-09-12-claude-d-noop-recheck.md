# D-NOOP closure: Claude second checkpoint (2026-09-12)

Scope: the uncommitted D-NOOP diff on top of `bb03aea2`. For this review,
`git diff | sha256sum` =
`c5d2bd04a16bb7a9abe7c2cb1505e06cb14ce60546831b8d8f840746cb985e30`.
The first checkpoint reviewed `3831ef5b…90244bc3`. The diff touches 17 files:
kernel, DefaultPolicy, Reduce, Restart, six tests, and five docs plus the plan
and PROGRESS. It follows up on the
[first checkpoint](2026-09-12-claude-d-noop-checkpoint.md) and the
[state-boundary follow-up](2026-09-12-n1-state-boundary-followup.md).

This was a bounded local read-only check in the session shell. It was not a
SLURM submission and is not tied to any allocation. I made no source, test,
plan, or build edits. I did not run `ctest`, so nothing was written into the
pins. Temporary files went to `mktemp -d` and were removed.

## 1. Repair for the first BLOCK finding

The earlier concern was that a full Restart could leave a live conflict whose
literals are all level 0 while `!inconsistent`, so a later root rewrite could
retire its antecedent.

In `restart.cpp`, `restart_backtrack_to` now ends in
`refresh_recorded_conflict`. It checks every conflict literal (binary through
`conflict_bin`, large through the arena) against current values:
- If the conflict still holds and `level == 0`, it calls `analyze_apply(k)`.
- If it still holds above root, it returns `Ok` with the conflict kept.
- Otherwise it clears `conflict_valid`, `conflict_ref` and `conflict_bin`.

**Soundness of calling `analyze_apply` here.**
- `analyze_guard` holds: `conflict-valid` is true, and `consistent` is inherited
  from `restart_guard`.
- At level 0 every conflict literal has `var_level == 0`, so
  `analyze_apply_internal` goes straight to the `alevel == 0` branch
  (`analyze.cpp:1214`). That branch sets `inconsistent`, clears
  `conflict_valid`, and emits the empty clause. In LRAT it chains the root unit
  IDs followed by the conflict ID; in DRAT it uses `add_empty`. It returns
  `Unsat` before any trail, seen-flag, or pending mutation.
- This is the same branch ConflictAnalysis already takes for a chronological
  conflict whose literals are all root. The proof obligations are therefore
  unchanged: root unit IDs for level-0 implications were already required.
- `kernel_backtrack` does not unassign level-0 literals, so the values the
  branch reads are the same ones that made the conflict hold.

**No other path reaches `conflict_valid && level == 0 && !inconsistent`.**
- Every other `kernel_backtrack` caller either clears the conflict (Lucky
  `abandon_probe` and `commit_failed_literal`, analyze's failed-literal path) or
  runs only under a `no-conflict` / root guard.
- Root Propagate conflicts still set `inconsistent`.
- The root rewrites named in the first report (Strengthen, SubsumeForward, BVE,
  Instantiate) require `root-level`. The only public route to root with a
  conflict is now Restart, and Restart discharges that conflict.
- Above root, the remaining `kernel_collect_garbage` callers with a possibly
  live conflict are Reduce, which locks and forwards it, and analyze, which
  consumes it.

**Root Restart without a conflict** is a no-op: `recorded_conflict_still_holds`
returns false and clearing the already-clear fields changes nothing. Only
`stats.restarts` moves, and `test_api_c` / `test_restart_at_root_is_explicitly_applicable`
assert exactly that.

**Reduce and GC.**
- `lock_reasons` marks a large conflict `REASON` for both selection and
  collection.
- `reduce_prepare` returns early under a conflict.
- Sparse root collection is skipped under a conflict.
- `kernel_collect_garbage` forwards `conflict_ref` through `searched`. After the
  Restart repair, its `!garbage()` assert is reachable only on the locked Reduce
  path.
- `test_reduce_preserves_recorded_large_conflict` packs the arena, then checks
  clause ID 701 at the forwarded Ref and runs ConflictAnalysis.

**DefaultPolicy scheduling.** All three Restart sites now require
`k.level > 0` (`default_policy.cpp:812`, `:3083`, `:4093`). No conflict can be
live at any of them:
- `:3083` and `:812` sit after the search-loop conflict branch, which always
  continues or returns.
- `:4093` is reached only through `run_phase`, from startup at `:2827` (after
  the root backtrack at `:2699`) or from the loop at `:3227` / `:3235`.

So the removed `no-conflict` fact was never decisive, and the added `level > 0`
restores the removed `nonroot-level` fact. **No non-root inprocessing is
enabled, and the restart schedule is unchanged** (by code reading).
`random_policy.cpp:140` still draws keep-levels in `[0, level]`, now including
root.

## 2. Evidence

Pins: `build/root-refactor-20260912` (Release `-O3 -DNDEBUG`) and
`build/root-refactor-sanitize-20260912` (Debug
`-O1 -g -fsanitize=address,undefined`). Every focused executable and `tactix`
was relinked between 18:15:25 and 18:17:03, after the last source/test edit
(`tests/test_api_c.c`, 18:16:06).

**Full-suite logs** (`Testing/Temporary/LastTest.log`, read only):

| pin | run | passed | failed | sanitizer lines |
|---|---|---|---|---|
| Release | 18:16 | 146 | 0 | — |
| sanitize | 18:17–18:18 | 145 | 0 | 0 (`AddressSanitizer`, `runtime error:`, `LeakSanitizer`) |

- Both `LastTestsFailed.log` files are older than the corresponding run (18:02
  and 17:56), so they are stale.
- Both logs register `micro_proofs` with the fifth `test_restart_reduce`
  argument, and both print `micro_proofs OK`.

**Focused runs** (executables run directly, no arguments):

| test | Release | sanitize |
|---|---|---|
| test_restart_reduce | rc 0 `OK` | rc 0 `OK` |
| test_api_c (api_n1) | rc 0 `OK` | rc 0 `OK` |
| test_api_instances | rc 0, 26/26 applied | rc 0, same |
| test_random_online | rc 0, breadth and replay PASS | rc 0, same |
| test_reduce_root_lock | rc 0 PASS | rc 0 PASS |
| micro_proofs.sh (5 args) | rc 0 `micro_proofs OK` | rc 0 `micro_proofs OK` |

Result: 12/12, with zero sanitizer lines.

**Restart root-conflict LRAT.**
- `test_restart_reduce <path>` on both pins emits an identical proof, `4 0 1 2 3 0`
  (sha256 `b3c3f36b…4ffed2`).
- On `p cnf 3 3 / -1 / -2 / 1 2`, `lrat-check` prints `c VERIFIED`.
- The formally verified `cake_lpr` prints `s VERIFIED UNSAT`, and it rejects both
  controls (`4 0 1 3 0` and `4 0 0`) with "pivot must be non-zero".

**30-case DefaultPolicy differential.** I found no retained artifact:
- No exits, stdout, stderr, or proof bytes under `build/` or elsewhere in the
  repository dated after the diff began.
- The only nearby pin, `build/pin-refactor-fuzz-20260912-161150-894155/tactix`,
  was built at 16:11, before `7a668e76` was committed at 17:42, so I cannot
  confirm it is that commit's binary.
- `tools/fuzz/corpus/regressions` does hold 30 cases.

The claimed byte-identical 30/30 result is **not independently verified** here.
The no-schedule-change conclusion rests on the code reading in §1.

## 3. Findings

**F1 — oracle weakness, low severity, non-blocking.** The new `micro_proofs.sh`
leg does not really test the emitted chain. It checks the Restart root proof
with `lrat-check` on a fixture made only of input units, and on such formulas
`lrat-check` accepts wrong empty-clause proofs. Witnesses (all print
`c VERIFIED`, and `lrat-check` exits 0 even on `NOT VERIFIED`):

| formula | status | proof |
|---|---|---|
| `-1 / -2 / (1 2)` | UNSAT | `4 0 1 3 0` (wrong hints) |
| `-1 / -2 / (1 2)` | UNSAT | `4 0 0` (no hints) |
| `1 / 2` | SAT | `3 0 0` |
| `-1 / 3 / (1 2)` | SAT | `4 0 1 2 3 0` |

The script greps for `c VERIFIED`, so a wrong chain would still pass. `cake_lpr`
confirms that the actual proof is correct, so this is not an N2 defect in the
repair. Fix options: check this leg with `cake_lpr`, or use a fixture whose root
units are derived rather than input clauses. I did not survey other
`lrat-check` legs for the same exposure.

**F2 — coverage, non-blocking.** The regression tests reach the state white-box:
`kernel_assign(..., level 0)` runs before a decision instead of going through a
public chronological Backjump sequence. Only the LRAT branch of the discharge is
checker-validated; the DRAT `add_empty` branch has no checker leg in this diff.
It is the same `analyze.cpp:1214` code that ConflictAnalysis already relies on.

No correctness, N1, N2, or default-schedule blocker found.

## Limitations

- Correctness rests on code reading, focused tests, and existing suite logs.
  This checkpoint ran no fuzz campaign, stress scenario, real-instance proof
  check, or tier gate.
- There is no execution witness that drives chronological root implications
  plus a full Restart through the public API.
- The DefaultPolicy no-change conclusion is from code; the 30-case differential
  artifacts were not available to inspect.
- Release library asserts are compiled out. The sanitize pin carries the GC
  `!garbage()` assert coverage.

## Conclusion

**PASS.** The Restart repair routes a surviving root conflict into the existing
root-refutation branch of analysis before any root rewrite can run. No other
route reaches that state. Reduce and GC lock and forward live conflict refs, and
root Restart without a conflict is a no-op. DefaultPolicy still restarts only
above root, with no conflict live at any call site. On both pins the focused
tests pass 12/12, the suite logs show 146/146 (Release) and 145/145 (sanitize)
with zero sanitizer lines, and `cake_lpr` verifies the emitted proof. F1 and F2
are non-blocking follow-ups.
