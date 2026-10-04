# Public controller registry and full-Debug checkpoint

## Disposition

The current public-controller source passes the complete Debug suite: **285 of
285 tests** in 957.60 seconds with four concurrent tests.  This closes the
local conformance break introduced while appending Direct PHP, GF(3) Gaussian,
and Tseitin cycle-breaking actions.  It does not establish effective-action
fuzz reach, target recovery, regression-gate acceptance, or portfolio closure.

## Failures and repairs

The first full run exposed eight failures and two still-running long tests.
None was a wrong model, rejected proof, or theory-action derivation failure.

- The append-only C API registry, root-guard classification, default posed-only
  inventory, and random-support inventory stopped at GF(3) Gaussian and omitted
  the newly appended `TseitinCycleBreaking` action.  They now name its exact
  public role: producer-owned, root-only, disabled in the shipped default.
- Generic random online generation could sample a bundled `Propagate` after
  `DirectPhpRefutation`, which the public validator correctly rejects because
  the successful action is terminal.  Generation now consumes the same random
  draw but suppresses bundled propagation for terminal/separate-propagation
  actions.  A 256-seed Direct-PHP regression requires `PostApplication::None`
  and validates every generated program.
- The BVE cache test retained the superseded assertion that Lucky/8 was retired.
  It now requires the restored shared affine action to configure successfully
  without executing or touching the occurrence cache during configuration.
- `test_bce` and `test_cce` were zero-byte, non-executable artifacts from an
  interrupted build.  Rebuilding their targets restored the binaries; both
  pass without a source workaround.
- The scripted-default Debug example used a formula that drove about 75,000
  externally stepped conflicts.  The accepted controller checkpoint showed
  the same cost, so this was not a new solver regression.  A deterministic
  140-variable/596-clause fixture retains verdict equality and positive
  restart, reduction, mode-switch, and rephase coverage in both built-in and
  public-API schedules with about 4,000 external conflicts.

The invalid Direct-PHP follow-up explained the `micro_proofs` and
`sweep_proofs` failures.  The other failures were exact conformance drift or
incomplete build artifacts rather than a shared runtime cause.

## Validation

The current Debug build `build/tseitin-public-root-review` records:

- full CTest: 285/285 passed, 957.60 seconds;
- `demo3_scripted_default`: 117.45 seconds alone and 183.63 seconds in the
  four-way full run;
- `tseitin_cycle_action_journal`: 1/1 in 3.79 seconds alone and 5.12 seconds in
  the full run, including ASCII/binary/repeated-root proof checks;
- strengthened `direct_php_public`: 1/1 after rebuilding the added 256-seed
  no-follow-up regression;
- the formerly failing scheduling, API, random-online, BVE-cache, BCE, CCE,
  micro-proof, and sweep-proof tests all pass in the full run.

The historical long `lucky` test remains the suite maximum at 920.96 seconds;
`symmetry_backend` took 308.45 seconds and `feedback_frontend` 269.94 seconds.
Those costs are recorded rather than hidden by a smaller test selection.

## Remaining acceptance work

Freeze a new sanitizer pin from the committed repair before broad fuzzing.  The
disabled DefaultPolicy remains the zero-reach compatibility control.  The
random controller must show producer calls/offers and effective applications
for Direct PHP, GF(3) affine/Gaussian, Direct CSP, and Tseitin cycle breaking,
with replay and independent proof/model checking.  Only a passing report may
advance the frozen target projections and then the B28+C5, Rank1--50, WV100,
and WV2--89 gates.  No benchmark or performance campaign was launched for this
checkpoint.
