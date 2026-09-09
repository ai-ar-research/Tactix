#!/usr/bin/env python3
"""Observe BVE candidate attempts at the validated first later Rank32 split."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6183-rank32-current-prefix'
s = json.loads((old / 'summary.json').read_text())
assert s['matching_decisions_from_start'] == 78180275
assert s['decisions']['left'][:3] == s['decisions']['right'][:3] == [386968, 78026757, 225]
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
c = root / 'benchmarks/runs/20260909-6188-rank32-bve-candidates'
assert not c.exists()
for folder in ('pins', 'traces', 'logs', 'results'):
    (c / folder).mkdir(parents=True)
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest, rel
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6183-', 'tactix-6188-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-candidates.tsv'))\nenv['SAT_BVE_CANDIDATE_CONFLICT'] = '386968'\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6183-rank32', '6188-rank32'))
m.update(parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    purpose='Census every BVE candidate begin/end and effort result at386968,where first pass changes active/irred counts and later decisions first split.',
    hypothesis='A different candidate,application orbudget outcome in BVE explains the first later Rank32 divergence. Equal incoming portable counts are not formula or heap identity.',
    validation='Replay both entire6183 300K–1M streams,all original boundaries and partial proof bytes/hash exactly before interpreting the additional BVE rows. No broadEventTrace,no early observer exit.',
    test_consideration='No new solver or observer source; reuse tested direct BVE candidate fields. Validate hash-bound source/pins and exact two-line runner instrumentation delta,then complete parent replay; existing prefix/observer fixtures retained.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
