# 5647: propagation-cursor fixture crash review (bounded, plan §8.2)

Terminal outcome: **PASS**. Root cause is a fixture error, not a solver defect.
The kernel change in prototype 5641 is state-identical to its predecessor on
every fixture case, and the repaired suites pass in all three independent pins.

## Scope

- Prototype under review: `.claude/worktrees/runtime-propagation-cursor-5641`
  (plain source copy of predecessor 5623 plus `src/kernel/kernel.cpp` and
  `tests/test_propagation_cursor.cpp`; not a git worktree, HEAD d887fc28).
- Predecessor (immutable): `.claude/worktrees/runtime-congruence-index-repair-5623`.
- Repair copy: `.claude/worktrees/runtime-propagation-cursor-repair-5647`
  (byte-identical copy of 5641 before the repair; only the fixture changed).
- Original failure source, pins `build/pin-564{1,2,3}-propagation-cursor-*`,
  and `benchmarks/runs/20260908-5641-propagation-cursor/logs/` are untouched;
  their hashes re-verified after this review (`source-and-prototype-hashes.sha256`).
- No SLURM job was submitted or cancelled. No production, root doc, symlink
  target, or frozen artifact was edited. No commit or push.

## Reproducer and root cause

Reproducer: `build/pin-5641-propagation-cursor-release/tests/test_propagation_cursor`
segfaults in the first fixture case (`logs/reproducer-5641-gdb.r1.log`):

```
Program received signal SIGSEGV, Segmentation fault.
0x0000000000000000 in ?? ()
#0  0x0000000000000000 in ?? ()
#1  0x0000000000404969 in fixture(unsigned int, bool, bool, bool, bool, unsigned int) ()
#2  0x0000000000402526 in main ()
```

Disassembly of the call site (`objdump -d`, `fixture+0xf16`):

```
  404930: callq  tactix::tactix_rules()
  40493d: callq  tactix::tactix_rule_id(char const*)
  404962: lea    (%rcx,%rax,8),%rax      ; &tactix_rules()[id]  (sizeof RuleVTable == 56)
  404966: callq  *0x20(%rax)             ; RuleVTable::instances  -> NULL
```

Offset 0x20 of `RuleVTable` is the `instances` slot. The fixture line

```cpp
  uint64_t instance;
  assert(rule.instances(k, &instance, 1) == 1);
```

called the raw registry slot. Propagate registers only `guard` and `apply`
(`src/rules/propagate/propagate.cpp:91`); `RuleVTable::instances` is
documented as "null for rules with no per-application choice"
(`src/rules/registry.hpp`). The canonical-only enumeration is a public-API
fallback in `tactix_rule_instances` (`src/api/api.cpp:200`), which is not
reachable from a C++ TU that uses `namespace tactix` (the header's `tactix`
typedef collides; see `src/api/api.cpp:1-5`). Every internal caller of the raw
slot is null-guarded (`src/policies/random_policy.cpp:139`,
`src/policies/default_policy.cpp:3624`; `src/policies/trace.cpp:141` guards
`apply_instance`). The public behaviour is already pinned by
`tests/test_api_instances.c` (`NO_SURFACE` and `CANONICAL_ONLY` lists).

Classification: **fixture setup/API error**. The kernel cursor change is not
implicated: the crash happens after every kernel assertion of the first case
has already passed, and the old/new digest comparison below is equal.

## Kernel change review (no defect found)

`prototype-5641-vs-5623-kernel.diff` is the only kernel change. Static review of
`src/kernel/kernel.cpp` (5641):

- `propagate_literal<>` (lines 1564-2040) contains no read or write of
  `propagate_head` and no clause addition, so the only cursor rewind path
  (`kernel_rearm_propagation`, line 276, clause-add only) cannot fire inside a
  scan. The local cursor therefore observes exactly what the field did.
- Diagnostic builds publish the cursor before each scan, so event-trace,
  rule-journal and conflict-stop observers see the same post-increment value.
- `kernel_settle_binary_only_debt`, conflict exit (`propagate_head == 1`), and
  `stats.propagations` accounting are unchanged in effect.

Dynamic evidence: the repaired fixture's 224-case state digest is identical when
the same fixture source is linked against the predecessor 5623 objects
(`predecessor-objlink/`, built from `build/pin-propagation-cursor-repair-5647-predecessor`):

| binary | digest |
|---|---|
| 5647 release / bc / trace (new kernel) | `f82475e400a7f62b` |
| predecessor 5623 objects (old kernel) | `f82475e400a7f62b` |

The digest covers cursor, debt head, conflict record, level/unassigned counters,
propagation/tick/search-tick/root-assignment/jumped-reason statistics, the trail
with per-variable assignment records, values, the clause arena, watch heads and
watch arena. The Release solver binary of the repair copy is byte-identical to
the prototype's (`032ef0ba…`), as expected since `kernel.cpp` is unchanged.

