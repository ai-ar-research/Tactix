---
name: release-gate
description: Evaluate and enforce Tactix release acceptance against N1--N4, PBP proof/model correctness, DefaultPolicy and random-policy fuzz, fixed performance regressions, public-interface trajectory parity, and measured overhead. Use before a version bump, tag, release publication, or claim that a candidate is releasable; do not use for ordinary intermediate commits.
---

# Tactix release gate

`CONTRIBUTING.md` under “Release gates” is the public authority; the local
development plan may impose additional milestone gates. This skill coordinates
the owning skills and does not weaken or reinterpret either authority.

## Freeze the comparison

1. Record the candidate and previous published release commits, binary hashes,
   build modes, checker pins, tier/manifest hashes, resources and solver flags.
2. State whether the release makes a performance claim. If it does, predeclare
   the target set, metric and release threshold before seeing candidate results.
3. Create immutable build pins. Never benchmark a mutable working tree, replace
   a frozen baseline, or omit an instance after results are known.
4. Reuse an existing result cell when its immutable evidence matches the gate:
   candidate/baseline commit and binary hashes, input and manifest hashes,
   build mode, solver flags, checker pins, resource contract, timeout, and
   required validation/cleanup fields. Record the reused evidence path and its
   hashes in the new disposition. Schedule only missing, incompatible, corrupt,
   or previously censored cells; do not rerun compatible completed experiments
   merely to recreate a full matrix. A deliberate contract change invalidates
   only the affected cells.

## Run the gates

Invoke the named project skills for their scopes:

- `proof-check`: check every required UNSAT PBP with pinned VeriPB and verified
  CakePB; validate every SAT model against the original CNF. Keep negative
  checker controls. Alternative supported proof routes retain their own gates.
- `fuzz`: obtain fresh terminal owner reports for at least 100,000 seeds each
  under DefaultPolicy and the controller-driven random policy, plus retained
  corpus and targeted reach. A missing action/configuration, pending report, or
  clean run without required coverage is incomplete.
  Scope large feature-specific targeted campaigns to candidates that change the
  owning feature, its scheduling or guards, or its feature-specific harness or
  oracle. For an unchanged feature, require its compact deterministic regression
  and reuse compatible immutable campaign evidence; do not count its optional
  stress leg toward the universal fresh-PBP seeds. Forced-TTF follows this rule.
  This scoping never waives targeted action/configuration coverage for changed
  controller foundations or shared execution.
- `benchmarks` and `slurm-jobs`: run matched performance and all four fixed
  regression sets. Apply row retention, bounded proof checking, the paper +10%
  ceiling, and the declared performance threshold exactly as the release
  contract states. Measure the paper ceiling in the paper-comparable proof
  mode. For the initial 0.2.0 default-PBP baseline, characterize PBP production
  and checking under predeclared resource caps without applying the paper +10%
  wall-time ceiling to that different proof route. Record the incompatible
  paper-comparable ceiling as not applicable for this transition, rather than
  incomplete; it must not block 0.2.0 solely because no same-route comparison
  exists. A timed-out row is a recorded baseline outcome, not a proof failure;
  missing, invalid, corrupt, or unchecked output still fails. For later
  releases, compare default PBP like-for-like with the preceding default-PBP
  release and require solve retention plus at most 10% aggregate production or
  checking-time regression.
  Score a CG `2x CryptoMiniSat` closure in the paper-comparable proof mode;
  never require a whole-run PBP wall time to fit the proof-free CMS target.
  Never subtract PBP overhead from a candidate's paper-comparable search
  regression.
- `compete`: run the current competition contract when packaging or command-line
  behavior changed.

Always run full Debug and Release tests and the focused public-client gate:

```sh
ctest --preset debug --output-on-failure
ctest --preset release --output-on-failure
ctest --test-dir build/release -R '^default_public_correspondence$' --output-on-failure
```

The public/static correspondence test must report `correspondence: COMPLETE`,
zero fallbacks, zero declined requests, and identical verdicts, proof bytes,
models and trajectory counters. Matched cluster evidence must keep the public
port within 5% of production on every fixed set and production within 5% of the
hardcoded twin. Do not delete, bypass, or narrow this test to make a candidate
pass.

## Decide and publish

Write one disposition listing every gate as PASS, FAIL, or INCOMPLETE, with
direct links to immutable evidence. Report solved counts, SAT/UNSAT splits,
PAR-2, paired-median gain, CG closures, timeouts, checker censors, memory and
cleanup. FAIL or INCOMPLETE prohibits the version bump, tag and release.

Before publication, use `wrap-up` to synchronize `PROGRESS.md`, generate the
Andrew report, review the diff, and commit/push only the coherent accepted
candidate. Never call an intermediate checkpoint a release candidate.
