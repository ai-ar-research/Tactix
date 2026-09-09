#!/usr/bin/env python3
"""Full B28+C5 paired bookkeeping-neutrality gate against immutable6424."""
import datetime
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6427-activation-stamp-BC-gate'
parent = root / 'benchmarks/runs/20260909-6424-activation-stamp-build'
authority = root / 'benchmarks/runs/20260909-6571-material-state-build'
c = root / 'benchmarks/runs/20260909-6574-material-state-BC-gate'
assert not c.exists()
b = json.loads((authority / 'manifest.json').read_text())
p = json.loads((parent / 'manifest.json').read_text())
for build in (b, p):
    for rel, digest in build['files'].items():
        assert sha(root / build['source'] / rel) == digest, rel
    for rel, digest in build['binaries'].items():
        assert sha(root / rel) == digest, rel
assert (root / 'benchmarks/runs/20260909-6573-material-shared-touch-control/summary.json').exists()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old / rel) == digest, rel
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for name in ('drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(root / 'tools/bench/runtime_task.py', c / 'pins/runtime_task.py')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6114-substitute-publication-BC', '6574-material-state-BC')
(c / 'array.sbatch').write_text(batch)
arms = {}
for label, build, prefix in (('current', p, '6424-activation-stamp'), ('candidate', b, '6571-material-state')):
    build_dir = parent if label == 'current' else authority
    shutil.copy2(build_dir / 'cumulative.patch', c / 'pins' / (label + '.patch'))
    for tier, kind in (('B', 'release'), ('C', 'bc')):
        binary = root / ('build/pin-' + prefix + '-' + kind + '/tactix')
        arm = label + '_' + tier
        shutil.copy2(binary, c / 'pins' / arm)
        arms[arm] = dict(path='pins/' + arm, sha256=sha(binary), arguments=[],
            source_commit=build['source_commit'], source_build=str(binary.parent.relative_to(root)),
            source_patch='pins/' + label + '.patch', source_patch_sha256=sha(build_dir / 'cumulative.patch'))
m.update(arms=arms, predecessor=str(old.relative_to(root)),
    candidate_authority=str(authority.relative_to(root)), candidate_source=b['source'],
    current_authority=str(parent.relative_to(root)),
    purpose='Approved stage-one B28+C5 gate and exact work/certificate-neutrality control against6424; no scheduling consumer enabled.',
    hypothesis='Separate material bookkeeping preserves every existing non-timing work counter and final certificate on every paired case. Any mismatch blocks cutover pending explanation; no performance improvement assumed.',
    validation='66 independently checked/clean records required; provisional matching solve outcomes and certificate hashes permit investigation while checks continue. Compare all paired non-timing work and certificates before scheduling cutover.',
    collect_runtime_context=True, test_consideration=b['test_consideration'],
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat())
m['pin_sha256'] = {str(path.relative_to(c)): sha(path) for path in [*sorted((c / 'pins').iterdir()), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
for path in (c / 'pins').iterdir():
    path.chmod(path.stat().st_mode & ~0o222)
print(c)
