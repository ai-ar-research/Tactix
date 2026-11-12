# Running Tactix

## Build

Tactix requires a C++20 compiler (GCC 13+, Clang 18+ or Apple Clang 15+),
CMake 3.25+ and zlib.

```sh
git clone https://github.com/ai-ar-research/Tactix.git
cd Tactix
cmake --preset release
cmake --build --preset release --parallel 8
```

This builds the solver, `build/release/tactix`, and the example strategies in
`build/release/examples/`. Replace `8` with the maximum number of concurrent
build jobs appropriate for your machine.

## Solve a formula

```sh
build/release/tactix formula.cnf
```

The input is DIMACS CNF; files ending in `.gz` or `.xz` are decompressed on the
fly. The answer follows the SAT Competition format:

| Output | Exit code | Meaning |
|---|---|---|
| `s SATISFIABLE`, then `v` lines | 10 | satisfiable; the `v` lines give a satisfying assignment |
| `s UNSATISFIABLE` | 20 | unsatisfiable |
| `s UNKNOWN` | 0 | stopped before an answer |

Lines starting with `c` are statistics; `-q` hides most of them. Tactix has
no time limit of its own: use `timeout 600 build/release/tactix ...`, which
stops it with `s UNKNOWN`.

## Generate a proof

Give a second file name to write an UNSAT proof. Proof generation works on
Linux and macOS without external checkers:

```sh
build/release/tactix formula.cnf proof
```

By default Tactix writes a binary DSR proof. It converts that proof to
pseudo-Boolean (PBP) once and continues in PBP only when a startup algebraic
argument refutes the formula, once parity reasoning has cost 10 MB of DSR, or at
a step DSR cannot follow. The output names the result, for example
`c proof-route dsr-cake_lpr`. Use `--pbp` to always write PBP and `--dsr` to always
write DSR. A startup counting refutation has no practical DSR spelling: under
`--dsr`, a second solve of the original formula without that refutation writes the
DSR proof and prints its statistics on `c follow-on` lines. If that solve is
interrupted, the run reports no verdict.

The search is the same with no proof and in every format; the choice affects
what is written, its checking time and, under `--dsr`, the follow-on's run time.

## Optional proof checking

Independent checking requires Python 3.9+ and the external checkers. Build
them once into `tools/checkers/bin/`, then check the proof against the original
formula:

```sh
tools/checkers/setup.sh
python3 tools/bench/proof_oracle.py check formula.cnf proof   # s VERIFIED UNSAT
```

The script recognizes the proof format and runs VeriPB and CakePB for PBP, or
drat-trim and CakeLPR or dsr-trim and Trestle for DSR. `setup.sh` pins each
checker's version and applies `tools/checkers/patches/`; the drat-trim patch
finds RAT candidates by occurrence and produces the same LRAT as upstream.

The toolchain targets Linux x86-64 (its VeriPB binary and CakePB require it);
check a proof made on macOS by copying it and the original CNF to such a
machine. The native `dsr-transport` helper for DSR-to-DRAT conversion is built
only on Linux; other platforms use the Python fallback. Conversion alone does
not verify a proof.

## Check a satisfying assignment

Check the saved assignment against every clause; this needs no external checkers:

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

The example strategies in `build/release/examples/` run the same way but read
uncompressed DIMACS only. [strategies.md](strategies.md) shows how to write
your own; `tactix --help` lists the solver options.
