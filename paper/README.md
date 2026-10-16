# Artifact: Tactix — Towards a Programmable Strategy Interface for Flexible SAT-Solving

This folder reproduces the evaluation of the paper. It contains the logs of
every experiment, the scripts that turn them into the paper's tables and
figures, and a runner that repeats the experiments from scratch.

There are three levels of reproduction:

| Level | What it does | Time |
|---|---|---|
| 1. Analysis | Regenerate every table and figure from the archived logs | seconds |
| 2. Smoke test and reduced evaluation | Run the solvers and checkers on small instances and on a reduced instance set of each experiment | ~15 minutes / a few hours |
| 3. Full evaluation | Rerun all experiments | thousands of CPU-hours; needs a cluster |

## Contents

```
paper/
  README.md            this file
  Makefile             analysis: logs -> data -> figures and printed tables
  logs/                archived campaigns of every experiment (raw/*.tar.xz) and
                       one normalized results.csv per experiment
  data/                the CSV files the paper's tables and figures are drawn from
  scripts/             normalize.py, make_data.py, plot and table scripts, report.py
  benchmarks/          instance definitions: GBD hash lists, the Tier A,
                       cruxmiter and Chvátal formulas, reduced subsets
  run/                 setup.sh (build solvers and checkers), bench.py (run experiments)
  Dockerfile           image with everything built
  tactix.def           the same image for Apptainer/Singularity
  install-deps.sh      system packages for Ubuntu 24.04
```

## Claims and where they come from

| Paper | Claim | Data | Produced by |
|---|---|---|---|
| §4.1, Table "Results on the SC'24-26 benchmark sets" | Tactix solves and checks more formulas than Satsuma+Kissat, LymphoSAT and three Kissat versions on every year and overall | `data/tierd-table.csv` | `make report` (Tier D section) |
| §4.1, cactus and scatter figures; appendix scatter figures | per-instance comparison with the references | `data/tierd-times.csv` | `figures/tierd-*.pdf` |
| §4.1, proof statistics | number, size and checking time of Tactix's UNSAT proofs; Tactix vs Satsuma+Kissat ratios | `data/tierd-times.csv` | `make report` |
| §4.2, Figure "interface overhead" | the default strategy through the C interface has a median slowdown of at most a few percent | `data/interface-overhead.csv` | `figures/interface-overhead.pdf`, `make report` |
| §4.3, Table "cruxmiter" | the cruxmiter strategy is faster than default Tactix and Kissat, with smaller proofs | `data/crux-results.csv` | `make report` |
| §4.4, Table and Figure "Chvátal" | the Chvátal strategy halves the solving time of default Tactix | `data/chvatal-results.csv` | `figures/chvatal-table.tex`, `figures/chvatal-scatter.pdf`, `make report` |
| §5, milestone ladder and development activity | solved counts and code activity across development milestones | `data/milestone-*.csv` | `figures/milestone-ladder.pdf`, `figures/development-roadmap.pdf`, `make report` |

## Requirements

- **Level 1:** Python 3.9+ with matplotlib, and `xz`. Nothing else.
- **Levels 2–3:** Linux on x86-64 and either Docker (or Apptainer), or an
  Ubuntu 24.04 machine with the packages from `install-deps.sh`. Building
  everything takes about an hour and 10 GB of disk; it downloads the solver and
  checker sources. The reduced evaluation needs 4 cores and 16 GB of memory.
  The paper's experiments used 4–12 cores and 16–48 GB per run (listed by
  `paper/run/bench.py list`).

## Level 1: regenerate the paper's tables and figures (seconds)

```sh
make -C paper check      # rebuild data/ from the raw logs; every file must be identical
make -C paper            # data, figures in paper/figures/, and the tables printed
```

`make check` unpacks the archived campaigns, applies each experiment's merge
rule (`scripts/normalize.py`), recomputes the data files (`scripts/make_data.py`),
and compares them byte for byte with `data/`. `make` then draws the figures and
prints every table of the paper with the section it belongs to; compare it with
the paper.

## Level 2: smoke test and reduced evaluation

