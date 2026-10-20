# Documentation

## Using Tactix

| Document | Contents |
|---|---|
| [getting-started.md](getting-started.md) | Build, solve a formula, check its proof, call Tactix from C |
| [install.md](install.md) | Release binaries, and installing Tactix as a library (CMake, pkg-config) |
| [cli.md](cli.md) | Every command-line option, the output format, the example strategy programs |
| [c-api.md](c-api.md) | The C interface: writing your own strategy, per-invocation rule settings, a tutorial |
| [rules.md](rules.md) | Every proof rule and policy operation a strategy can apply |
| [proof-rules.html](proof-rules.html) | Interactive guide to the proof rules (open in a browser) |
| [../examples/](../examples/) | Example strategies and small test formulas |
| [paper artifact](https://github.com/ai-ar-research/Tactix/tree/artifact/paper) | Reproducing the paper's evaluation |

## How Tactix works

| Document | Contents |
|---|---|
| [architecture.md](architecture.md) | Kernel, rules, policies, proof backend, and the source tree |
| [calculus.md](calculus.md) | Formal semantics of each rule: preconditions, effects, proof steps |
| [implementation-notes.md](implementation-notes.md) | Guards, deviations from the calculus, and design decisions in the implementation |
| [controller-interface.md](controller-interface.md) | The contract of the online configuration interface used by external controllers |
| [scheduling-parity-ledger.md](scheduling-parity-ledger.md) | How the default strategy's scheduling follows Kissat and CaDiCaL, mechanism by mechanism |
| [algebraic-reasoning-plan.md](algebraic-reasoning-plan.md) | CDCL-time GF(2), GF(3) and cardinality integration gates and current proof/performance evidence |
| [algebraic-proof-bundle.md](algebraic-proof-bundle.md) | One public certificate bundle for batched algebraic lemmas and the canonical DSR search proof |

The development reference documents use
some internal terminology and cross-reference each other.

## Contributing

See [../CONTRIBUTING.md](../CONTRIBUTING.md).
