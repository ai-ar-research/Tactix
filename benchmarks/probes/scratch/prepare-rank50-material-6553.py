#!/usr/bin/env python3
"""One native producer diagnostic with exact cached-control replay required."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from material_trace import parse_material
old = root / 'benchmarks/runs/20260909-6286-rank50-admission'
build = root / 'benchmarks/runs/20260909-6550-native-material-observer-build'
c = root / 'benchmarks/runs/20260909-6553-rank50-material-producers'
assert not c.exists()
b = json.loads((build / 'manifest.json').read_text())
for rel, digest in b['files'].items(): assert sha(root / b['source'] / rel) == digest, rel
assert sha(root / b['binary']['path']) == b['binary']['sha256']
parse_material((build / 'fixture.tsv').read_text(), 2, 3, 9)
parent = json.loads((old / 'manifest.json').read_text())
control = json.loads((old / 'results/kissat.json').read_text())
assert control['exit_code'] == 0 and control['scratch_deleted']
assert control['partial_certificate']['bytes'] > 0
for name, digest in control['trace_sha256'].items(): assert sha(old / 'traces' / name) == digest, name
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
shutil.copy2(root / b['binary']['path'], c / 'pins/kissat')
runner = (old / 'run.py').read_text()
for before, after in {
    "('candidate', 'candidate_control', 'kissat')": "('kissat',)",
    'tactix-6286-': 'tactix-6553-',
}.items():
    assert runner.count(before) == 1
    runner = runner.replace(before, after)
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''env['KISSAT_SUBSUME_MATERIAL_TRACE'] = str(c / 'traces/kissat.material.tsv')
env['SAT_SUBSUME_MATERIAL_FROM'] = str(m['material_from'])
env['SAT_SUBSUME_MATERIAL_TO'] = str(m['material_to'])
env['SAT_SUBSUME_MATERIAL_CONFLICT_TO'] = str(m['admission_conflict'])
''' + anchor)
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6286-rank50-admission', '6553-rank50-material').replace('--array=0-2', '--array=0-0')
assert '--array=0-0' in batch and '--cpus-per-task=12' in batch and '--mem=48000M' in batch
(c / 'array.sbatch').write_text(batch)
keys = ('rank', 'input', 'input_sha256_compressed', 'stop_conflict', 'stream_from',
        'common_decisions', 'selection_sha256', 'earlier_closure', 'admission_conflict')
m = {key: parent[key] for key in keys}
m.update(resource_contract=dict(parent['resource_contract'], array='0-0'),
    material_from=2508, material_to=2510,
    observer_build=dict(path=str((build / 'manifest.json').relative_to(root)), sha256=sha(build / 'manifest.json')),
    cached_control=dict(campaign=str(old.relative_to(root)), result_sha256=sha(old / 'results/kissat.json'), summary_sha256=sha(old / 'summary.json')),
    protected_campaign_hashes=json.loads((root / 'benchmarks/reports/2026-09-09-factor-material-audit-6548.json').read_text())['protected_campaign_hashes'],
    purpose='Identify actual native added-material producers for the observed Rank50 admission-counter increase, not implement its Tactix tracker.',
    validation='Require all ordinary traces including admission and the partial certificate to match cached6286 native exactly; verify inherited prefix and all three consecutive material-counter rows before interpretation. Partial proof hash/deletion is not final-proof acceptance.',
    test_consideration='Actual producer callback fixture covers inclusive counter/conflict filters, repeated/rearmed suppression, signed export, fixed flag, no mutation and no-overwrite. Nine strict material-reader tests. Only a native observer changes; Tactix sources and all reference timings remain frozen.',
    limitation='Native-only bounded diagnostic. No competitiveness timing, final SAT/UNSAT verdict, fuzz acceptance or approved policy redesign. 1200 seconds solver-only; 25-minute allocation; no node constraint/throttle.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in (c / 'run.py', c / 'array.sbatch', c / 'pins/kissat')}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
(c / 'pins/kissat').chmod((c / 'pins/kissat').stat().st_mode & ~0o222)
print(c)
