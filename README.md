# Tactix — A Programmable Strategy Interface for Flexible SAT Solving

Tactix is an inprocessing CDCL SAT solver that treats the solver as a library.
It collects decades of SAT techniques as proof-producing rules behind a
**public strategy interface**, so humans and AI can write programs that select
techniques, observe their effects and decide what to apply next. Its built-in
default strategy is competitive with state-of-the-art solvers.

## Quick start

```sh
cmake --preset release
cmake --build --preset release --parallel 8
build/release/tactix formula.cnf             # s SATISFIABLE / s UNSATISFIABLE
build/release/tactix formula.cnf proof       # also write an UNSAT proof
```

See [docs/cli.md](docs/cli.md) for requirements and checking answers, and
[docs/strategies.md](docs/strategies.md) for writing your own strategy.
By default Tactix writes a DSR proof and switches to pseudo-Boolean (PBP) proof
when algebraic reasoning needs it; `--dsr` or `--pbp` fixes the format (see
[proof generation](docs/cli.md#generate-a-proof)). Building Tactix, solving
formulas, and generating proofs do not require external proof checkers.

<details>
<summary>Optional: install proof checkers and verify a proof</summary>

```sh
tools/checkers/setup.sh                     # install the external checker toolchain
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

The checker toolchain targets Linux x86-64; see
[the checking guide](docs/cli.md#optional-proof-checking) for other platforms.

</details>

## Example strategies

| Program | Source | Strategy |
|---|---|---|
| `tactix-default` | `examples/default_strategy/` | The default strategy in C over `tactix.h`; it makes the same decisions and writes the same proofs as the built-in strategy (`--reference` runs that one instead). |
| `tactix-public-random` | `tools/fuzz/public_random_main.cpp` | The random qualification strategy: varied one-action-at-a-time trajectories through `tactix.h`. |
| `tactix-cruxmiter-policy` | `examples/cruxmiter_policy.c` | Multiplier miters: four bounded congruence/substitution rounds, Backbone, then stable default search with vivification as its only inprocessing. |
| `tactix-chvatal-policy` | `examples/chvatal_policy.c` | Sub-problems of Chvátal's conjectures: merge duplicated totalizer counters with bidirectional substitution-redundancy steps, then run the default strategy with a DSR proof. |

Each runs as `build/release/examples/<program> formula.cnf proof`;
`tactix-public-random` is built in `build/release/tests/` and takes
`--random-policy=SEED`.

`examples/` also holds teaching strategies (DPLL, CDCL without restarts, the
demo3 search scheduler) and an extended-resolution strategy for pigeonhole
formulas. The experimental [Python API](python/README.md) adds IPASIR sessions,
runtime strategies and certified cores.

## Documentation and paper

[docs/README.md](docs/README.md) indexes the guides. The
[Tactix paper](https://arxiv.org/abs/2610.24369) describes the solver and its
programmable strategy interface. The
[artifact branch](https://github.com/ai-ar-research/Tactix/tree/artifact/paper)
contains the logs, figures, container image, benchmarks and experiment runner.
User-visible release changes are listed in [CHANGELOG.md](CHANGELOG.md).

```bibtex
@misc{wu2026tactixprogrammablestrategyinterface,
  title={Tactix: Towards a Programmable Strategy Interface for Flexible SAT Solving},
  author={Haoze Wu and André Schidler and Armin Biere and Nina Narodytska},
  year={2026},
  eprint={2610.24369},
  archivePrefix={arXiv},
  primaryClass={cs.LO},
  url={https://arxiv.org/abs/2610.24369}
}
```

## Layout

`include/` holds the public C interfaces, `src/` the solver, `examples/` client
strategies, `tests/` and `tools/` the test, checker, fuzzing and benchmark
infrastructure, `benchmarks/` frozen datasets, `docs/` user guides and
`third_party/` vendored code.

## License

MIT; see [LICENSE](LICENSE) and [NOTICE](NOTICE) for third-party credits.
