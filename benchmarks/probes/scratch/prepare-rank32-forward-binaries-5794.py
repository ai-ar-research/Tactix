#!/usr/bin/env python3
"""Complete the selected forward action's missing common binary context."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'
selected = root / 'benchmarks/runs/20260908-5791-rank32-selected-forward/summary.json'
c = root / 'benchmarks/runs/20260908-5794-rank32-forward-binaries'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
base = json.loads((parent / 'manifest.json').read_text())
for rel, digest in base['hashes'].items(): assert sha(parent / rel) == digest
assert json.loads(selected.read_text())['observer_parent_trace_and_proof_equal']
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
runner = (parent / 'run.py').read_text().replace('tactix-5771-', 'tactix-5794-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = '''env['SAT_EVENT_TRACE_BINARY_SET_PATH'] = str(c / 'traces' / (arm + '.binaries.tsv'))
env['SAT_EVENT_TRACE_BINARY_SET_CONFLICT'] = '179463'
env['SAT_EVENT_TRACE_BINARY_SET_MATCH'] = 'name=Strengthen|stage='
'''
(c / 'run.py').write_text(runner.replace(anchor, extra + anchor))
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)))
m = dict(base)
m.update(parent_campaign=str(parent.relative_to(root)),
         selected_authority=dict(path=str(selected.relative_to(root)), sha256=sha(selected)),
         hypothesis='5791 shows Tactix binary subsumption of a Kissat-filtered clause. Establish complete binary multiplicities at both forward entry/exit pairs and presence of the shared two-literal subsumer before classifying the extra deletion.',
         validation='Both arms full ordinary300K traces and partial proof bytes equal5771; four correctly framed Strengthen snapshots must match native/Tactix boundaries and their counts.',
         native_rerun_reason='Missing binary context for the validated selected action, not a runtime baseline refresh.',
         test_consideration='Configuration-only existing snapshot hooks. Explicit native set-boundary source routing inspected after prior formula selector error; parser tests and full parent replay required. No solver change or duplicate fuzz campaign.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/candidate', c / 'pins/kissat')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
