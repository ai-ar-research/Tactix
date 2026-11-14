# Example formulas

Small formulas that each take a different path through the solver. All solve
in a few seconds.

| File | Answer | What it shows |
|---|---|---|
| `random3sat-sat.cnf` | SAT | A model on `v` lines; check it with `tools/fuzz/checkmodel`. |
| `pigeonhole-9-8.cnf` | UNSAT | Hard for plain resolution. Symmetry breaking closes it at once; `--dsr` demonstrates native substitution-redundancy steps (route `dsr-trestle`). |
| `tseitin-40.cnf` | UNSAT | A parity contradiction refuted before search; the proof is pseudo-Boolean (route `pbp`). |

A build with tests enabled (the default for a top-level build; target
`example_cnfs`) writes all three into `build/<preset>/examples/cnf/`.
`python3 examples/cnf/generate.py [DIR]` writes the same bytes into `DIR`, by
default this directory. The
[artifact branch](https://github.com/ai-ar-research/Tactix/tree/artifact/paper/benchmarks)
ships the full Chvátal and cruxmiter case-study sets; main does not include
separate development or reduced copies of those families.

## Try them

From the repository root, after building (see `docs/cli.md`):

```sh
cnf=build/release/examples/cnf
build/release/tactix $cnf/random3sat-sat.cnf > out.txt
python3 tools/fuzz/checkmodel $cnf/random3sat-sat.cnf out.txt

build/release/tactix --dsr $cnf/pigeonhole-9-8.cnf proof
python3 tools/bench/proof_oracle.py check $cnf/pigeonhole-9-8.cnf proof
```
