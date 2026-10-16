# Contributing to Tactix

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
