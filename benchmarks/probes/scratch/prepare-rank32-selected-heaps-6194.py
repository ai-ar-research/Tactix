#!/usr/bin/env python3
"""Use existing selected-heap observers to locate the first changed producer."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6188-rank32-bve-candidates'
alignment = json.loads((old / 'attempt-alignment-6193.json').read_text())
assert alignment['matched_attempts'] == 673
assert sha(old / 'summary.json') == alignment['parent_summary_sha256']
c = root / 'benchmarks/runs/20260909-6194-rank32-selected-heaps'
assert not c.exists()
for name in ('pins', 'traces', 'logs', 'results'): (c / name).mkdir(parents=True)
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest, rel
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6188-', 'tactix-6194-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env[prefix + '_BVE_HEAP_TRACE'] = str(c / 'traces' / (arm + '.bve-heap.tsv'))\nenv['SAT_BVE_HEAP_CONFLICT'] = '386968'\nenv['SAT_BVE_HEAP_SUBJECTS'] = '349460,132302,349446'\nif arm == 'candidate':\n    env['TACTIX_EVENT_TRACE_BVE_HEAP'] = env['SAT_BVE_HEAP_SUBJECTS']\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6188-rank32', '6194-rank32'))
m.update(parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    alignment=dict(path=str((old / 'attempt-alignment-6193.json').relative_to(root)), sha256=sha(old / 'attempt-alignment-6193.json')),
    purpose='Selected BVE heap scores, occurrences and positions at first attempt-order split after673 identical attempts; include preceding349446 as anchor.',
    hypothesis='Incoming selected score or later heap mutation explains349460 versus132302; physical topology alone is not evidence of defective scheduling.',
    validation='Require both complete6188 suffix streams, boundary/census/prefix traces and partial proofs byte-identical before interpreting selected heap rows.',
    test_consideration='No solver source changes. Verified both observer implementations and asymmetric subject environment names; fixed6193 alignment fixtures and full parent replay protect interpretation.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
