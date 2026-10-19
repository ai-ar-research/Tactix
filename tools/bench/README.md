# Benchmark tools

This directory contains reusable benchmark infrastructure and the runners for
the active evaluation campaign. It deliberately does not retain scripts that
were written for a single diagnostic pass or a completed campaign.

## Current campaign

- `experimental_tierd.py` and `tierd_home_campaign.sh` run the current Tier D
  experiments.
- `fixed_runtime.py`, `controller_regression.py`,
  `freeze_paper_controller_baseline.py`, and `score_tierd_full.py` provide the
  fixed-runtime and paper-target controller-regression gates and scoring.
- `tierd_paper_regression.py` freezes and scores the full 1,166-row Tier-D
  gate against the paper release with its 64 GiB checker rechecks and a 10%
  raw/verified PAR-2 allowance.
- `runtime_task.py`, `proof_oracle.py`, `dsr_transport.cpp`, and the scratch
  helpers implement shared execution, proof validation, and cleanup behavior.

## Reusable and frozen runners

`run.py`, `fetch.py`, `gen_tier_a.py`, the frozen-baseline tools,
`freeze_sc24_26.py`, `subset_score.py`, `tactix_tierd.py`, `p4_cvg.py`, and
`public_theory_targets.py` support the published and reproducible benchmark
workflows. Frozen tier definitions live under `tiers/`; fixed gate inputs live
under `manifests/`.

Focused tests are kept beside the Python modules they exercise. Repository-wide
integration tests remain under `tests/`.

## Retention policy

A one-off diagnostic or closed-campaign script should not remain in this live
tool directory. Preserve its results and sufficient provenance in an immutable
`benchmarks/reports/<date-name>/` report or on the `paper-commits` branch, then
remove the script when the investigation closes. Add a new live tool here only
when it is part of a current workflow or is intended for reuse.
