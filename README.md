# Tactix — A Programmable Strategy Interface for Flexible SAT Solving

We are exploring a paradigm shift from *solvers as black boxes* to *solvers as libraries*. The goal is to unleash the full potential of the rich, growing repertoire of techniques that have been actively developed for decades. To this end, we introduce Tactix, an inprocessing CDCL SAT solver with a public strategy interface that enables users to define custom solving strategies for orchestrating search and inprocessing rules.

Tactix incorporates a wide range of techniques from existing SAT solvers and exposes them through a public interface where the user can define specialized solving strategies by writing a program that iteratively selects particular techniques, observes their effects, and decides the next strategy to apply.

Tactix binary also comes with a default strategy that achieves competitive performance against state-of-the-art solvers.

## Quick start

```sh
cmake --preset release
cmake --build --preset release --parallel
build/release/tactix formula.cnf             # s SATISFIABLE / s UNSATISFIABLE
```

See [docs/cli.md](docs/cli.md) for requirements and checking answers, and
[docs/strategies.md](docs/strategies.md) for writing your own strategy.

To generate an UNSAT proof, supply an output file:

```sh
build/release/tactix formula.cnf proof.pbp
```

Tactix generates pseudo-Boolean proofs by default on Linux and macOS.
Building Tactix, solving formulas, and generating proofs do not require external
proof checkers.

<details>
<summary>Optional: install proof checkers and verify a proof</summary>

```sh
tools/checkers/setup.sh                     # install the external checker toolchain
python3 tools/bench/proof_oracle.py check formula.cnf proof.pbp
```

The current setup script targets Linux x86-64 for the complete checker
toolchain. Some individual checkers support other platforms, but the script
does not provide a complete native macOS installation. Proofs generated on
macOS can be checked on a supported platform together with the original CNF.
See [the checking guide](docs/cli.md#optional-proof-checking) for details.

</details>

## Example strategies

| Program | Source | Strategy |
|---|---|---|
| `tactix-public` | `tests/tactix_public_main.cpp` | The default strategy with every operation issued through `tactix.h`; it makes the same decisions and writes the same proofs as `tactix`. |
| `tactix-public-random` | `tools/fuzz/public_random_main.cpp` | The sole random qualification strategy; it generates varied one-action-at-a-time trajectories exclusively through `tactix.h`. |
| `tactix-cruxmiter-policy` | `examples/cruxmiter_policy.c` | A strategy developed for a set of multiplier-miter benchmarks: four bounded congruence/substitution rounds, Backbone, then the fixed stable default search with vivification as its only inprocessing. |
| `tactix-chvatal-policy` | `examples/chvatal_policy.c` | A strategy developed for tackling sub-problems of the Chvátal's conjectures: merge duplicated totalizer counters with bidirectional substitution-redundancy steps, collapse after each round, then run the default strategy with a DSR proof. |

```sh
build/release/tests/tactix-public formula.cnf proof
build/release/tests/tactix-public-random --random-policy=7 formula.cnf proof
build/release/examples/tactix-cruxmiter-policy formula.cnf proof
build/release/examples/tactix-chvatal-policy formula.cnf proof
```

`examples/` also holds smaller teaching strategies (plain DPLL, CDCL without
restarts, the default schedule re-implemented outside the library) and
strategies for binarized-neural-network and random 3-SAT encodings.

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

`include/tactix.h` is the public C interface; `src/` contains the solver.
`examples/` contains client strategies, while `tests/` and `tools/` contain
the test, checker, fuzzing and benchmark infrastructure. Frozen datasets live
under `benchmarks/`; guides under `docs/` (developer references in
`docs/dev/`); vendored code under `third_party/`.

## License

MIT; see [LICENSE](LICENSE) and [NOTICE](NOTICE) for third-party credits.
