# Getting started

This guide builds Tactix, solves a formula, and checks the answer.

## Requirements

- Linux and a C++20 compiler (tested with GCC 13).
- CMake 3.25 or newer.
- zlib (development headers).
- `gzip` and `xz` on the `PATH`, for `.gz` and `.xz` input.
- Python 3.9+, for the proof-checking script.
- x86-64 for the pinned VeriPB and verified CakePB binaries installed by the
  checker setup (the solver itself is not restricted to x86-64).

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

By default (with no proof-format flag), Tactix writes a whole-run
**pseudo-Boolean proof (PBP)**. VeriPB elaborates it and CakePB independently
checks the result against the original CNF. On UNSAT, the solver prints which
checker route applies:

| `c proof-route` | Format | Checked by |
|---|---|---|
| `dsr-cake_lpr` | DSR without substitution witnesses: DRAT in content, converted to DRAT for checking | drat-trim, then CakeLPR |
| `dsr-trestle` | DSR with substitution witnesses | dsr-trim, then Trestle |
| `pbp` | pseudo-Boolean proof | VeriPB, then CakePB |

`--pbp` is the explicit spelling of the default. Other formats can be requested
explicitly: `--dsr` (native DSR, which extends DRAT with substitution
redundancy), `--drat`, and `--lrat` (with hints). `--auto-proof` retains the
former behavior: binary DSR unless a complete startup refutation selects PBP.
With `--dsr` or `--drat`, `--no-binary` writes the textual variant. PBP and
LRAT are already textual, so the option has no effect on those formats.

## Check a proof

Build the checkers once. This builds the clausal checkers plus the pinned
VeriPB and CakePB used by the default PBP route. Building Trestle needs Lean's
`lake`.

```sh
tools/checkers/setup.sh
```

Then check an UNSAT answer against the original formula:

```sh
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

The script reads the proof, picks the route from the table above, and prints
`s VERIFIED UNSAT` with exit code 0, or the failing stage and checker output
with exit code 1. The setup command installs pinned VeriPB and CakePB in
`tools/checkers/bin/`, the proof oracle's default PBP-tool location.
Alternative pinned binaries can be selected explicitly with `--veripb` and
`--cake-pb`.

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
  tactix_proof(solver, "proof", TACTIX_PROOF_PBP); /* optional */
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

To build against Tactix, add the repository to your CMake project and link
`tactix_lib`, which carries the include path of `tactix.h`:

```cmake
add_subdirectory(path/to/tactix tactix)   # builds the solver and library only
add_executable(my_strategy my_strategy.c)
target_link_libraries(my_strategy PRIVATE tactix_lib)
```

`cmake --install build/release --prefix DIR` installs the `tactix` binary, the
library `libtactix.a` and `tactix.h`, with a CMake package and a pkg-config
file; [install.md](install.md) shows how to use them.

## Run the tests

```sh
ctest --preset debug
```
