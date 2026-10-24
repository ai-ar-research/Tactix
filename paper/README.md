# Paper evaluation: logs, runner and analysis

This directory reproduces the evaluation of *Tactix: Towards a Programmable
Strategy Interface for Flexible SAT Solving*. The TACAS 2027 artifact wraps it
in a Docker image (Ubuntu 26.04) and one-command scripts (`install.sh`,
`smoke-test.sh`, `run-experiments.sh`, `regenerate-from-logs.sh`); its README
is the guide for artifact evaluation. This file documents the pieces.

```
paper/
  logs/          archived campaigns of every experiment (raw/*.tar.xz, with each
                 run's command, limits, outputs and checker results) and one
                 normalized results.csv per experiment
  data/          the CSV files the paper's tables and figures are drawn from
  scripts/       normalize.py      raw campaigns -> logs/*/results.csv (merge rules)
                 make_data.py      results.csv -> data/*.csv and limits.json
                 report.py         prints every table of the paper from data/
                 compare.py        a rerun next to the paper's logs on the same runs
                 plot_*.py, table_chvatal.py   the figures and Table 5's LaTeX
                 analyze_milestone_activity.py Table 6's activity columns (git history)
  benchmarks/    instance definitions: GBD content hashes of SC 2024-2026 and the
                 interface sets, Tier A, the cruxmiter and Chvatal formulas, and
                 the reduced and smoke subsets (subsets/, chosen by run/select_subsets.py)
  run/           experiments.py   every experiment: instances, limits, the command of
                                  each configuration, profiles full/reduced/smoke
                 bench.py         runs an experiment locally or as a SLURM array
                 setup.sh         builds checkers, reference solvers and the Tactix
                                  builds the paper evaluated (BUILDS)
  Makefile       logs -> data -> figures and printed tables
```

## Analysis

```sh
make -C paper check     # rebuild data/ from the raw logs; every file must be identical
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

How each answer is checked: a SAT model against the formula
(`tools/fuzz/checkmodel`); an UNSAT certificate by the pipeline of its format:
DRAT with drat-trim (to LRAT) and cake_lpr; DSR with dsr-trim (to LSR) and
Trestle; PBP with VeriPB (elaboration) and CakePB. Tactix's certificates are
routed by content (`tools/bench/proof_oracle.py`).

## Notes on the archived runs

- **Which runs count.** Where an archived campaign was rechecked with more
  memory or a longer checking window, the merge rule in
  `scripts/normalize.py` takes the recheck only when it verifies (Tier D) or
  only after a resource failure (Chvátal). One Chvátal campaign carries a
  `CORRECTNESS-BUG` note about two CakeLPR crashes; the recheck campaigns
  verified both certificates, and the paper uses the rechecked outcome.
- **Commits.** Each Tactix configuration was built from a commit of this
  repository; `BUILDS` in `run/experiments.py` lists them by full hash, and all
  of them are in this branch's history.
- **Cruxmiter settings.** The cruxmiter strategy (`examples/cruxmiter_policy.c`)
  drives its opening through `tactix.h`, then hands over to the built-in stable
  default strategy with reordering, BVE, conflict reuse, Factor, Sweep and the
  default Backbone slot disabled and Vivify as its only inprocessing slot.
  These fixed settings are part of the strategy implementation.
- **Reference solvers** are built from their SAT Competition sources
  (`tools/solvers/fetch-winners.sh` records the URLs and archive hashes).
  LymphoSAT's competition binaries require an Ice Lake CPU; it is rebuilt for
  generic x86-64 without changing its algorithms (`run/build_lymphosat.py`).
- **Checkers.** drat-trim, CakeLPR, dsr-trim, Trestle, VeriPB and CakePB at
  pinned commits (`run/setup.sh`). Satsuma's DSR certificates are checked like
  Tactix's: dsr-trim, then `dsr-compress -d`, then Trestle.
