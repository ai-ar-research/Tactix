# Winner rerun environment validation

## Decision

Matched winner subprocesses now use one environment stored in the immutable
campaign manifest. It contains fixed C locale, `/tmp`, deterministic Python
hashing, cluster Python/system utility paths, and the GCC 13 runtime library
paths needed by the pinned binaries. Arbitrary parent variables—including
`LD_PRELOAD`, `PYTHONPATH`, sanitizer settings, and solver knobs—cannot enter
decompression, adapters, solvers, model checks, or proof checks.

Commit `631b23807bae8ebc8df93de92f9223cc319fe1c0` passed 394 benchmark-tool
tests. A focused negative control injects external loader, Python, sanitizer,
and Tactix/Kissat variables and verifies the exact fixed result. Static checks
also resolved `taskset`, `xz`, Python, Bash, and every core pinned binary under
that environment with no missing shared library.

## Compute checkpoint

Job `1685454` ran from manifest
`a1e74dc6784f7a12edf15c329dae2ee6692d70b0186f283f630cc28c7c9830fa`
on `node017`, using `cpu-dense-hwulab-q`, 12 logical CPUs, 32 GiB, and no node
constraint. It completed `0:0` in five seconds. LymphoSAT and Satsuma/Kissat
SAT models checked against original inputs; Satsuma/Kissat DSR and Tactix DRAT
passed their native pipelines. Forced exit and timeout controls released every
anonymous handle, and no formula or certificate artifact remained outside
immutable pins.

The temporary 184 MiB fixture was removed after evidence transfer. Slurm launch
identity details remain in the [launch-binding report](2026-09-12-winner-launch-binding-validation.md).
The real 40-task campaign must be prepared from the final complete Tier-D
selection and repeat its own manifest-bound validation before submission.
