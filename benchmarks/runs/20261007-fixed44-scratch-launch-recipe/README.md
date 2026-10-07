# Full44 shared-floor launch recipe (prepared, never submitted here)

Frozen runner: `runner-source/` is byte-identical to `tools/bench` at
`cf2440956` (shared-floor profile; includes the file-bound fixed64 provenance
of `8031e22c6`). `prepare-wrapper.py` differs from
`runner-source/experimental_tierd.py` only by `wrapper.diff` (repository root
and runner-pin source relocation). `runner-source.sha256` binds every file;
`prepare.sh` refuses to run if any changed.

The prepared campaign matches the b26 preparation except the scratch profile:
cpu-q, 4 CPUs, 32 GiB, 5000 s solve, 14400 s checker, `--certificate auto`,
`long64-fixed` DSR checker profile, `--default-graph-labeling=on`, the exact
44-row selection, array `0-43` with no throttle or node constraint, wall
05:45:00, and `--scratch-capacity-profile fixed44-shared-floor-v1`
(see `benchmarks/reports/2026-10-07-fixed44-scratch-admission/`).

## Commands (run from anywhere; all paths absolute inside the scripts)

```sh
R=/home/hwu/SAT-solving
P=$R/benchmarks/runs/20261007-fixed44-scratch-launch-recipe
C=benchmarks/runs/<new-campaign>            # must not exist
# 1. prepare (verifies binary sha256, full width, no throttle/constraint)
$P/prepare.sh $C build/pin-<candidate>-rel/tactix <release-sha256> \
    <archived-source-tree> <source-commit>
#    optional trailing --solver-argument=... replace the default
#    --solver-argument=--default-graph-labeling=on
# 2. validation job (~1 min on a compute node)
PYTHONDONTWRITEBYTECODE=1 python3 $P/prepare-wrapper.py --campaign $R/$C --validate
# 3. after validation.json has "passed": true, submit the full-width array
PYTHONDONTWRITEBYTECODE=1 python3 $P/prepare-wrapper.py --campaign $R/$C --submit
# 4. score (coverage only)
python3 $R/tools/bench/score_fixed44.py $R/$C
```

`prepare.sh` writes `<campaign>-preparation/PREPARED.json` (hashes of manifest,
matrix, array, validation script, solver, recipe). Before step 3, check
`quota -s` (home quota) and record a fresh `squeue`/node snapshot.

Exercised prepare-only on the b26 pin as
`../20261007-fixed44-scratch-recipe-check-b26` (never validated or submitted).
