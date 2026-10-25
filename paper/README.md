# Paper evaluation: logs, runner and analysis

This directory reproduces the evaluation of *Tactix: Towards a Programmable
Strategy Interface for Flexible SAT Solving*. The paper's artifact wraps it
in a Docker image (Ubuntu 26.04) and one-command scripts (`install.sh`,
`smoke-test.sh`, `run-experiments.sh`, `regenerate-from-logs.sh`); its README
is the guide for artifact evaluation. This file documents the pieces.

```
paper/
  logs/          the logs of every experiment: one <configuration>.tar.xz per
                 configuration (each run's record: command, limits, outputs and
                 checker results; and the campaign manifests) and one
                 normalized results.csv per experiment
  data/          the CSV files the paper's tables and figures are drawn from
  scripts/       normalize.py      logs/*/*.tar.xz -> logs/*/results.csv
                 make_data.py      results.csv -> data/*.csv and limits.json
                 report.py         prints every table of the paper from data/
                 compare.py        a rerun next to the paper's logs on the same runs
                 plot_*.py, table_chvatal.py   the figures and Table 5's LaTeX
                 analyze_milestone_activity.py Table 6's activity columns (git history)
  benchmarks/    instance definitions: the SAT Competition 2024-2026 formulas and
                 the interface sets by their Global Benchmark Database (GBD)
                 identifier, a hash of the formula's content (with the GBD family
                 of each competition formula); Tier A, the cruxmiter and Chvatal
                 formulas; the reduced and smoke subsets (subsets/, chosen by
                 run/select_subsets.py)
  run/           experiments.py   every experiment: instances, limits, the command of
                                  each configuration, profiles full/reduced/smoke
                 bench.py         runs an experiment locally or as a SLURM (cluster
                                  scheduler) array job
                 setup.sh         builds checkers, reference solvers and the Tactix
                                  builds the paper evaluated (BUILDS)
  Makefile       logs -> data -> figures and printed tables
```

## Analysis

```sh
make -C paper check     # rebuild data/ from the logs; every file must be identical
make -C paper           # data, figures in paper/figures/, tables printed
make -C paper compare RUN=paper/run/results OUT=/tmp/comparison   # a rerun vs the logs
```

## Running experiments

```sh
paper/run/setup.sh all                                  # build everything
paper/run/bench.py list                                 # experiments and profiles
paper/run/bench.py run tierd --profile reduced --jobs 3 # one experiment
paper/run/bench.py slurm tierd --profile full --partition P   # on a cluster
paper/run/bench.py collect                              # records -> results.csv
```

A profile selects instances and limits; the commands are the same in every
profile. `full` is the paper's setting (sets and limits in
`run/experiments.py`), `reduced` the artifact-evaluation subsets with smaller
limits, `smoke` a few quick instances. Every run is limited in time and in
resident memory (of the solver or checking pipeline and all its children).
Each run writes a JSON record with the command, limits, solver output tail,
and each checker stage's result; an interrupted run resumes.

How each answer is checked: a satisfying assignment against the formula
(`tools/fuzz/checkmodel`); an unsatisfiability proof by the checkers of its
format: DRAT (clausal proofs) with drat-trim and cake_lpr; DSR (DRAT with
substitution steps) with dsr-trim and Trestle; pseudo-Boolean proofs with
VeriPB and CakePB. Tactix's proofs are sent to the right checkers by their
content (`tools/bench/proof_oracle.py`).

## Notes

- **Commits.** Each Tactix configuration was built from a commit of this
  repository; `BUILDS` in `run/experiments.py` lists them by full hash, and all
  of them are in this branch's history.
- **Cruxmiter settings.** The cruxmiter strategy (`examples/cruxmiter_policy.c`)
  drives its opening through `tactix.h`, then hands over to the built-in
  default strategy in stable mode, with reordering, bounded variable
  elimination, conflict reuse, Factor, Sweep and the default Backbone slot
  disabled, and Vivify as its only inprocessing slot.
  These fixed settings are part of the strategy implementation.
- **Reference solvers** are built from their SAT Competition sources
  (`tools/solvers/fetch-winners.sh` records the URLs and archive hashes).
  LymphoSAT's competition binaries require an Ice Lake CPU; it is rebuilt for
  generic x86-64 without changing its algorithms (`run/build_lymphosat.py`).
- **Checkers.** drat-trim, CakeLPR, dsr-trim, Trestle, VeriPB and CakePB at
  pinned commits (`run/setup.sh`). Satsuma's DSR certificates are checked like
  Tactix's: dsr-trim, then `dsr-compress -d`, then Trestle.
