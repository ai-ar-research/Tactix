#!/usr/bin/env python3
"""Use validated5956 binaries to capture the earlier same-pop divergence."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5958-bve-full-heap'
c = root / 'benchmarks/runs/20260908-5978-bve-earlier-heap'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest
assert not c.exists()
c.mkdir()
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir()
for p in (parent / 'pins').iterdir(): shutil.copy2(p, c / 'pins' / p.name)
runner = (parent / 'run.py').read_text().replace('tactix-5958-', 'tactix-5978-')
old = "env['SAT_BVE_FULL_HEAP_EXTERNAL'] = '122629'"
assert runner.count(old) == 1
runner = runner.replace(old, "env['SAT_BVE_FULL_HEAP_EXTERNAL'] = '462198'")
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('5958-bve-full-heap', '5978-bve-earlier-heap'))
evidence = root / 'benchmarks/runs/20260908-5972-bve-sift-ancestry/heap-validation.json'
m.update(parent_campaign=str(parent.relative_to(root)),
    ancestry_evidence=dict(path=str(evidence.relative_to(root)), sha256=sha(evidence)),
    hypothesis='5972 moves the earlier same-pop topology divergence to462198:60705 lands8930 versus8954 with equal score/counts/root/size. Capture complete pre-pop state to localize its differing child subtree.',
    validation='Reuse unchanged5956 observer binaries. Replay all ordinary300K traces, partial proof and complete BVE attempts per arm. Capture exactly initial and firstbeforepop462198 at271079.',
    test_consideration='No source changes;5956 complete-heap tests and actual native controls remain applicable. Parameterized strict parser and tie-sift fixtures rerun for this selector. No timing or fuzz claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir()]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
