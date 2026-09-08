#!/usr/bin/env python3
"""Separate the input snapshot from fresh-only observers that filter boundaries."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5537-rank32-small-prefix'
c = root / 'benchmarks/runs/20260908-5565-rank32-factor-input'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
runner = (old / 'run.py').read_text().replace('tactix-5537-', 'tactix-5565-')
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''
selector = 'factor_begin' if arm == 'kissat' else 'name=Factor|stage=begin'
env.update({'SAT_EVENT_TRACE_BINARY_SET_PATH': str(c / 'traces' / (arm + '.binary.tsv')),
            'SAT_EVENT_TRACE_BINARY_SET_MATCH': selector,
            'SAT_EVENT_TRACE_BINARY_SET_CONFLICT': '19375',
            'SAT_EVENT_TRACE_FORMULA_SET_PATH': str(c / 'traces' / (arm + '.formula.tsv')),
            'SAT_EVENT_TRACE_FORMULA_SET_MATCH': selector,
            'SAT_EVENT_TRACE_FORMULA_SET_CONFLICT': '19375'})
assert not any(key.endswith('_EVENT_TRACE') for key in env)
assert 'SAT_EVENT_TRACE_FACTOR_FRESH_ONLY' not in env
''' + anchor)
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)),
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_summary_sha256=sha(old / 'summary.json'),
    companion='benchmarks/runs/20260908-5564-rank32-small-factor',
    hypothesis='Compare full incoming binary multiplicities and large-clause multisets at Factor19375. Fresh-only5564 filters slot snapshots in both trace implementations, so retain its accepted grids and use a separate no-global-event-trace replay for the input; do not interpret missing snapshots as empty formula.',
    validation='Two clean100000-conflict stops, exact entire per-arm5537 decision/prefix/boundary replay and partial-proof SHA; frozen pin/input/trace hashes; complete framed binary snapshots and full live large-clause rows. No verdict or acceptance timing claim.',
    test_consideration='Configuration-only existing observer reuse. Explicit runner assertions forbid the incompatible global/fresh-only mode. Comparator fixtures reject missing/truncated snapshots, wrong boundary/conflict/count, malformed clauses and lost multiplicity.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
