# How the paper's experiments were actually run

All experiments of the paper were run as SLURM array jobs on a cluster of AMD
EPYC 7513 nodes, one array task per (configuration, formula), with the
development harness in this repository's `tools/bench/`. This directory holds
the exact SLURM scripts that were submitted, extracted from the archived
campaigns in `../logs/*/raw/` (`../scripts/extract_slurm.py`); `campaigns.md`
lists every campaign with its partition, cores, memory, time limit, array size
and concurrency, and entry point.

## The pipeline

1. **Campaign.** A campaign directory was frozen before submission: the
   instance list, the solver and checker binaries with their SHA-256
   (`manifest.json`), the commands and limits, and a copy of the harness
   scripts (`pins/`, `runner/`). The archived campaigns keep everything but the
   binaries and formulas.
2. **Runs** (`array.sbatch`). Each task solved one formula with one
   configuration, under the job's cgroup memory limit and the configuration's
   time limit, then checked the answer: SAT models with
   `tools/fuzz/checkmodel`, UNSAT certificates with the pinned checker pipeline
   (`tools/bench/proof_oracle.py`). It wrote one JSON record (`results/`).
   Certificates were deleted after checking.
3. **Validation and aggregation** (`validate.sbatch`, `aggregate.sbatch`).
   The records were checked for completeness and consistency against the
   manifest, and aggregated.
4. **Rechecks.** Certificates whose check ran out of memory or time were
   rechecked in separate campaigns with 64 GB and 8 h (`*-recheck-*`).
5. **Analysis.** The campaigns were archived as `../logs/*/raw/*.tar.xz`.
   `../scripts/normalize.py` applies the merge rules (a recheck counts where it
   verifies, or after a resource failure), `../scripts/make_data.py` computes
   the data the tables and figures are drawn from (`../data/`), and the plot and
   table scripts draw them. `make -C paper check` reruns this and confirms that
   the result is identical to `../data/`.

| Experiment | Runs with | Limits per run |
|---|---|---|
| Tier D, Tactix | `tools/bench/runtime_task.py` | 4 cores, 32 GB, 5,000 s; checking 8 h (64 GB in rechecks) |
| Tier D, reference solvers | `tools/bench/frozen_baseline.py`; rechecks `tools/bench/baseline_rerun.py` | 4 cores, 32 GB, 5,000 s; checking 8 h (64 GB in rechecks) |
| Milestones | `tools/bench/runtime_task.py` (no proofs) | 4 cores, 32 GB, 5,000 s |
| Interface | Tier A: `tools/bench/run.py`; other sets: `tools/bench/runtime_task.py` | 8-12 cores, 32 GB, 30 s (Tier A) or 1,200 s |
| Cruxmiter | `tools/bench/run.py` | 12 cores, 48 GB, 12 h; checking 72 h |
| Chvátal | `tools/bench/run.py`; Satsuma+Kissat: `chvatal_satsuma.py` | 4 cores, 16 GB, 5,000 s; checking 2 h (48-64 GB in rechecks) |

Each run used one solver thread; the extra cores reserved memory bandwidth and
avoided interference between runs on a node. `chvatal_satsuma.py` was removed
from the tree after the paper's runs; it is in the history:
`git show 0e8aed8f2:tools/bench/chvatal_satsuma.py`.

## Rerunning

The development harness depends on the cluster's layout (paths, partitions,
frozen campaign directories). For reruns, `../run/bench.py` implements the same
runs in one place: the same commands (`../run/experiments.py`, copied from the
archived manifests), the same limits, the same checker pipelines, and records
that the same analysis reads. On a SLURM cluster:

```sh
paper/run/setup.sh all                                   # build everything
paper/run/bench.py fetch tierd --profile full            # download the formulas
paper/run/bench.py slurm tierd --profile full --partition PARTITION --out results/full
paper/run/bench.py collect --out results/full            # when the array has finished
python3 paper/scripts/compare.py --run results/full --out results/full/comparison
```

`bench.py slurm` writes `results/full/tierd/array.sbatch` (one array task per
run, with the experiment's cores, memory and time) and submits it; `--dry-run`
writes it without submitting, `--concurrency N` runs at most N tasks at a time,
and `--sbatch "OPTIONS"` adds options such as `--account=...`.
