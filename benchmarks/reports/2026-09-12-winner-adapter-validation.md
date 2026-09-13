# Winner rerun adapter validation

## Decision

The matched winner rerun harness may be used after the final 10 SAT + 10 UNSAT
selection exists. Slurm job `1685372` passed every solver, certificate, model,
and abnormal-cleanup check using anonymous node-local `O_TMPFILE` artifacts.
The result validates transport and checking; it does not select instances or
measure winner performance.

## Contract and pins

- Commit: `fd046fef65cc97e44809ce084cd563e436169e4b`
- Validation manifest SHA-256:
  `0c65eba6f5ff8c38a07c5e61550ae716e5cde25f6c00db9dab871df968bbc31b`
- Tactix binary SHA-256:
  `916eae4d7e87f5602649035ac15a8c2985e83deaec2250e338a47972b998e9bf`
- LymphoSAT tree SHA-256:
  `a4a7441fa158b303c863e4353b40e611b683579b6cdae9ec3efa4771b1c88702`
- Satsuma/Kissat tree SHA-256:
  `3bdce296d3b97e24be5403a51f28c40ec9e486132c7ed1ad65573daa3889d2ab`
- Proc-fd shim source/shared-object SHA-256:
  `4ad4241f856a493fd9d05136f7f5500dd5b013677063a6dab8b59f6e2ef311ae` /
  `78d079b95fd25684f6b13fd46a00491885ae5dac4a2748862e9901536f3e6433`
- Resources: `cpu-dense-hwulab-q`, 12 logical CPUs, 32 GiB, no node
  constraint; solver/checker contract 1200/16000 seconds. This one-job bounded
  interface checkpoint used a one-hour validation wall limit.

## Results

Job `1685372` ran on `node018`, completed `0:0` in five seconds, and recorded:

| Arm | Case | Independent result |
|---|---|---|
| LymphoSAT | SAT | original-input model verified |
| Satsuma/Kissat | SAT | original-input model verified |
| Satsuma/Kissat | UNSAT | DSR verified by `dsr-trim`, `dsr-compress`, and `srcheck` |
| Tactix | SAT | original-input model verified |
| Tactix | UNSAT | DRAT verified by `drat-trim` and `cake-lpr` |

The forced exit-7 and forced-timeout controls both closed all anonymous handles.
Every solver case also reported cleanup complete. A scan outside immutable
`pins/` found no CNF, proof, DRAT, LRAT, DSR, or PBP files.

## Repaired validation failures

- Job `1685317` failed closed because bundled Kissat called `realpath()` on an
  anonymous proc-fd path whose inode target ends in `(deleted)`. Commit
  `f3ca202b` added a preload shim for exact `/proc/<pid>/fd/<fd>` spellings.
- Job `1685347` reached the next guard and failed closed because Kissat first
  queried `W_OK` on `/proc/<pid>/fd`, although the existing descriptor itself
  remained writable. Commit `fd046fef` bypasses only that exact redundant
  directory query. Descriptor `stat`, descriptor `access`, and the actual
  `fopen` remain authoritative; all unrelated libc calls delegate unchanged.

The temporary validation campaigns were removed after this evidence was
transferred here. The final matched campaign must still pin and validate the
selection produced from all 1,166 final post-refactor Tier-D outcomes.
