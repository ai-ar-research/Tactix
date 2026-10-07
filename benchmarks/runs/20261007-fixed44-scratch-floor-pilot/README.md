# Shared-floor mechanics pilot (Slurm 1755641, node002, 35 s)

`pilot.py` with `snapshot/` (= `tools/bench` at `cf2440956`, hashes in
`sources.sha256`). Real `/tmp`, O_TMPFILE, ledger flock slots, `statvfs`, and
the runtime SIGUSR1 interruption of a `measured()` writer child. Private pilot
ledger `/tmp/tactix-scratch-pilot-<uid>-<job>` was removed by the pilot.
Total transient writes about 5 GB. Evidence: `evidence-1755641.json`; PASS.
