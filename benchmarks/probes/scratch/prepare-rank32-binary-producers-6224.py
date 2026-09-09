#!/usr/bin/env python3
"""Reuse frozen observers to locate the missing retry's binary producer."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

old = root / 'benchmarks/runs/20260909-6217-rank32-retry-members'
c = root / 'benchmarks/runs/20260909-6224-rank32-binary-producers'
assert not c.exists()
s = json.loads((old / 'summary.json').read_text())
assert s['parent_replay']
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest
for name in ('pins', 'traces', 'results', 'logs'):
    (c / name).mkdir(parents=True)
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6217-', 'tactix-6224-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = """env['SAT_EVENT_TRACE_BINARY_SET_PATH'] = str(c / 'traces' / (arm + '.binary-sets.tsv'))
env['SAT_EVENT_TRACE_BINARY_SET_MATCH'] = 'slot_boundary'
env['SAT_EVENT_TRACE_BINARY_SET_CONFLICT'] = '319386'
env['SAT_EVENT_TRACE_BINARY_SET_EXTERNALS'] = '337674,390671,399915,409823,444046'
env[prefix + '_BINARY_LIFECYCLE_TRACE'] = str(c / 'traces' / (arm + '.binary-lifecycle.tsv'))
env[prefix + '_BINARY_LIFECYCLE_PAIRS'] = '337674,468072;-337674,-468072;337674,-468072;-337674,468072'
"""
runner = runner.replace(anchor, extra + anchor)
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6217-rank32', '6224-rank32'))
m.update(parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    purpose='Locate native binary deletions at319386 causing extra retry marks for337674; selected snapshots include allfive missingmembers.',
    hypothesis='Canonical Substitute drops an equivalence binary without publishing the native removal retry; other producer paths remain possible.',
    validation='Every6217 trace and partialproof must match exactly. Only binary-sets and binary-lifecycle are new. Snapshot multiset differences and observed site labels, not guessed endpoints, decide the producer.',
    test_consideration='No solver source changes. Reuse tested observers, require full parent replay and framed snapshot parsing with negative controls. Four signed337674/468072 pairs are hypotheses from the last shared deleted clauses, not exhaustive endpoint coverage.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
