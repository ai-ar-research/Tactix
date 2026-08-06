# Probe outputs — tracking policy

One-off `sbatch --wrap` probe outputs land here (named `<topic>-<jobid>.out`).
Tracking follows the fuzz-log convention (slurm-jobs skill):

- **Commit ONLY probes cited as evidence** from committed docs (PROGRESS,
  plan, `docs/`, `benchmarks/reports/`) — cite by job id.
- Uncited probes are transient: delete them at session wrap-up.
- Probes are append-only evidence once committed — never edited.
- **`tools/fuzz/retention.py --tree benchmarks/probes --glob '*.out'` decides
  it mechanically**, one rule for this directory and the fuzz logs: cited means
  the JOB ID appears in a tracked document — literally, or inside a cited id
  RANGE, which is how a multi-leg experiment names its jobs. Matching on the
  basename instead reports nearly every cited probe as uncited.

(Policy origin: `docs/design/bve-tolerance-plan.md` §5.7.)
