# Example formulas

The original three examples take different paths and solve in a few seconds.
The case-study fixtures below are harder and belong in cluster regressions.

| File | Answer | What it shows |
|---|---|---|
| `random3sat-sat.cnf` | SAT | A model on `v` lines; check it with `tools/fuzz/checkmodel`. |
| `pigeonhole-9-8.cnf` | UNSAT | Hard for plain resolution. Symmetry breaking closes it at once; `--dsr` demonstrates native substitution-redundancy steps (route `dsr-trestle`). |
| `tseitin-40.cnf` | UNSAT | A parity contradiction refuted before search; the proof is pseudo-Boolean (route `pbp`). |

`generate.py` regenerates the original three examples. Small PHP instances
in `php/` cover five AMO encodings. `cruxmiter32seed0.cnf` and
`chvatal-n7-1111001.cnf.xz` are actual case-study benchmarks copied from the
artifact branch. `fixtures.json` records source revisions, hashes and saved
independently checked verdicts. They are not part of the fast unit gate.
Use `xz -dc` to expand the Chvátal fixture before solving. No additional
benchmark generators are included.

## Try them

From the repository root, after building (see `docs/cli.md`):

```sh
build/release/tactix examples/cnf/random3sat-sat.cnf > out.txt
python3 tools/fuzz/checkmodel examples/cnf/random3sat-sat.cnf out.txt

build/release/tactix --dsr examples/cnf/pigeonhole-9-8.cnf proof
python3 tools/bench/proof_oracle.py check examples/cnf/pigeonhole-9-8.cnf proof
```
