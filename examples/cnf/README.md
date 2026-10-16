# Example formulas

Small formulas that each take a different path through the solver. All solve
in a few seconds.

| File | Answer | What it shows |
|---|---|---|
| `random3sat-sat.cnf` | SAT | A model on `v` lines; check it with `tools/fuzz/checkmodel`. |
| `pigeonhole-9-8.cnf` | UNSAT | Hard for plain resolution. Symmetry breaking closes it at once; the proof uses substitution-redundancy steps (route `dsr-trestle`). |
| `tseitin-40.cnf` | UNSAT | A parity contradiction refuted before search; the proof is pseudo-Boolean (route `pbp`). |
| `cruxmiter20seed0.cnf` | UNSAT | A small multiplier-equivalence miter from the `cruxmiter` family; input for `tactix-cruxmiter-policy`. |
| `chvatal-6.cnf` | UNSAT | A Chvátal-conjecture formula (case n = 6) with duplicated totalizers; input for `tactix-chvatal-policy`. |

`generate.py` regenerates the first three files. The last two are instances
of the families studied in the paper:

- `cruxmiter20seed0.cnf` belongs to the `cruxmiter` multiplier-verification
  family introduced in D. Kaufmann, M. Kauers, A. Biere, D. Cok, *Arithmetic
  Verification Problems Submitted to the SAT Race 2019*, building on
  D. Ritirc, A. Biere, M. Kauers, *Column-Wise Verification of Multipliers
  Using Computer Algebra*, FMCAD 2017.
- `chvatal-6.cnf` is a Chvátal-conjecture formula from the encoding of
  J. Looney et al., *Solution Space Partitioning for Extremal Set Theory*,
  FMCAD 2026.

The cruxmiter and Chvátal formulas of the paper's experiments are in
`paper/benchmarks/`, from the same sources.

## Try them

From the repository root, after building (see `docs/getting-started.md`):

```sh
build/release/tactix examples/cnf/random3sat-sat.cnf > out.txt
python3 tools/fuzz/checkmodel examples/cnf/random3sat-sat.cnf out.txt

build/release/tactix examples/cnf/pigeonhole-9-8.cnf proof
python3 tools/bench/proof_oracle.py check examples/cnf/pigeonhole-9-8.cnf proof

build/release/examples/tactix-cruxmiter-policy --drat examples/cnf/cruxmiter20seed0.cnf proof
build/release/examples/tactix-chvatal-policy examples/cnf/chvatal-6.cnf proof
```
