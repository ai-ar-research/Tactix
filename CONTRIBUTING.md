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

## Correctness checks

Tactix's correctness rests on independent checking, not on trusting the
solver:

- every UNSAT answer must come with a proof that the external checkers accept
  (`tools/bench/proof_oracle.py check`, see
  [docs/getting-started.md](docs/getting-started.md));
- every SAT answer must satisfy the original formula (`tools/fuzz/checkmodel`).

`tools/fuzz/` is a differential fuzzer that generates formulas, runs Tactix
under its default and randomized strategies, and checks every answer against a
reference solver and the proof checkers:

```sh
tools/checkers/setup.sh          # proof checkers
tools/solvers/setup.sh           # reference solvers (Kissat, CaDiCaL)
tools/fuzz/run.sh --quick --solver build/release/tactix
```

`--quick` runs about 10,000 cases plus the regression corpus in
`tools/fuzz/corpus/`; see [tools/fuzz/README.md](tools/fuzz/README.md) for the
other modes. A formula that exposes a bug belongs in the regression corpus.

## Release gates

The 0.2.0 release establishes the first default-PBP operational baseline.
Because the paper release used a different proof route, its PBP production and
checking time is characterized and reported, not forced under the paper +10%
solver-time ceiling. The baseline still requires valid independently checked
proofs, complete result and cleanup records, and predeclared solver/checker time
and memory caps; a timeout is a measured baseline outcome rather than a proof
failure. Beginning with the release after that baseline, every release candidate
pins the preceding published release, immutable build and checker hashes,
benchmark manifests, resources, and solver arguments. Pending, censored, or
missing evidence does not pass a required gate.

- A performance release predeclares its target and metric, then demonstrates at
  least a 5% paired-median solver-time or PAR-2 improvement outside the measured
  noise range, without losing an independently checked solve. During the
  algebraic milestone it must also add at least one retained CG instance within
  2x its matched CryptoMiniSat time. A functionality release need not claim a
  gain, but must pass the preservation gates and must not be advertised as
  faster.
- Release fuzzing runs at least 100,000 fresh PBP cases under each of
  DefaultPolicy and the controller-driven random policy, plus the retained
  corpus and targeted action/configuration coverage. Every SAT model and UNSAT
  proof is independently checked; any crash, sanitizer, invariant, verdict,
  model, or proof failure rejects the candidate. Missing required reach is
  incomplete, even when no generated case fails.
- Regression acceptance requires the full Debug and Release suites, retained
  rows and aggregate limits on B28+C5, Rank 1--50, WV100, and WV2--89, and at
  most 10% aggregate solver-time regression from the paper release. Measure
  that ceiling in the paper-comparable proof mode. Measure default-PBP
  production and checking separately. After 0.2.0, compare it like-for-like
  with the preceding default-PBP release: no lost independently checked solve
  at the same cap and at most 10% aggregate production or checking-time
  regression unless a stricter predeclared performance claim applies. PBP cost
  cannot be used to absorb a search regression. Correctness, memory, cleanup,
  and per-instance limits remain strict.
- Keep the complete DefaultPolicy implementation through the public port.
  `default_public_correspondence` must report no fallback or declined request
  and byte-identical proofs, models, verdicts, and trajectory counters against
  production. In matched measurements the public port must remain within 5% of
  production on each fixed regression set, and production must remain within
  5% of the hardcoded twin.

The project skill `.claude/skills/release-gate/` coordinates these checks and
records one PASS, FAIL, or INCOMPLETE disposition. A version bump, tag, or
release publication waits for every applicable gate to pass.

## Adding a rule or changing one

A rule is the unit of soundness and of the public interface:

1. Its semantics (precondition, effect, proof step) go in
   [docs/calculus.md](docs/calculus.md), and implementation-specific guards in
   [docs/implementation-notes.md](docs/implementation-notes.md).
2. Register it in `src/rules/registry.cpp`. Rule ids are append-only; never
   renumber or reuse a retired slot, since external strategies refer to ids.
3. Make it reachable through `include/tactix.h` (the interface only grows; see
   [docs/c-api.md](docs/c-api.md)).
4. Every effect on the clause database must be logged to the proof, and the
   proof must check with the routes in
   [docs/getting-started.md](docs/getting-started.md).
5. Add unit tests under `tests/` and run the fuzzer with the rule enabled.

The default strategy follows Kissat and CaDiCaL;
[docs/scheduling-parity-ledger.md](docs/scheduling-parity-ledger.md) records,
mechanism by mechanism, how its scheduling corresponds to theirs. A change to
the default strategy's scheduling should name the reference behavior it
follows or the reason it departs from it.

## Development records

Some source comments cite development documents that are no longer in the
working tree: the development plan (`plan §…`, `DEVELOPMENT_PLAN.md`),
benchmark reports (`benchmarks/reports/…`) and design notes (`docs/design/…`).
They remain in the repository history. To read one, show it from the commit
before they were removed:

```sh
git show c2a0867f2^:DEVELOPMENT_PLAN.md
git show c2a0867f2^:benchmarks/reports/<report>.md
```

## Code style

The solver core is C++20 without exceptions or RTTI, written in a C-like
style; match the surrounding code. Comments state invariants and point to
references; history belongs in commit messages.

## License

By contributing you agree that your contribution is licensed under the MIT
License of this repository.
