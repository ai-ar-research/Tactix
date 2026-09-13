# Configurable-strategy closure — 2026-09-13

Candidate `690322ab` closes the online-strategy/N1 refactor without changing
the reference schedule or deterministic search trajectory. Its Release SHA-256
is `40b6b348adfbfd75736b45edc2c229463deb54876719ef11bfda4057eccaf9e8`.

## Closure

- Reduce takes a validated victim fraction, tier protection, tier cutoffs and
  collection choice through the typed C++/C invocation surface. Explicit
  single-clause instances remain independent of canonical protection.
- Walk owns a validated copied ProbSAT curve and even-call break base. The
  default rephase strategy owns rotation, failed-Walk fallback, warmup, random
  seed source and the reference curve.
- Reorder owns its target, maximum clause size, clause-decay, binary weight and
  polarity balance. Reduction/Vivify tier sources, focused phase pattern and
  the BVE bound ladder are also validated strategy data.
- The typed random generator varies every setting, serializes it in trace v3
  and accepts prior Reduce/Walk/Reorder tuple encodings for replay.
- E2 admits every legal nontrivial grid, including the zero-gain 2x2 equality
  case, through exact application and lazy complete enumeration. The shipped
  canonical matcher retains only the profitable frontier.
- Walk scoring is total over every validated positive break base. Scores that
  grow are normalized within the selected clause, and slowly decaying tables
  stop at their floating-point fixed point. This preserves the full parameter
  domain rather than restricting it around an implementation artifact.

The C ABI container sizes remain `sizeof(tactix_online_parameters)==88` and
`sizeof(tactix_online_step)==200`; appended Reduce, Walk and Reorder payloads
fit inside the existing union. Configuration checks classify all 66 kernel
settings, including 52 policy-owned fields.

## Audit and correctness

The production preprocessor audit finds only C/C++ linkage/header guards,
platform representation branches, assertions, and event, rule-journal and
proof-shadow instrumentation. Rule constants are limited to soundness bounds,
payload/bit encodings, data-structure representation, proof bookkeeping and
diagnostic sampling; admission, effort, thresholds, limits and schedule
choices are typed settings.

Candidate `690322ab` passes 146/146 Release tests. The seven affected Walk,
C API, online-executor and random-program tests pass under ASan+UBSan, including
bases 1, 25, 232, 999, 1000, 1001, 1500, 1999 and 2000. Native tests
independently check an original-CNF SAT model plus DRAT and LRAT proofs after
exact zero-gain factoring. Static documentation, configuration, rule/policy
boundary, option, citation and line-budget checks pass.

## Claude fuzz

Claude job `1686458` found no oracle error, but failed after noncanonical Walk
bases at or below one grew score tables without limit. Its
[failure report](2026-09-13-configurable-strategy-fuzz-0daf2ded.md) preserved
676 reproductions. Codex implemented and debugged the general repair in
`690322ab`.

Claude's single non-array recheck `1686614` then passed in 23:13; see the
[terminal report](2026-09-13-walk-scoring-closure-fuzz-690322ab.md). It replayed
all 676 former failures, including bases 25 and 232 first, with zero failures
and at most 517 MiB per replay. The fresh typed audit completed 2000/2000 and
the square Factor audit 600/600. It verified 2572 models, 911 DRAT proofs and
721 native LRAT proofs; every replay, determinism, cross-build, interruption
and negative control passed. All required rule and setting reachability was
met, including effective Walk bases below, at and above one. Anonymous scratch
cleanup found no residue.

## Runtime and memory

Only Tactix was rerun. Both arrays use the retained definitions and historical
`cpu-dense-hwulab-q` contract: 12 logical CPUs, 48,000 MiB, one solver core,
1,200 solver seconds and 16,000 checker seconds. Formula, stream, model, proof
and checker scratch are anonymous node-local objects.

| Set | `690322ab` | Pre-fix `0daf2ded` | Ratio | Retained refactor | Ratio |
|---|---:|---:|---:|---:|---:|
| B28+C5 | 949.997 s | 951.866 s | 0.998036x | 958.538 s | 0.991090x |
| Rank 1–50 | 8,146.381 s | 8,135.359 s | 1.001355x | 8,278.533 s | 0.984037x |

All 83 verdicts and certificate hashes match the pre-fix pin. All non-timing
statistics also match; summed and maximum peak RSS are byte-for-byte unchanged.
Jobs `1686615` and `1686616` continue proof/model checking in the background.
Every completed final record reports deleted formula, certificate and derived
scratch. Pending checkers do not block independent work.

## Operations

Storage guard PID `1076798` measures the complete repository every ten minutes,
holds only pending repository jobs above 700 GiB, never suspends running jobs
and releases only its own holds. Usage was 106 GiB at this checkpoint. Existing
Tier-D array `1685130` remains unchanged.
