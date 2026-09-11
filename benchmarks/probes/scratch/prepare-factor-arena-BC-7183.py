#!/usr/bin/env python3
"""Full unchanged B28+C5 correctness gate for the isolated occurrence arena."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260910-7183-factor-arena-BC'
assert not c.exists()
old = root / 'benchmarks/runs/20260910-7166-vivify-revalidation-BC-gate'
om = json.loads((old / 'manifest.json').read_text())
dependencies = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    dependencies[str(p.relative_to(root))] = digest
bind(old / 'manifest.json')
for rel, digest in om['pin_sha256'].items(): bind(old / rel, digest)
for task in range(66):
    p = old / f'results/{task:03d}.json'
    assert classify(json.loads(p.read_text()), True)['checked'], p
for stem in ('7181-factor-arena-proofs', '7182-factor-arena-sanitizers'):
    p = root / ('benchmarks/runs/20260910-' + stem + '/summary.json')
    g = json.loads(p.read_text()); assert g['status'] == 'PASS'; bind(p)
    for rel, digest in g['hashes'].items(): bind(root / rel, digest)
    if stem.startswith('7181'):
        assert g['checked'] == 288 and g['negative_controls'] == 2 and g['exact_parent_certificates']
authority = root / 'benchmarks/runs/20260910-7180-factor-arena-build'
b = json.loads((authority / 'manifest.json').read_text()); bind(authority / 'manifest.json')
for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
for kind, count in (('release', 116), ('bc', 116), ('trace', 115)):
    p = authority / f'logs/{kind}-ctest.log'
    assert f'100% tests passed out of {count}' in p.read_text(); bind(p)
for folder in ('pins', 'logs', 'claims', 'results', 'provisional'): (c / folder).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
arms = {}
for arm, stem, kind in (('current_B', '7162-vivify-revalidation', 'release'),
                        ('current_C', '7162-vivify-revalidation', 'bc'),
                        ('candidate_B', '7180-factor-arena', 'release'),
                        ('candidate_C', '7180-factor-arena', 'bc')):
    bm_path = root / ('benchmarks/runs/20260910-' + stem + '-build/manifest.json')
    bm = json.loads(bm_path.read_text()); bind(bm_path)
    binary = root / ('build/pin-' + stem + '-' + kind + '/tactix')
    bind(binary, bm['binaries'][str(binary.relative_to(root))])
    arms[arm] = dict(path='pins/'+arm, sha256=sha(binary), arguments=[],
        source_commit=bm['source_commit'], source_build=str(binary.parent.relative_to(root)))
    shutil.copy2(binary, c / 'pins' / arm)
assert arms['current_B']['sha256'] == arms['current_C']['sha256']
assert arms['candidate_B']['sha256'] == arms['candidate_C']['sha256']
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('7166-vivify-revalidation-BC', '7183-factor-arena-BC')
with (c / 'array.sbatch').open('x') as out: out.write(batch)
bind(Path(__file__))
m = dict(schema='tactix-factor-arena-BC-v1', arms=arms, input_sha256=om['input_sha256'],
    collect_runtime_context=True, resource_contract=om['resource_contract'], dependencies=dependencies,
    hypothesis=b['hypothesis'], test_consideration=b['test_consideration'],
    acceptance='All66 expected, independent models/proofs and clean final state. Exact complete non-timing statistics and certificate bytes/hash across arms. No policy/semantic drift accepted for storage change. Pending proofs permit independent work, not final correctness/promotion.',
    next='After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out: json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
