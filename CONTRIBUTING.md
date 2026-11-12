# Contributing to Tactix

Develop changes on a named non-default branch. Keep public `main` for reviewed
integration; do not use it as a development worktree.

## Build and test

```sh
cmake --preset debug                       # AddressSanitizer + UndefinedBehaviorSanitizer
cmake --build --preset debug --parallel
ctest --preset debug                       # unit, API, proof and example tests
```

Measure performance with the release preset. Every change keeps the debug
suite green. Routine presets exclude the `extended` label: keep each CI test
under ten seconds, and put exhaustive sweeps, large solver fixtures and long
external proof checks in explicit extended runs:

```sh
ctest --test-dir build/debug -L extended --output-on-failure
```

## Correctness checks

Tactix's correctness rests on independent checking: every UNSAT answer must
come with a proof that the external checkers accept
(`tools/bench/proof_oracle.py check`, see [docs/cli.md](docs/cli.md)), and
every SAT answer must satisfy the original formula (`tools/fuzz/checkmodel`).
The differential fuzzer in `tools/fuzz/` generates formulas, runs them through
the public DefaultPolicy and public-random clients, and checks every answer
against a reference solver and the proof checkers:

```sh
tools/checkers/setup.sh          # proof checkers
tools/solvers/setup.sh           # reference solvers (Kissat, CaDiCaL)
tools/fuzz/run.sh --quick --solver 'build/release/examples/tactix-default --pbp'
tools/fuzz/run.sh --public-random --quick \
  --solver 'build/release/tests/tactix-public-random --pbp --random-policy={seed}'
```

`--quick` runs 10,000 cases plus the regression corpus in `tools/fuzz/corpus/`,
where every formula that exposes a bug belongs; see
[tools/fuzz/README.md](tools/fuzz/README.md) for the other modes.

## Release gates

Every release candidate pins the preceding published release, immutable build
and checker hashes, benchmark manifests, resources and solver arguments.
Pending, censored or missing evidence does not pass a required gate. A version
bump, tag or release publication waits for every applicable gate to pass.

- **Performance:** a performance release predeclares its target and metric,
  then demonstrates at least a 5% paired-median solver-time or PAR-2
  improvement outside the measured noise range, without losing an
  independently checked solve. During the algebraic milestone it also adds at
  least one retained CG instance within 2x its matched CryptoMiniSat time in
  the paper-comparable proof mode. That proof-free CMS target is a separate
  algebraic-development target and never bounds whole-run PBP wall time, which
  passes the verified-completion and like-for-like proof gates below. A
  functionality release need not claim a gain, but passes the preservation
  gates and is not advertised as faster.
- **Fuzzing:** at least 100,000 independently checked fresh PBP verdicts and at
  most 5% nonterminating generated cases under each of DefaultPolicy and the
  public-interface random policy, plus the retained corpus and targeted
  action/configuration coverage. Every SAT model and UNSAT proof is
  independently checked; any crash, sanitizer, invariant, verdict, model or
  proof failure rejects the candidate, and missing required reach is incomplete
  even when no case fails. Large feature-specific campaigns are required when
  the candidate changes the owning feature, its scheduling or guards, or its
  feature-specific harness or oracle; an unchanged feature keeps its compact
  deterministic regression and may reuse compatible prior campaign evidence.
  Retired one-off controller/stress scripts are not release legs; stable
  failures become corpus entries or compact deterministic tests.
- **Regression:** the full Debug and Release suites pass; B28+C5, Rank 1--50,
  WV100 and WV2--89 keep their retained rows and aggregate limits; aggregate
  solver time stays within 10% of the paper release, measured in the
  paper-comparable proof mode when the candidate is on the paper-compatible
  route. Default proof production and checking are measured separately and
  like-for-like against the preceding release: no lost independently checked
  solve at the same cap and at most 10% aggregate production or checking-time
  regression, unless a stricter predeclared performance claim applies. The
  release that makes automatic proof selection the default compares against
  the preceding default-PBP release, whose trajectory is that of `--pbp`, and
  reports per-route certificate sizes and checking times. PBP cost never
  absorbs a search regression inside a compatible route. Correctness, memory,
  cleanup and per-instance limits remain strict.
- **C default strategy:** `examples/default_strategy` stays complete.
  `c_default_correspondence` reports no unported path and byte-identical
  proofs, models, verdicts and trajectory counters against DefaultPolicy. In
  matched measurements `tactix-default` stays within 5% of production on each
  fixed regression set, and production within 5% of the hardcoded twin.

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
