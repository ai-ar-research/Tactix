# Getting started

This guide builds Tactix, solves a formula, and checks the answer.

## Requirements

- Linux and a C++20 compiler (tested with GCC 13).
- CMake 3.25 or newer.
- zlib (development headers), for `.gz` input.
- `xz` on the `PATH`, for `.xz` input.
- Python 3.9+, for the proof-checking script.

## Build

```sh
git clone https://github.com/ai-ar-research/Tactix.git
cd Tactix
cmake --preset release
cmake --build --preset release --parallel
```

This produces `build/release/tactix`, the command-line solver, and the example
strategies from `examples/`. The `debug` preset builds with AddressSanitizer
and UndefinedBehaviorSanitizer into `build/debug/`.

## Solve a formula

```sh
build/release/tactix formula.cnf proof
```

The input is DIMACS CNF, optionally compressed as `.gz` or `.xz`. The output
follows the SAT Competition conventions:

- `s SATISFIABLE` followed by `v` lines with a model, exit code 10;
- `s UNSATISFIABLE`, exit code 20, with the proof written to `proof`;
- `s UNKNOWN`, exit code 0, if the solver was interrupted (SIGINT, SIGTERM or
  SIGXCPU).

Lines starting with `c` are statistics. `-q` suppresses most of them. Omit the
proof path to solve without writing a proof.

## Proof formats

By default (`--auto-proof`), Tactix writes its native **DSR** format in binary.
DSR extends DRAT with substitution redundancy, which lets symmetry breaking and
variable merging be stated as proof steps. A few complete refutations found
before search (parity and counting arguments) are written instead as a
**pseudo-Boolean** proof for VeriPB. On UNSAT, the solver prints which checker
route applies:

| `c proof-route` | Format | Checked by |
|---|---|---|
| `dsr-cake_lpr` | DSR without substitution witnesses, which is valid DRAT | drat-trim, then CakeLPR |
| `dsr-trestle` | DSR with substitution witnesses | dsr-trim, then Trestle |
| `pbp` | pseudo-Boolean proof | VeriPB, then CakePB |

Other formats can be requested explicitly: `--drat` (DRAT), `--lrat` (LRAT
with hints), `--dsr` (always DSR). `--no-binary` writes the textual variant.

## Check a proof

Build the checkers once. This clones drat-trim, cake_lpr, dsr-trim and Trestle
at pinned commits next to the source tree and links them into
`tools/checkers/bin/`. Building Trestle needs Lean's `lake`.

```sh
tools/checkers/setup.sh
```

Then check an UNSAT answer against the original formula:

```sh
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

The script reads the proof, picks the route from the table above, and prints
`s VERIFIED UNSAT` with exit code 0, or the failing stage and checker output
with exit code 1. For a `pbp` route, install
[VeriPB](https://gitlab.com/MIAOresearch/software/VeriPB) and
[CakePB](https://gitlab.com/MIAOresearch/software/cakepb)
and pass them with `--veripb` and `--cake-pb`; `paper/run/setup.sh checkers`
builds both at the versions the paper used, into `paper/run/tools/bin/`.

To check a SAT answer, save the solver output and run the model checker,
which confirms that the `v` lines satisfy every clause of the formula:

```sh
build/release/tactix formula.cnf > out.txt
python3 tools/fuzz/checkmodel formula.cnf out.txt
```

`examples/cnf/` has small formulas to try, one for each answer and proof
route; its README lists the expected results.

## Use Tactix as a library

`include/tactix.h` is the C interface. A minimal client that loads a formula
and runs the built-in default strategy:

```c
#include "tactix.h"
#include <stdio.h>

int main(int argc, char **argv) {
  tactix *solver = tactix_init();
  tactix_proof(solver, "proof", TACTIX_PROOF_AUTOMATIC); /* optional */
  FILE *in = fopen(argv[1], "r");
  if (!in || tactix_read_dimacs(solver, in) != 0) return 1;
  fclose(in);
  int result = tactix_solve(solver); /* 10 SAT, 20 UNSAT */
  if (result == 10)
    printf("x1 is %s\n", tactix_value(solver, 1) > 0 ? "true" : "false");
  tactix_release(solver);
  return result;
}
```

Instead of `tactix_solve`, a client can run its own strategy: ask which rules
apply with `tactix_rules_applicable`, list a rule's choices with
`tactix_rule_instances`, apply one with `tactix_apply_instance`, and branch
with `tactix_decide`. The README's three example strategies are complete
programs written this way, and `examples/` has smaller ones, starting from
plain DPLL (`demo1_dpll.c`). Every strategy, including your own, produces
checkable proofs.

To build against Tactix, add the repository as a CMake subdirectory and link
`tactix_lib`, as `examples/CMakeLists.txt` does.

## Run the tests

```sh
ctest --preset debug
```
