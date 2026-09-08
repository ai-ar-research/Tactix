#!/usr/bin/env python3
"""Observe actual incoming clauses and accepted grids at the next Rank32 split."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5537-rank32-small-prefix'
c = root / 'benchmarks/runs/20260908-5564-rank32-small-factor'
authority = old / 'nonbinary-localization.json'
if not authority.exists():
    authority = old / 'nonbinary-boundary-localization.json'
assert authority.is_file(), authority
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
assert json.loads((old / 'summary.json').read_text())['matching_rows'] == 18696140
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
runner = (old / 'run.py').read_text().replace('tactix-5537-', 'tactix-5564-')
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''
selector = 'factor_begin' if arm == 'kissat' else 'name=Factor|stage=begin'
env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.factor.tsv')),
            'SAT_EVENT_TRACE_FACTOR_FRESH_ONLY': '1',
            prefix + '_EVENT_TRACE_CONFLICT_FROM': '19375',
            prefix + '_EVENT_TRACE_CONFLICT_TO': '19375',
            prefix + '_EVENT_TRACE_LIMIT': '100000',
            'SAT_EVENT_TRACE_FACTOR_FRESH_CONFLICT': '19375',
            'SAT_EVENT_TRACE_BINARY_SET_PATH': str(c / 'traces' / (arm + '.binary.tsv')),
            'SAT_EVENT_TRACE_BINARY_SET_MATCH': selector,
            'SAT_EVENT_TRACE_BINARY_SET_CONFLICT': '19375',
            'SAT_EVENT_TRACE_FORMULA_SET_PATH': str(c / 'traces' / (arm + '.formula.tsv')),
            'SAT_EVENT_TRACE_FORMULA_SET_MATCH': selector,
            'SAT_EVENT_TRACE_FORMULA_SET_CONFLICT': '19375'})
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
    causal_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
    hypothesis='At19375 the first nonbinary portable difference follows Factor (13 irredundant clauses), before the20095 decision split. Compare all incoming physical binary multiplicities and large-clause multisets plus accepted fresh grids; equal counts alone do not establish equal input or heap order.',
    validation='Two clean100000-conflict stops, exact entire per-arm5537 decision/prefix/boundary replay and partial-proof SHA, pin/input/trace hashes, framed complete formula/binary snapshots and untruncated contiguous factor_fresh events. Preserve multiplicity and original event order. No final proof or timing claim.',
    test_consideration='No solver or observer code change. Existing immutable pins plus complete parent replay check observational neutrality. Add focused positive/negative parser fixtures for the new general nonzero-conflict grid and full-formula comparison.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