Build the image (or run `install-deps.sh` and `paper/run/setup.sh all` on
Ubuntu 24.04):

```sh
docker build -f paper/Dockerfile -t tactix-artifact .
docker run --rm -it tactix-artifact
```

Inside the container (`/tactix`):

```sh
paper/run/bench.py run smoke --jobs 4
```

The smoke test solves the five formulas in `examples/cnf/` with Tactix and
Kissat and checks every answer; it takes seconds. Expect `10 records, 10
answered, 10 verified`. The records in `paper/run/results/smoke/records/`
show all three routes of Tactix's proofs: `tactix-dsr` (dsr-trim and Trestle),
`tactix-drat` (drat-trim and CakeLPR) and `tactix-pbp` (VeriPB and CakePB).

The reduced evaluation runs each experiment on its subset in
`benchmarks/subsets/`. The instances are already in the image:

```sh
B=paper/run/bench.py
$B run tierd      --subset short --jobs 4   # 30 formulas x 6 solvers
$B run milestones --subset short --jobs 4   # the same 30 formulas x 6 Tactix versions
$B run interface  --subset short --no-check # Tier A and fast regression instances, sequential for timing
$B run cruxmiter  --subset short --jobs 3   # seed 7
$B run chvatal    --subset short --jobs 4   # 3 formulas x 5 configurations
make -C paper LOGS=run/results DATA=run/results/data FIGURES=run/results/figures data figures report
```

Each command prints one line per run and ends with a summary; `make` then
prints the paper's tables for the rerun. On a subset, the counts are those of
the subset, and runtimes depend on the machine. What should agree with the
paper: every answer is correct and checked, the same solvers solve the same
formulas, and the relative order of the configurations (for example, the
cruxmiter and Chvátal strategies are faster than default Tactix on these
formulas).

## Level 3: full evaluation

The same commands without `--subset` run the full experiments. Their cost in
the paper's runs:

| Experiment | Runs | Solving + checking (CPU-core hours) |
|---|---|---|
| tierd | 1,166 × 6 | ~20,600 |
| milestones | 1,166 × 6 | ~13,500 |
| interface | 383 × 2 | ~100 |
| cruxmiter | 10 × 3 | ~440 |
| chvatal | 23 × 5 | ~330 |

On a SLURM cluster, `bench.py slurm EXPERIMENT --partition P` submits one
array task per run with the experiment's CPU, memory and time limits, and
`bench.py collect` gathers the records when it finishes. The paper's runs
used the same commands, limits and checker pipelines; the archived manifests
record the exact binaries (`logs/*/raw/*/manifest.json`).

## Notes on the archived runs

- **Which runs count.** Where an archived campaign was rechecked with more
  memory or a longer checking window, the merge rule in
  `scripts/normalize.py` takes the recheck only when it verifies (Tier D) or
  only after a resource failure (Chvátal). One Chvátal campaign carries a
  `CORRECTNESS-BUG` note about two CakeLPR crashes; the recheck campaigns
  verified both certificates, and the paper uses the rechecked outcome.
- **Commits.** Each Tactix configuration was built from a commit tagged
  `paper-*` in this repository (`git tag -l 'paper-*'`; `run/experiments.py`
  lists them). The cruxmiter strategy was run with the environment settings
  listed in `run/experiments.py`, which configure the built-in default
  strategy it hands over to.
- **Reference solvers** are built from their SAT Competition sources
  (`tools/solvers/fetch-winners.sh` records the URLs and archive hashes).
  LymphoSAT's competition binaries require an Ice Lake CPU; it is rebuilt for
  generic x86-64 without changing its algorithms (`run/build_lymphosat.py`).
- **Checkers.** drat-trim, CakeLPR, dsr-trim, Trestle, sr2drat, VeriPB and
  CakePB are built at pinned commits by `tools/checkers/setup.sh` and
  `run/setup.sh`.

## Beyond the paper

Tactix is a general solver: see the main [README](../README.md),
[docs/getting-started.md](../docs/getting-started.md) for building, solving
and checking, and `examples/` for strategies written against the C
interface.
