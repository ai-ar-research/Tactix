#!/usr/bin/env python3
"""Collect existing full Reduce traces after the now-proven common learning."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6035-learned-stage'
c = root / 'benchmarks/runs/20260908-6038-learned-retention'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
s = json.loads((parent / 'summary.json').read_text()); assert s['full_parent_replay']
assert s['selected_clause_rows']['candidate'] == s['selected_clause_rows']['kissat']
assert s['selected_clause_rows']['candidate'][-1]['conflicts'] == 245473
c.mkdir()
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
runner = (parent / 'run.py').read_text().replace('tactix-6035-', 'tactix-6038-')
anchor = "assert not any(key.endswith('_EVENT_TRACE')"; assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env[prefix + '_REDUCE_TRACE'] = str(c / 'traces' / (arm + '.reduce.tsv'))
env[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '245473'
env[prefix + '_REDUCE_TRACE_CONFLICT_TO'] = '270978'
""" + anchor)
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6035-learned-stage', '6038-learned-retention'))
m.update(parent_campaign=str(parent.relative_to(root)),
    hypothesis='Both solvers learn the exact causal seven-literal clause at245473 with identical stages. It is missing from Tactix by270978. Compare every existing Reduce census/rank/victim table between these conflicts to identify retention/deletion mismatch.',
    validation='All6035 traces including learned-stage rows and partial certificate must replay exactly. Join native rank rows before comparing clause retention metadata; retain duplicate multiplicity.',
    native_rerun_reason='Missing retention and victim metadata after the proven common learning event; not a timing refresh.',
    test_consideration='No solver edit or rebuild. Reuse100/100-tested6030 pins and established Reduce callbacks; run existing reduction comparator tests and require full replay.',
    causal_evidence=dict(path=str((parent / 'summary.json').relative_to(root)), sha256=sha(parent / 'summary.json')),
    created_at=__import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir()]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
