#!/usr/bin/env python3
"""Observe the first current/native search split on a short, replayed witness."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6201-rank49-current-prefix'
current = root / 'benchmarks/runs/20260909-6632-rank49-current-replay'
c = root / 'benchmarks/runs/20260909-6639-rank49-search-window'
assert not c.exists()
s = json.loads((current / 'summary-6634.json').read_text())
assert s['partial_proof_equal'] and s['native_decision_comparison']['matching_rows'] == 57569
m = json.loads((current / 'manifest.json').read_text())
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
runner = (old / 'run.py').read_text().replace('tactix-6201-', 'tactix-6639-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.search.tsv')),
            prefix + '_EVENT_TRACE_CONFLICT_FROM': '9268',
            prefix + '_EVENT_TRACE_CONFLICT_TO': '9275',
            prefix + '_EVENT_TRACE_SEARCH_ONLY': '1',
            prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
            prefix + '_EVENT_TRACE_SKIP_HEAP': '1',
            prefix + '_EVENT_TRACE_SKIP_RANDEC_DRAWS': '1',
            prefix + '_EVENT_TRACE_SKIP_ANALYSIS_TICKS': '1',
            prefix + '_EVENT_TRACE_LIMIT': '250000'})""")
compile(runner, str(c / 'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c / folder).mkdir(parents=True)
shutil.copy2(current / 'pins/candidate', c / 'pins/candidate')
shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
for arm, parent in (('candidate', current), ('kissat', old)):
    p = parent / f'results/{arm}.json'
    m['dependencies'][str(p.relative_to(root))] = sha(p)
    r = json.loads(p.read_text())
    for name, digest in r['trace_sha256'].items():
        assert sha(parent / 'traces' / name) == digest
        m['dependencies'][str((parent / 'traces' / name).relative_to(root))] = digest
for p in (current / 'summary-6634.json', old / 'summary.json', old / 'run.py',
          root / 'benchmarks/probes/scratch/validate-square-phase-5486.py'):
    m['dependencies'][str(p.relative_to(root))] = sha(p)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6201-rank49-prefix', '6639-rank49-window'))
m['resource_contract']['array'] = '0-1'
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    cached_controls={'candidate': str(current.relative_to(root)), 'kissat': str(old.relative_to(root))},
    search_window=[9268, 9275], trace_row_cap=250000,
    purpose='Current/native decisions first differ at9274/9273 after57569 matching rows. Capture bounded implications/conflicts/learned transactions and analysis detail preceding this split; do not infer causality from earlier Congruence binary count or tune heap ties.',
    validation='Require exact own decisions/prefix/partial proof through100K; current ordinary macro and inprocess traces exact6632. Native6201 original inprocess rows must match after only source-backed extra analysis/end rows within9268..9275 and their sequence shifts (5486 tested reader). Retain all other extra/missing/changed outputs as failed observer premises. Selected detail must be nonempty and below row cap.',
    test_consideration='No solver/observer implementation changes. Existing narrow trace flags and5486 boundary fixtures are reused; add any new transaction/parser fixtures before causal interpretation. Current183 benchmark tests pass.',
    limitation='Two diagnostic invocations only, not timing denominator refresh or final proof acceptance. Global tracing is bounded in detail on this small2200-variable witness; macro snapshots outside detail window may remain. No stop-before100K or changed proof/checker contracts.')
m['hashes'] = {name: sha(c / name) for name in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
