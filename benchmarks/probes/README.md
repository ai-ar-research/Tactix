# Probe outputs — tracking policy

One-off `sbatch --wrap` probe outputs land here (named `<topic>-<jobid>.out`).
Tracking follows the fuzz-log convention (slurm-jobs skill):

- **Commit ONLY probes cited as evidence** from committed docs (PROGRESS,
  plan, `docs/`, `benchmarks/reports/`) — cite by job id.
- Uncited probes are transient: delete them at session wrap-up.
- Probes are append-only evidence once committed — never edited.

(Policy origin: `docs/design/bve-tolerance-plan.md` §5.7.)
