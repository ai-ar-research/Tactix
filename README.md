# Tactix — A Programmable Strategy Interface for Flexible SAT Solving

Tactix is a CDCL SAT solver whose solving strategy is a program you can
replace. Every step the solver takes — propagating, deciding, analyzing a
conflict, restarting, and each inprocessing technique such as variable
elimination, vivification or symmetry breaking — is a proof rule exposed
through a C interface. A strategy asks which rules apply, chooses one and its
settings, and applies it. Whatever the strategy does, every UNSAT answer comes
with a machine-checkable proof and every SAT answer with a model.

- **A strong default.** The built-in strategy reimplements the search and
  inprocessing schedules of Kissat and CaDiCaL and adds symmetry breaking and
  structure-aware techniques (GF(2) reasoning, cardinality and parity
  arguments).
- **Your own strategy.** Strategies written against `include/tactix.h` drive
  the solver one operation at a time, and each invocation of a rule can use
  different settings. The default strategy itself can run this way.
- **Checked answers.** UNSAT proofs are written in DSR (DRAT extended with
  substitution redundancy) or, for a few refutations found before search, as
  pseudo-Boolean proofs, and are checked with formally verified checkers
  (CakeLPR, Trestle, CakePB).

## Quick start

```sh
cmake --preset release
cmake --build --preset release --parallel
build/release/tactix formula.cnf proof       # s SATISFIABLE / s UNSATISFIABLE
tools/checkers/setup.sh                       # once: build the proof checkers
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

See [docs/getting-started.md](docs/getting-started.md) for requirements,
proof formats, checking, and using Tactix as a library.

## Example strategies

Three strategies show what the C interface can express. Each is a standalone
program built with the solver and produces checkable proofs.

| Program | Source | Strategy |
|---|---|---|
| `tactix-public` | `tests/tactix_public_main.cpp` | The default strategy with every operation issued through `tactix.h`; it makes the same decisions and writes the same proofs as `tactix`. |
| `tactix-cruxmiter-policy` | `examples/cruxmiter_policy.c` | For multiplier-equivalence miters (`cruxmiter`): alternates congruence closure and equivalent-literal substitution while they make progress, then runs CDCL search with vivification as its only inprocessing. |
| `tactix-chvatal-policy` | `examples/chvatal_policy.c` | For Chvátal-conjecture subproblems: finds counter variables that the formula's totalizers duplicate, merges them with substitution-redundancy steps, then runs the default strategy. |

```sh
build/release/tests/tactix-public formula.cnf proof
build/release/examples/tactix-cruxmiter-policy --drat formula.cnf proof
build/release/examples/tactix-chvatal-policy formula.cnf proof
```

`examples/` also holds smaller teaching strategies (plain DPLL, CDCL without
restarts, the default schedule re-implemented outside the library) and
strategies for binarized-neural-network and random 3-SAT encodings.

## Layout

```
include/tactix.h  the public C interface
src/              solver kernel, proof rules, default strategy, proof output
examples/         strategies written against the C interface
tests/            unit, API, and proof tests (ctest)
tools/checkers/   proof-checker setup and patches
tools/fuzz/       fuzzing harness with proof and model oracles
tools/bench/      benchmark runner and proof-checking script
third_party/      DejaVu (graph automorphisms for symmetry breaking)
```

## License

MIT; see [LICENSE](LICENSE) and [NOTICE](NOTICE) for third-party credits.
