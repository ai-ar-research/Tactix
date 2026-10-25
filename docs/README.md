# Documentation

| Document | Contents |
|---|---|
| [cli.md](cli.md) | Build Tactix and run it on your formulas, with or without checking the answers |
| [c-api.md](c-api.md) | An overview of the C interface, `include/tactix.h` |
| [strategies.md](strategies.md) | Example strategies, and how to write, compile and run your own |
| [ai-strategy-prompt.md](ai-strategy-prompt.md) | An example prompt for designing a strategy with an AI agent |
| [../examples/](../examples/) | Example strategies and small formulas |

## For developers

`dev/` holds the detailed references and the notes on how Tactix works:

| Document | Contents |
|---|---|
| [dev/cli-reference.md](dev/cli-reference.md) | Every command-line option, output line and `TACTIX_*` setting |
| [dev/c-api-reference.md](dev/c-api-reference.md) | The complete C interface, with a tutorial |
| [dev/rules.md](dev/rules.md) | Every rule a strategy can apply; [dev/proof-rules.html](dev/proof-rules.html) is an interactive version |
| [dev/install.md](dev/install.md) | Release binaries, and installing the library (CMake, pkg-config) |
| [dev/architecture.md](dev/architecture.md) | The solver's components and the source tree |
| [dev/calculus.md](dev/calculus.md) | The proof rules, formally |
| [dev/implementation-notes.md](dev/implementation-notes.md) | Guards and design decisions in the implementation |
| [dev/scheduling-parity-ledger.md](dev/scheduling-parity-ledger.md) | How the built-in strategy's scheduling follows Kissat and CaDiCaL |
| [dev/controller-interface.md](dev/controller-interface.md) | The interface used by external controllers |
| [dev/algebraic-reasoning-plan.md](dev/algebraic-reasoning-plan.md), [dev/algebraic-proof-bundle.md](dev/algebraic-proof-bundle.md) | Algebraic reasoning and its proofs |

See also [../CONTRIBUTING.md](../CONTRIBUTING.md).
