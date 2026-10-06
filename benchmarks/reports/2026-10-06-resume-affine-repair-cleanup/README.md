# Crash recovery: affine publication and old-build cleanup

The interrupted work on base `5f647d7eb` repaired three affine service boundaries:
certified root units already present during certificate accumulation, certified
empty-clause contradictions, and captured sessions invalidated by later formula
changes. The recovered increment preserves cache generation and proof epoch even
when terminal publication does not retain cache support. Invalidated optional
sessions publish nothing, disable themselves, and allow ordinary CDCL to continue;
resource, interruption, unsupported-proof and invalid outcomes remain terminal.

## Validation

Fresh focused Debug and ASan/UBSan plus proof-population-shadow each pass all six
transaction/live/search and proof tests. Search proof fixtures now cover root-unit
and empty-reason publication through independent every-lemma DSR/Trestle checking.
The sanitizer pass uses leak detection outside the sandbox: the initial sandbox
run failed because LeakSanitizer cannot operate under ptrace, then the same tests
passed outside it. Exact binary hashes and gate dispositions are in
[validation.json](validation.json); retained logs are [tests.log](tests.log) and
[asan-tests.log](asan-tests.log). No target or performance gain is claimed.

Proof-rule synchronization and progress validation pass. Documentation size has
two pre-existing failures at the base commit: the winner subplan is 251/240 lines
and implementation notes are 569/560. This increment adds no lines to either;
compaction was not requested. Current candidate full Debug, both-policy
Claude-owned fuzz and four regression gates remain open.

## User-authorized storage cleanup

The user explicitly requested aggressive old-build cleanup after hitting quota.
The initial repository scan measured 745,753,146,880 allocated bytes. The sweep
removes 6,221 obsolete test executables occupying 306,372,995,584 allocated bytes
(306.37 GB / 285.33 GiB). Every selected path is now absent; [cleanup.json](cleanup.json)
records the exact paths, sizes, retained builds and earlier-pass removals.
Only old CMake builds' generated test executables were removed. Historical CTest
suites require a rebuild before rerunning; their source archives, manifests,
logs and result authority remain intact. Top-level solver binaries, independent
checkers and benchmark evidence were preserved, as were the current focused
builds, newest full Debug pin and both open fuzz reproducer pins.

The SLURM queue was inspected before removal: no Tactix batch campaign was
active. User allocation 1751450 runs `/bin/bash` from `/home/hwu`; unrelated jobs
were untouched. No benchmark certificate, cache or report was deleted.