## Repair (exact diff: `fixture-repair.diff`)

Smallest correct change, in `tests/test_propagation_cursor.cpp` only:

1. Replace the raw-slot call with the registry contract the API's fallback keys
   on: `rule.guard == propagate_guard && rule.apply == propagate_apply` and
   `!rule.instances && !rule.apply_instance`.
2. Strengthen the guard check to unconditional `!propagate_guard(k)` after every
   scan (a conflict exit records the conflict; a fixpoint exit publishes the
   consumed trail), and add the pre-scan check
   `propagate_guard(k).eligible == (start < k.trail.size())` so the guard is
   tied to the settled cursor including binary-only debt.
3. Add `registry_dispatch(refute)`: the canonical instance through
   `apply_rule` (the dispatch shared by `tactix_apply` and
   `tactix_apply_instance(TACTIX_INSTANCE_CANONICAL)`), checking guard
   admission, published cursor and propagation count, the implied value, and
   root refutation (`ApplyResult::Unsat`, `k.inconsistent`).

No expected behaviour was weakened. Kernel source is unchanged
(`a1071669…` in both 5641 and 5647).

## Validation

Builds (`logs/configure-*.log`, `logs/build-*.log`), same configuration as the
prototype pins (GCC 13.1.0, Release `-O3 -DNDEBUG`; trace adds
`TACTIX_ENABLE_EVENT_TRACE=ON`):

- `build/pin-propagation-cursor-repair-5647-release`
- `build/pin-propagation-cursor-repair-5647-bc`
- `build/pin-propagation-cursor-repair-5647-trace`
- `build/pin-propagation-cursor-repair-5647-predecessor` (5623 sources,
  `tactix_lib` target only, for the digest comparison)

Direct fixture runs (`logs/fixture-direct.r1.log`): 224 cases, exit 0 in all
three pins. Full suites (`logs/ctest-{release,bc,trace}.r1.log`, run in the
session shell like the prototype's suites):

| pin | result |
|---|---|
| release | 100% passed, 89/89 |
| bc | 100% passed, 89/89 |
| trace | 100% passed, 88/88 |

Hashes (`repair-binary-hashes.sha256`, `source-and-prototype-hashes.sha256`):

| artefact | sha256 |
|---|---|
| kernel.cpp (5641 == 5647) | `a1071669c076a109525f4a9d9f1032a74d3b96818b2041f1436ad9584d0686aa` |
| kernel.cpp (5623) | `6a5844b84d9a7868cb136a2391673a1c80b25c39fb5b5494fc4187f643ced059` |
| fixture (5641, crashing) | `e24d813332251587cd2ee6488fdab9dc7e98ec5a39117010461bc95fe40cb1b4` |
| fixture (5647, repaired) | `099e6d59f46ec2c9469b764435c42fff99fb4dbfabe88e567e60f7c1f2732338` |
| tactix release == bc (5647) == 5641 release == 5643 bc | `032ef0ba2d3cac6f3579b2e158de36488a1110e2b586e81d5ca7c62f12cdbbb8` |
| tactix trace (5647) == 5642 trace | `b42f0e54533c5032db5b318b4baab9e508c513fff92d454f692acbfc89f19f4b` |
| test_propagation_cursor release == bc (5647) | `a6afffab397ceccdb4bf9a7fe0d5b5a588c8d9d98b582943372716c3ec0a98a2` |
| test_propagation_cursor trace (5647) | `51834f46ffb236bbb61dc490a4949153d0bd24cc829ccf2efb78b992ff240b06` |

## Implications and residual risk

- No semantic, proof or model behaviour is implicated; no independent
  model/proof validation was required and none was run. Predecessor jobs
  (5644/1672779, 5628/5629, 5592) and the 5610 fuzz do not execute this source
  and share no defect; leave them running.
- The fixture exercises small, fixed formulas only. It does not cover the
  probing/warming propagators (unchanged by 5641) or long-run interaction with
  compaction, which the ordinary Tier and fuzz gates own.
- The diagnostic-build publish-before-scan is verified by the trace pin's suite
  and identical digest, but no event-trace comparison against a real instance
  was run here (out of scope for a bounded fixture review).
- `apply_rule_instance` still dereferences `apply_instance` unconditionally;
  every current caller guards it, so this is a latent footgun for future
  fixtures, not a defect.

## Cleanup

Nothing was deleted. Disposable after the parent absorbs this review: the four
`build/pin-propagation-cursor-repair-5647-*` directories (they contain CTest
`Testing/` output from the runs above) and the `predecessor-objlink/` binary.
The 5641 pins, logs and source remain the exact reproducer.
