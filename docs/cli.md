# Running Tactix

## Build

Tactix requires a C++20 compiler (GCC 13 or newer, Clang 18 or newer, or Apple
Clang 15 or newer),
CMake 3.25 or newer and zlib.

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

## Generate a proof

Give a second file name to write an UNSAT proof. Proof generation works on
Linux and macOS and requires no external checker installation:

```sh
build/release/tactix formula.cnf proof
```

By default Tactix writes a binary DSR proof. It converts that proof to
pseudo-Boolean (PBP) once and continues in PBP only when a startup algebraic
argument refutes the formula, once parity reasoning has cost 10 MB of DSR, or at
a step DSR cannot follow. The output names the result, for example
`c proof-route dsr-cake_lpr`. Use `--pbp` to always write PBP and `--dsr` to always
write DSR. `--dsr` stops with an error on a startup counting refutation, which has
no practical DSR spelling; the default and `--pbp` handle it.

The search is the same with no proof and in every format, so the choice affects
only what is written and how long it takes to check.

## Optional proof checking

Independent checking is a separate step. It requires Python 3.9 or newer and
the external checkers for the generated proof format.

The complete toolchain installed by `tools/checkers/setup.sh` currently targets
Linux x86-64: the PBP route uses VeriPB and CakePB, and the script
pins a Linux x86-64 VeriPB binary and requires x86-64 for CakePB. Some individual
checkers support other platforms, but the script does not provide a complete
native macOS installation. To check a proof generated on macOS, transfer the
proof and the original CNF to a supported platform.

Build the checkers once (this downloads and builds them into
`tools/checkers/bin/`):

```sh
tools/checkers/setup.sh
```

Then check the proof against the original formula:

```sh
python3 tools/bench/proof_oracle.py check formula.cnf proof
```

The checker prints `s VERIFIED UNSAT` when it accepts the proof. The script
recognizes the proof's format and runs the matching checkers: VeriPB and
CakePB for PBP; drat-trim and CakeLPR, or dsr-trim and Trestle, for DSR.
`tools/checkers/setup.sh` builds each checker at a pinned commit with the patches in
`tools/checkers/patches/`. The drat-trim patch finds RAT candidates by occurrence and
produces the same LRAT as upstream.

For DSR-to-DRAT conversion, the optional native `dsr-transport` helper is built
only on Linux; other platforms use the Python conversion fallback. Conversion
alone does not verify a proof.

## Check a satisfying assignment

Save the output and check that the assignment satisfies every clause. This
Python check does not require the external proof-checker toolchain:

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

[strategies.md](strategies.md) shows how to write and compile your own. The
command-line help (`tactix --help`) lists the available solver options.
