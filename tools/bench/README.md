# Benchmark tools

This directory contains reusable benchmark infrastructure and the runners for
the active evaluation campaign. It deliberately does not retain scripts that
were written for a single diagnostic pass or a completed campaign.

## Current campaign

- `experimental_tierd.py` and `tierd_home_campaign.sh` run the current Tier D
  experiments. The default `--certificate pbp` selects the solver's no-option
  whole-run PBP route; `--certificate dsr` is the explicit native-DSR route.
- `gate.py` prepares and scores the fixed-set regression gates: `fixed-*`
  against the frozen Rank 1-50 Kissat reference, `controller-*` against the
  paper-target B28+C5, Rank 1-50 and WV100 baseline, and `tierd-*` for the full
  1,166-row Tier-D gate with its 64 GiB checker rechecks and a 10% raw/verified
  PAR-2 allowance. `tierd-score` accepts repeated `--recheck` arguments in
  overlay order, so a later strict retry replaces only its overlapping source
  tasks without weakening direct base binding.
- `tierd_home_campaign.sh recheck CAMPAIGN SOURCE CLASSES [SOURCE_TASK...]`
  prepares a direct, immutable-source 64 GiB recheck. Optional source-task IDs
  restrict retries without changing the source manifest, solver arguments, or
  5,000-second solve contract; use this for transient regeneration timeouts
  instead of nesting a recheck campaign.
- `runtime_task.py`, `proof_oracle.py`, `dsr_transport.cpp`, and the scratch
  helpers implement shared execution, proof validation, and cleanup behavior.

## Reusable and frozen runners

`run.py`, `fetch.py`, `gen_tier_a.py`, `subset_score.py`, and `p4_cvg.py`
support the published and reproducible benchmark workflows. Frozen tier
definitions live under `tiers/`; fixed gate inputs live under `manifests/`.

Focused tests are kept beside the Python modules they exercise, except that
`tests/test_runtime_task.py` covers `runtime_task.py`. Repository-wide
integration tests are also under `tests/`.

## Retention policy

A one-off diagnostic or closed-campaign script should not remain in this live
tool directory. Preserve its results and sufficient provenance in an immutable
benchmark report or on the `paper-commits` branch, then remove the script when
the investigation closes. Add a new live tool here only when it is part of a
current workflow or is intended for reuse.
