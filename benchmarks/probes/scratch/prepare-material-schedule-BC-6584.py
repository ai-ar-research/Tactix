#!/usr/bin/env python3
"""Full B28+C5 paired approved C2/Eliminate scheduling gate."""
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
authority = root / 'benchmarks/runs/20260909-6579-material-schedule-build'
c = root / 'benchmarks/runs/20260909-6584-material-schedule-BC-gate'
assert not c.exists()
b = json.loads((authority / 'manifest.json').read_text())
p = json.loads((parent / 'manifest.json').read_text())
for build in (b, p):
    for rel, digest in build['files'].items():
        assert sha(root / build['source'] / rel) == digest, rel
    for rel, digest in build['binaries'].items():
        assert sha(root / rel) == digest, rel
assert (root / 'benchmarks/runs/20260909-6582-material-proof-control/summary.json').exists()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old / rel) == digest, rel
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for name in ('drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(root / 'tools/bench/runtime_task.py', c / 'pins/runtime_task.py')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6114-substitute-publication-BC', '6584-material-schedule-BC')
(c / 'array.sbatch').write_text(batch)
arms = {}
for label, build, prefix in (('current', p, '6424-activation-stamp'), ('candidate', b, '6579-material-schedule')):
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
    purpose='Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.',
    hypothesis='New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.',
    validation='66 independently checked/clean records required. Expected solver outcomes permit provisional targeted diagnostics while checks continue; any invalidity pauses dependent work. Changed work/certificates are expected, not a neutrality requirement.',
    dependencies=['benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json', 'benchmarks/runs/20260909-6582-material-proof-control/summary.json'],
    collect_runtime_context=True, test_consideration=b['test_consideration'],
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat())
m['pin_sha256'] = {str(path.relative_to(c)): sha(path) for path in [*sorted((c / 'pins').iterdir()), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
for path in (c / 'pins').iterdir():
    path.chmod(path.stat().st_mode & ~0o222)
print(c)
