# Contributing to Tactix

Develop changes on a named non-default branch. Keep public `main` for reviewed
integration; do not use it as a development worktree.

## Build and test

```sh
cmake --preset debug                       # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --build --preset debug --parallel
ctest --preset debug                       # unit, API, proof and example tests
```

The release preset (`--preset release`) is the one to measure performance
with. Every change should keep the debug test suite green.

Routine test presets exclude the `extended` label. Keep individual CI tests
under ten seconds by limiting their workload; exhaustive sweeps, large solver fixtures and
long external proof checks belong in explicit extended runs:

```sh
ctest --test-dir build/debug -L extended --output-on-failure
```

## Correctness checks

Tactix's correctness rests on independent checking, not on trusting the
solver:

- every UNSAT answer must come with a proof that the external checkers accept
  (`tools/bench/proof_oracle.py check`, see
  [docs/cli.md](docs/cli.md));
- every SAT answer must satisfy the original formula (`tools/fuzz/checkmodel`).

`tools/fuzz/` is a differential fuzzer that generates formulas, runs Tactix
through its public DefaultPolicy and public-random clients, and checks every
answer against a reference solver and the proof checkers:

```sh
tools/checkers/setup.sh          # proof checkers
tools/solvers/setup.sh           # reference solvers (Kissat, CaDiCaL)
tools/fuzz/run.sh --quick --solver 'build/release/examples/tactix-default --auto-proof'
tools/fuzz/run.sh --public-random --quick \
  --solver 'build/release/tests/tactix-public-random --auto-proof --random-policy={seed}'
```

`--quick` runs about 10,000 cases plus the regression corpus in
`tools/fuzz/corpus/`; see [tools/fuzz/README.md](tools/fuzz/README.md) for the
other modes. A formula that exposes a bug belongs in the regression corpus.

Routine fuzzing also includes retained/generated incremental C API traces with
minimization and C/Python binding regressions. Full CI runs 16 traces per
Debug/Release build; Python coverage is deterministic. See the
[fuzz harness](tools/fuzz/README.md#routine-fuzzing-bundle).

## Release gates

Pin the candidate, preceding release, binaries, inputs, flags, checkers and
resources. Reuse passing evidence after an immutable scope/risk audit; rerun
affected, missing or censored checks. Commit labels alone invalidate nothing.
Performance reuse requires matching measurement keys or established equivalence.
Preserve original grades, failures and reach obligations: passing cells do not
qualify an incomplete campaign. Missing or pending required evidence blocks release.

- **Fuzz:** at least 100,000 independently checked verdicts and at most 5%
  nontermination per DefaultPolicy/public-random leg, plus retained corpus and
  targeted action/configuration reach. Use shipped `--auto-proof` (DSR first,
  converting to PBP when required), with explicit `--dsr`/`--pbp` backend and
  conversion coverage. Independently check original-CNF models and each proof's
  declared content route. Crashes, sanitizer/invariant findings, wrong verdicts,
  invalid models/proofs fail; missing reach is incomplete. Large targeted
  campaigns cover changed features, scheduling, guards, harnesses or oracles;
  unchanged features retain compact regressions and compatible evidence.
  Retired one-off stress scripts do not replace qualification legs.
- **Performance:** a performance release predeclares its target/metric and
  demonstrates at least 5% paired-median solver-time or PAR-2 improvement outside
  noise, with no lost checked solve. During the algebraic milestone, add a CG
  closure within 2x matched CryptoMiniSat time without losing a retained closure.
  That proof-free target is separate from whole-run PBP cost. A functionality
  release passes preservation gates without claiming a gain.
- **Regression:** full Debug/Release suites and B28+C5, Rank1–50, WV100 and
  WV2–89, retaining checked rows and strict memory, cleanup and per-instance
  limits. Paper-comparable solver time may regress at most 10%; proof cost
  cannot absorb search regressions. Compare proof production/checking like for
  like with the previous release: retain solves at the same cap and limit
  aggregate production or checking-time regression to 10% (or a stricter
  declared threshold). Measure automatic selection, conversion and route costs.
  Release 0.2.0 established the PBP baseline without an incompatible paper
  ceiling. The automatic-default transition retains solves against that PBP
  predecessor and reports per-route sizes/checking times; later releases retain
  the automatic baseline. A timeout is recorded, not a rejected proof; missing,
  invalid or unchecked required evidence does not pass.
- **C default:** keep `examples/default_strategy` complete.
  `c_default_correspondence` requires zero unported paths and identical verdicts,
  proof bytes, models and trajectory counters. On each fixed set, the C default
  stays within 5% of production, and production within 5% of the hardcoded twin.

Every applicable gate must pass before a version bump, tag or publication.

## Adding a rule or changing one

A rule is the unit of soundness and of the public interface:

1. Specify its precondition, formula and proof effects, and implementation
   guards in the change and its tests.
2. Register it in `src/rules/registry.cpp`. Rule ids are append-only; never
   renumber or reuse a retired slot, since external strategies refer to ids.
3. Make it reachable through `include/tactix.h`; the public interface only
   grows.
4. Every effect on the clause database must be logged to the proof, and the
   proof must check with the routes described in [docs/cli.md](docs/cli.md).
5. Add unit tests under `tests/` and run the fuzzer with the rule enabled.

A change to the default strategy's scheduling should name the reference
behavior it follows or the reason it departs from it.

## Code style

The solver core is C++20 without exceptions or RTTI, written in a C-like
style; match the surrounding code. Comments state invariants and point to
references; history belongs in commit messages.

## License

By contributing you agree that your contribution is licensed under the MIT
License of this repository.
