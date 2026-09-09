#!/usr/bin/env python3
"""Extend the largest added-rank loss after its fully equal100K prefix."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6200-rank50-current-prefix'
s = json.loads((old / 'summary.json').read_text())
assert s['decisions']['complete_equal'] and s['decisions']['matching_rows'] == 139633
assert s['prefixes']['candidate']['hash'] == s['prefixes']['kissat']['hash'] == '236dcfa1a362af4d'
for rel, digest in s['hashes'].items(): assert sha(root / rel) == digest, rel
c = root / 'benchmarks/runs/20260909-6211-rank50-extended-prefix'
assert not c.exists()
for name in ('pins', 'traces', 'results', 'logs'): (c / name).mkdir(parents=True)
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest, rel
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6200-', 'tactix-6211-')
anchor = "m['common_decisions'] if arm == 'candidate' else 1"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "m['common_decisions']")
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6200-rank50', '6211-rank50'))
m.update(stop_conflict=1000000, common_decisions=139633,
    parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    purpose='Locate first later choice or portable boundary split on addedRank50,largest absolute runtime loss.',
    hypothesis='The roughly6.87x total conflict gap emerges after the matched100K prefix; find its first portable producer,not tune instance-specific scheduling.',
    validation='Validate common139633-row hash236dcfa1a362af4d and entire cached decision/boundary/macro prefixes on both arms,then compare full through1M. Hash/delete partial certificates;no timing or final-proof acceptance.',
    test_consideration='No source/observer changes. Reused tested prefix reader,complete stream framing,common hash and byte-prefix checks. Unchanged12CPU/48000MiB,1200solver seconds and separate arms;reference timing remains frozen.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
