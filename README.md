# Tactix — A Programmable Strategy Interface for Flexible SAT Solving

Tactix is a CDCL SAT solver whose strategy you can replace. Propagation,
decisions, conflict analysis, restarts and inprocessing rules are exposed
through a C interface. A strategy chooses applicable rules and their settings.
Every UNSAT answer has a machine-checkable proof; every SAT answer has a model.

- **A strong default strategy.** It reimplements Kissat and CaDiCaL scheduling
  and adds symmetry breaking and structure-aware reasoning.
- **Your own strategy.** Strategies written against `include/tactix.h` drive
  operations one at a time with per-invocation settings; the default can too.
- **Checked answers.** UNSAT proofs are written as whole-run pseudo-Boolean
  proofs by default, elaborated by VeriPB and then checked by CakePB. Native
  DSR (DRAT extended with substitution redundancy) remains available with
  `--dsr`; `--auto-proof` retains the earlier startup-PBP/DSR route.

## Quick start

```sh
cmake --preset release
cmake --build --preset release --parallel
build/release/tactix formula.cnf proof       # s SATISFIABLE / s UNSATISFIABLE
tools/checkers/setup.sh                       # once: build the proof checkers
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

See [getting started](docs/getting-started.md) for requirements, proof checks
and library use, and [installation](docs/install.md) for release binaries.

## Example strategies

| Program | Source | Strategy |
|---|---|---|
| `tactix-public` | `tests/tactix_public_main.cpp` | The default strategy with every operation issued through `tactix.h`; it makes the same decisions and writes the same proofs as `tactix`. |
| `tactix-public-random` | `tools/fuzz/public_random_main.cpp` | The sole random qualification strategy; it generates varied one-action-at-a-time trajectories exclusively through `tactix.h`. |
| `tactix-cruxmiter-policy` | `examples/cruxmiter_policy.c` | For multiplier-equivalence miters (`cruxmiter`): alternates congruence closure and equivalent-literal substitution while they make progress, then runs CDCL search with vivification as its only inprocessing. |
| `tactix-chvatal-policy` | `examples/chvatal_policy.c` | For Chvátal-conjecture subproblems: finds counter variables that the formula's totalizers duplicate, merges them with substitution-redundancy steps, then runs the default strategy. |

```sh
build/release/tests/tactix-public formula.cnf proof
build/release/tests/tactix-public-random --random-policy=7 formula.cnf proof
build/release/examples/tactix-cruxmiter-policy --drat formula.cnf proof
build/release/examples/tactix-chvatal-policy formula.cnf proof
```

`examples/` also holds smaller teaching strategies (plain DPLL, CDCL without
restarts, the default schedule re-implemented outside the library) and
strategies for binarized-neural-network and random 3-SAT encodings.

## Documentation and paper

[docs/README.md](docs/README.md) indexes the guides. The
[paper manuscript](https://github.com/wu-haoze/Tactix-paper) is maintained
separately; the [artifact branch](https://github.com/ai-ar-research/Tactix/tree/artifact/paper)
contains the logs, figures, container image, benchmarks and experiment runner.
User-visible release changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Layout

```
include/tactix.h  the public C interface
src/              solver kernel, proof rules, default strategy, proof output
examples/         strategies written against the C interface
tests/            unit, API, and proof tests (ctest)
tools/checkers/   proof-checker setup and patches
tools/fuzz/       fuzzing harness with proof and model oracles
tools/bench/      benchmark runner and proof-checking script
benchmarks/       frozen tiers, reference data, campaigns and reports
docs/             user and developer documentation
third_party/      DejaVu (graph automorphisms for symmetry breaking)
```

## License

MIT; see [LICENSE](LICENSE) and [NOTICE](NOTICE) for third-party credits.
