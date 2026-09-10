#!/usr/bin/env python3
"""Bounded repair replay against cached actual native evidence; no refresh."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'
c = root / 'benchmarks/runs/20260910-7038-collect-retry-prefix'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
build = root / 'benchmarks/runs/20260910-7030-collect-retry-build/manifest.json'
b = json.loads(build.read_text())
gate = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate/summary-7036.json'
g = json.loads(gate.read_text())
assert g['solver_records'] == 66 and len(g['pairs']) == 33
dependencies = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or expected == digest, p
    dependencies[str(p.relative_to(root))] = digest
for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
for rel, digest in b['binaries'].items(): bind(root / rel, digest)
for rel, digest in g['hashes'].items(): bind(root / rel, digest)
for rel, digest in m['hashes'].items(): bind(old / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
native = old / 'results/kissat.json'
nr = json.loads(native.read_text())
assert nr['exit_code'] == 0 and nr['scratch_deleted']
for rel, digest in nr['trace_sha256'].items(): bind(old / 'traces' / rel, digest)
for p in (native, old / 'summary-7015.json', old / 'manifest.json', build, gate, Path(__file__),
          root / 'benchmarks/runs/20260910-7024-rank35-retry-producer/producer-localization-7032.json'):
    bind(p)
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
pin = root / 'build/pin-7030-collect-retry-trace/tactix'
shutil.copy2(pin, c / 'pins/candidate')
runner = (old / 'run.py').read_text()
before = "('candidate', 'kissat')[int(sys.argv[1])]"
assert runner.count(before) == 1
runner = runner.replace(before, "('candidate',)[int(sys.argv[1])]").replace('tactix-7011-', 'tactix-7038-')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace(
    '7011-rank35-current-native-prefix', '7038-collect-retry-prefix').replace('--array=0-1', '--array=0-0')
assert '--nodelist' not in batch and '%' not in batch.split('--array=')[1].splitlines()[0]
(c / 'array.sbatch').write_text(batch)
out = {key: m[key] for key in ('rank', 'input', 'input_sha256_compressed', 'stop_conflict',
    'stream_from', 'common_decisions', 'prior_common_hash', 'session_authority')}
out.update(arms=['candidate'], parent=str(old.relative_to(root)), dependencies=dependencies,
    resource_contract=dict(m['resource_contract'], array='0-0'),
    hypothesis='Removing satisfied-large collection retry advice may close the native Rank35 BVE divergence. First verify the inherited100K prefix; if changed, localize earlier instead of claiming late closure. Otherwise compare complete1M suffix against cached native and current6730.',
    validation='One changed-source diagnostic, not an observer-neutrality claim. Require clean requested stop and bound whole traces/partial proof. Cached native7011 already validated; no native rerun. Gate7036 has all66 expected outcomes; proofs may remain pending.',
    test_consideration='7030 full113/113/112 suites, new40-state advice matrix,16 strong actualold/new proofs and B28+C5 solver gate. Reuse existing decision-prefix/suffix reader fixtures; no new observer or maintained parser.',
    limitation='No timing acceptance, proof acceptance for partial certificates, promotion, fuzz access or rule legality change. Preserve earlier mismatches and independently price full runtime.')
out['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'pins/candidate', c / 'run.py', c / 'array.sbatch')}
with (c / 'manifest.json').open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)
