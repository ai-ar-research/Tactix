# Running Tactix

## Build

Tactix requires a C++20 compiler (GCC 13 or newer), CMake 3.25 or newer
and zlib. Checking proofs also needs Python 3.9 or newer.

```sh
git clone https://github.com/ai-ar-research/Tactix.git
cd Tactix
cmake --preset release
cmake --build --preset release --parallel
```

This builds the solver, `build/release/tactix`, and the example strategies in
`build/release/examples/`.

## Solve a formula

```sh
build/release/tactix formula.cnf
```

The input is a formula in DIMACS CNF, the format of the SAT Competitions;
files ending in `.gz` or `.xz` are decompressed on the fly. The answer is one
line, as in the SAT Competitions:

| Output | Exit code | Meaning |
|---|---|---|
| `s SATISFIABLE`, then `v` lines | 10 | satisfiable; the `v` lines give a satisfying assignment |
| `s UNSATISFIABLE` | 20 | unsatisfiable |
| `s UNKNOWN` | 0 | stopped before an answer |

Lines starting with `c` are statistics; `-q` hides most of them. Tactix has
no time limit of its own: use `timeout 600 build/release/tactix ...`, which
stops it with `s UNKNOWN`.

## Solve and check the answer

Tactix can justify both answers, and independent checkers can confirm them.

Build the checkers once (this downloads and builds them into
`tools/checkers/bin/`):

```sh
tools/checkers/setup.sh
```

**Unsatisfiable:** give a second file name, where Tactix writes a proof, then
check the proof against the formula:

```sh
build/release/tactix formula.cnf proof
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

The checker prints `s VERIFIED UNSAT` when it accepts the proof. Tactix writes
a pseudo-Boolean proof by default; the script recognizes the proof's format
and runs the matching checkers.

**Satisfiable:** save the output and check that the assignment satisfies every
clause:

```sh
build/release/tactix formula.cnf > out.txt
python3 tools/fuzz/checkmodel formula.cnf out.txt
```

`examples/cnf/` has three small formulas to try, with their expected answers.

## A set of formulas

```sh
for f in my-formulas/*.cnf; do
  timeout 600 build/release/tactix -q "$f" proof > out.txt
  case $? in
    10) python3 tools/fuzz/checkmodel "$f" out.txt ;;
    20) python3 tools/bench/proof_oracle.py check "$f" proof ;;
    *)  echo "$f: no answer" ;;
  esac
done
```

## Other strategies

The example strategies run the same way, for example the strategy from the
paper's cruxmiter case study:

```sh
build/release/examples/tactix-cruxmiter-policy formula.cnf proof
```

They read uncompressed DIMACS only.

[strategies.md](strategies.md) shows how to write and compile your own. Every
option, output line and setting of the solver is listed in
[dev/cli-reference.md](dev/cli-reference.md).
