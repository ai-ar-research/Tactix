# Immutable fuzz-harness submission

Date: 2026-08-26
Scope: fuzz infrastructure only; no solver behavior change and no fuzz rerun

## Finding

Claude Code's authoritative terminal report classified the exit-127 tails of
fuzz jobs 1648215 and 1648217 as infrastructure artifacts after their complete,
clean oracle summaries. While the Bash parent was waiting for `fuzz.py`, commit
`8a9c8f7` changed the live `tools/fuzz/run.sh` by 16 bytes. Bash later resumed
at its saved byte offset in the changed file and interpreted a shifted option
as a command. This occurred after all solver and oracle work; it was neither a
solver failure nor a fuzz failure.

The old `tools/fuzz/slurm.sh` batch body invoked the live repository path
`tools/fuzz/run.sh`, so a queued or running job did not own immutable harness
inputs even when its solver binary was correctly pinned.

## Repair

At submission, `tools/fuzz/slurm.sh` now:

1. creates a unique `build/pin-fuzz-harness-*` directory;
2. copies only `run.sh`, the Python harness modules, stress scenarios, and the
   regression corpus required by that leg;
3. computes and records a deterministic aggregate SHA-256;
4. makes the snapshot read-only and emits a batch script that executes only its
   pinned `run.sh`;
5. removes the pin after a successful exit, but retains it after failure for
   Claude-owned diagnosis.

`run.sh` accepts `TACTIX_REPO_ROOT` and `TACTIX_FUZZ_OUTPUT_ROOT`. Thus a pinned
runner still resolves the frozen solver/oracles from the repository and writes
default failure bundles to the established live output directory rather than
inside an ephemeral read-only pin. An explicit `--outdir` continues to win.

Development Plan section 8.2 and the harness README now make immutable executed
harness inputs mandatory for every fuzz leg.

## Validation

- `bash -n tools/fuzz/run.sh tools/fuzz/slurm.sh`: PASS.
- New regression `tests/test_fuzz_slurm_snapshot.py`: PASS.
- Mocked three-physical-core submission requests six logical CPUs, 24,000 MiB,
  and three workers.
- The emitted batch names the pinned runner and contains no live
  `tools/fuzz/run.sh` invocation.
- The pin is read-only and an independent recomputation exactly matches the
  wrapper's reported aggregate SHA-256.
- Focused CTest set passes 6/6: immutable fuzz snapshot, Tier-D pinning,
  citations, section references, proof-rule synchronization, and the
  rule/policy boundary.
- Test pins and temporary files are removed; no cluster job or fuzz campaign
  was submitted.

## Consequence

The completed consolidation fuzz verdict remains terminal PASS and is not
repeated. Future Claude-owned fuzz campaigns retain their existing seeds, case
counts, oracles, and 50-physical-core policy, while their executed harness
identity is now stable for the full job lifetime.
