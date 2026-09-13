# Winner rerun launch-binding validation

## Decision

The matched winner runner now waits up to 30 seconds for the atomic launch
record and binds validation jobs, array jobs, and array task indices to that
record. Results carry the array and task identities; aggregation rejects a
record produced by another Slurm invocation. This closes the race between
`sbatch` returning and `launch.json` publication.

Commit `2df3c2298a27fc9c6013144a1e9397a229f8a10f` passed 393 benchmark-tool
tests. The launch-binding test covers accepted identities plus wrong validation
job, array job, and task negative controls.

## Compute checkpoint

Slurm job `1685429` passed from manifest
`0df5cbf617bd515c40a80f4df17cf857d7c56e3f3d5087cdc859024ca8c24826`:

- `cpu-dense-hwulab-q`, 12 logical CPUs, 32 GiB, no node constraint;
- completed `0:0` on `node018` in five seconds;
- LymphoSAT and Satsuma/Kissat SAT models checked against original inputs;
- Satsuma/Kissat DSR passed `dsr-trim`, `dsr-compress`, and `srcheck`;
- Tactix DRAT passed `drat-trim` and `cake-lpr`;
- forced exit and timeout controls released every anonymous handle; and
- no CNF or certificate artifact existed outside immutable pins.

The temporary 184 MiB validation fixture was removed after transferring this
evidence. Adapter transport details and the two preceding fail-closed checks
remain in the [adapter validation report](2026-09-12-winner-adapter-validation.md).
The real 40-task campaign still requires a final selection from the complete
post-refactor Tier-D solver phase and its own manifest-bound validation job.
