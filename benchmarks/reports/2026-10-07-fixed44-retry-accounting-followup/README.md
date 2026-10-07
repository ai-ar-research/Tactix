# Fixed-44 retry accounting provenance follow-up

This is a follow-up to `../2026-10-07-fixed44-retry-accounting`.  The original
17-test report is unchanged.  This checkpoint closes provenance gaps found by
an independent review of scorer checkpoint `38488a3c1` and adds nine focused
negative/continuation checks, for 26 passing tests total.

## Fail-closed bindings

The scorer now requires every campaign to have a launch record bound to its
current manifest, a validation launch bound to that same manifest, and a
matching `passed=true` validation result.  It hashes every declared pin and
rejects undeclared files anywhere below `pins/`.

Validation-created Python bytecode is the only supported exception.  It must
be authorized explicitly with repeatable `--pyc-supplement` arguments.  Each
supplement binds one campaign, its validation job, all source and bytecode
hashes, the timestamp/size bytecode headers, the Python 3.9 producer, and an
affirmative semantic-compile audit.  The scorer checks those recorded facts;
it does not unmarshal or recompile bytecode under the scoring interpreter.
The two explicit authorities used here are retained as
`current-parent-pyc-supplement.json` and `old-parent-pyc-supplement.json`.

For checked results, the scorer verifies the pinned checker executable path
whenever the hash-bound runtime recorded a checker command.  Legacy result
records without checker commands continue to rely on the hash-bound runtime
and checker pins; the merged output states this remaining trusted-runner
boundary rather than inventing fields absent from those records.

## Retry chain

Retry attempt numbers must be consecutive and each retry must name the
immediately preceding campaign, manifest, launch job, and launch-record hash.
An undispatched row may still bind its explicit older result predecessor.  A
row replaced by any later result can only use that new result as its next
predecessor.

The pinned Slurm array must equal `allowed_tasks`.  Its node exclusions must
equal the manifest resource constraint and the typed resource-exclusion
record.  Every excluded node is backed by a frozen RUNNING snapshot and a
hash-bound holder claim.  Holder job IDs must belong to a preceding campaign
launch.  Coverage rank, instance, node, completion reason, and predecessor
result hash must all agree before a retry can replace a cell.

## Frozen commands and results

Focused checks:

```bash
PYTHONPATH=tools/bench python3 -m unittest tools/bench/test_score_fixed44.py
```

Current parent plus retry:

```bash
python3 tools/bench/score_fixed44.py \
  --pyc-supplement benchmarks/reports/2026-10-07-fixed44-retry-accounting-followup/current-parent-pyc-supplement.json \
  benchmarks/runs/20261007-fixed44-10942-cooperative-python \
  benchmarks/runs/20261007-fixed44-10942-cooperative-python-retry01
```

At the frozen read, this reports 39 resource-incomplete rows, four missing
live rows, one proof-route/stage failure, zero independently verified rows,
no PAR-2, and `qualification_accepted=false`.  The full JSON is
`current-score.json`.

The older full-44 campaign remains readable only with its own explicit
bytecode authority:

```bash
python3 tools/bench/score_fixed44.py \
  --pyc-supplement benchmarks/reports/2026-10-07-fixed44-retry-accounting-followup/old-parent-pyc-supplement.json \
  benchmarks/runs/20261007-final-0e6b95c91-fixed44
```

That frozen read reports 13 missing rows, 25 solver timeouts, three checked
UNSAT rows, one checked SAT row, two proof-route/stage failures, four verified
original ranks (147, 263, 540, and 630), no PAR-2, and
`qualification_accepted=false`.  The full JSON is `old-score.json`.
