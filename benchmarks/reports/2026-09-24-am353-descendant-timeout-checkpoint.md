# AM353: descendant-held checker pipe timeout repair

Unaccepted tooling-only checkpoint, 2026-09-24 UTC. Source commit `9772a74c700782a3f4a78fb64b9e6a9b787cdded`, `git archive` SHA-256 `549e21f20591cd0bf6a129de8979aff54158b23ffd9e076e9303f503ad5839f3`. No solver rule, proof format, certificate routing, checker binary, model reconstruction, or benchmark resource contract changed. The AM347 terminal Opus finding A1 is the bounded handoff at `benchmarks/runs/20260924-am347-checker-boundaries-claude-fuzz/handoff/A1-bounded-timeout-waits-for-descendant-pipe/`.

## Cause and general repair

For allowances above the Python poll ceiling, `run_captured` uses repeated `Popen.communicate` calls under one monotonic deadline. On expiry it killed the direct child, then called `communicate()` again *without a timeout*. A descendant that inherited stdout/stderr kept those pipes open, so timeout delivery waited for that descendant instead of the deadline. The exact handoff took 6.028 s against a 0.5 s allowance on the current pre-fix code; the ordinary `subprocess.run` path took 0.513 s.

AM353 reaps the killed direct child with `wait()` and raises `TimeoutExpired` using the partial stdout/stderr already retained by the timed-out `communicate` call. Byte partials are decoded with the active text stream's encoding/error policy and universal-newline normalization; no post-deadline pipe read is attempted. The ordinary path, successful/nonzero completion, repeated wait ceiling, caller allowance, DRAT `-t`, and exception/status classification remain unchanged. This applies to every caller of `run_captured`, not a checker- or command-specific branch.

## Local evidence

- The exact A1 reproducer on the repaired live source returns `TimeoutExpired` at 0.523 s; ordinary path 0.530 s. The test ceiling forces the same bounded branch used by very large real allowances without waiting 24.8 days.
- New regression exercises both a still-running direct child and one that exits while its descendant holds the pipes. Each 0.25 s deadline returns in under 2 s, retains the exact stdout/stderr, and kills the test descendant in cleanup. Running this same test against the pre-fix function loaded from commit `0566165d` fails both subcases at 4.13 s; the current function passes both. The first in-memory old-module control accidentally kept the old module's default ceiling and therefore exercised the ordinary path; it is not counted. Existing tests continue to cover 22 successful communicate chunks, nonzero exit/full output, missing commands, child reaping, and numeric boundaries.
- `tools/bench/test_proof_oracle.py`: 27/27 pass. Binary, anonymous-scratch, and tier-runner units: 2/2, 3/3, and 14/14 pass. Runtime integration: 7/7 pass. Documentation size, proof-rule sync, comment hygiene, and diff checks pass; the living docs remain at their existing size ceilings and were not compacted.
- Full `tools/fuzz/selftest.sh` passes. Its 36 proof-dispatch tests pass. Kissat and CaDiCaL clean legs report 0 failures, with 145/150 and 96/100 cases reaching an oracle; five and four timeouts are explicitly uncovered, not accepted. Planted false-model and truncated-proof controls are flagged for the intended failure kinds; duplicate-input conversion and native-LRAT controls pass.

## Open acceptance

Independent Opus review/fuzz on the immutable AM353 archive is pending. It must compare old/new on exact A1, retain partial-output/deadline and child-reaping coverage on routed, DSR, PBP, tier-runner and fuzz surfaces, verify real certificate acceptance and known-bad rejection, and report any new bounded handoff. This local repair does not close N1/N2, the still-live AM349/AM351/AM352 campaigns, N3/N4, or the required performance gates. No Tier D task was submitted and no benchmark material was removed.
