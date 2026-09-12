# D-NOOP closure: Claude final fuzz/review checkpoint (2026-09-12)

Scope: the uncommitted D-NOOP diff on top of `bb03aea2` (`git diff | sha256sum`
= `3831ef5b…90244bc3`). Public Restart and Reduce now use only
`consistent`. Restart refreshes a recorded conflict after backtracking. Reduce
may run with a recorded conflict and locks and forwards a large conflict clause.
DefaultPolicy adds `k.level > 0` at its two Restart call sites.

This was a bounded local checkpoint, run as SLURM job 1683143 in the session
shell. It includes no fuzz campaign, no SLURM submission, and no benchmark. I
made no source, test, or documentation edits.

## 1. Focused tests

Both pins were built after the last source edit. `test_random_online` was
rebuilt at 18:07:47 and 18:08:01, after `tests/test_random_online.cpp` changed
at 18:07:37. The Release pin uses `-O3 -DNDEBUG`, so library asserts are off
there. All five test sources `#undef NDEBUG`, so their own assertions stay live.
The sanitize pin is Debug `-O1 -g -fsanitize=address,undefined`. I ran the
executables directly from the `CTestTestfile.cmake` registrations, which take no
arguments. I did not use `ctest`, so nothing was written into the pins.

```
for b in root-refactor-20260912 root-refactor-sanitize-20260912; do
  for t in test_restart_reduce test_api_c test_api_instances \
           test_random_online test_reduce_root_lock; do
    /home/hwu/SAT-solving/build/$b/tests/$t
  done
done
```

| ctest name | executable | Release | ASan+UBSan Debug |
|---|---|---|---|
| restart_reduce | test_restart_reduce | rc 0, `OK` | rc 0, `OK` |
| api_n1 | test_api_c | rc 0, `OK` | rc 0, `OK` |
| api_instances | test_api_instances | rc 0 | rc 0 |
| random_online | test_random_online | rc 0, `…breadth and replay PASS` | rc 0, same |
| reduce_root_lock | test_reduce_root_lock | rc 0 | rc 0 |

Result: **10/10 pass**, with no sanitizer reports.

## 2. DefaultPolicy schedule review

These are all the `restart_guard` and `reduce_guard` uses in
`src/policies/default_policy.cpp`:

- **`:812` `prepare_rephase_root`:** the new `k.level > 0 &&` restores the
  removed `nonroot-level` fact. The `no-conflict` fact is not needed here. This
  site runs at `:3119`, after the loop's conflict branch.
- **`:3083` search restart:** same restoration.
- **`:4093` inprocessing phase entry:** already had `k.level > 0`, so it is
  unchanged.
- **`:3003` search reduce:** it lost `no-conflict`. That fact was redundant: the
  conflict branch (`:2910` onward) always ends in `continue` or `return`, so
  `conflict_valid` is false when control reaches `:3003`.

Conclusion: **no change to DefaultPolicy's non-root schedule.** This is based on
reading the code only; I did not compare stats or proofs with the pre-change
build.

## 3. Reduce and Restart path review

- **Reduce with a conflict:** `reduce_candidates` and `reduce_apply_refs` both
  call `lock_reasons`. That marks the conflict clause `REASON`, so it cannot be
  selected, and the re-validation loop skips it. `reduce_prepare` returns early,
  which leaves the trail and `unflushed_root` alone. Collection falls back from
  `kernel_sparse_collect_root` to `kernel_collect_garbage`, so root-false
  literals are never stripped from the conflict clause. The new forwarding in
  `kernel_collect_garbage` runs before unlocking. A conflict clause is entirely
  false, so it can never also be a trail reason, and unlocking cannot clear a
  real reason flag. I found no hole in this path.
- **Restart keeping a non-root prefix:** a conflict that stays falsified below
  the current level is handled by the chronological conflict-level path in
  `analyze.cpp:1229`.

### Finding (blocking, plausible, no witness): Restart can leave a live conflict at level 0 that root rewrites do not protect

**Before this diff**, `conflict_valid && level == 0 && !inconsistent` could not
happen:
- A root conflict found by Propagate sets `inconsistent` (`propagate.cpp:43`).
- Restart required `no-conflict`.

**Now** `restart_backtrack_to(k, 0)` keeps `conflict_valid` whenever the
conflict clause is still falsified at the root, meaning every literal in it is
root-assigned. That can happen through D-LEVEL-2: with chronological
backtracking, which the public Backjump keep-levels make reachable, Propagate
running above level 0 can stamp implied literals at level 0. A clause falsified
by those literals becomes a conflict whose literals are all level 0. A full
Restart then lands in the new state.

In that state, every root rewrite guard is only `consistent` + `root-level`.
Strengthen and SubsumeForward say so explicitly (D-SPLIT-3), and the same
applies to BVE, Instantiate, and the other rules without `no-conflict`. So these
rules can still run:
- Strengthen removes root-false literals, and every literal of this conflict
  clause is root-false. It can retire the clause and then call
  `kernel_collect_garbage` (`strengthen.cpp:1484/1522`).
- SubsumeForward (`:363/417`), BVE (`:2223`) and Instantiate (`:327-349`) have
  the same shape.

None of them locks the conflict clause. The new block in
`kernel_collect_garbage` then either:
- aborts on `assert(!c->garbage())` in Debug or sanitize builds, or
- in Release, reads a garbage record's `searched` search cursor as a forwarding
  Ref. `conflict_ref` ends up pointing inside an unrelated clause, and a later
  ConflictAnalysis resolves against the wrong literals and antecedent id. That
  would be a DRAT/LRAT or verdict failure (N2).

No test covers this. `test_restart_can_abandon_or_retain_a_conflict` keeps the
conflict only at level 2, and the root-conflict API test abandons it. I did not
build a witness, because creating a probe was outside this checkpoint's write
scope.

Possible repair directions (Codex owns the choice):
- Make a root-retained conflict terminal: run the analysis-style refutation or
  mark it inconsistent.
- Or give root rewrites the same conflict-antecedent protection that Reduce has
  (lock, skip, forward), and state the rule in `docs/calculus.md` next to
  D-SPLIT-3.

A regression witness would be: an API sequence under chrono that reaches a
level-0-literal conflict above the root, then `Restart(0)`, then `Strengthen`,
run on the sanitize pin.

## Limitations

- These are focused unit and API tests only. I did not run a random-online fuzz
  leg, a proof-checker leg over real instances, a stress scenario, or Tier gates.
- The DefaultPolicy no-change result comes from code reading, not from matching
  stats or proofs.
- The finding is based on code reading, with no execution witness.

## Conclusion

**BLOCK.** The focused tests pass 10/10 on both pins, and the DefaultPolicy
schedule is unchanged. The block is the finding above: a Restart can leave a live
conflict at level 0 that root rewrites neither lock nor forward, and the new
`kernel_collect_garbage` forwarding turns that into an assert abort or a corrupt
`conflict_ref`. Close it, or show it cannot be reached, before committing
D-NOOP.
