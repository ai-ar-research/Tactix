#!/usr/bin/env python3
"""Prepare fresh B28+C5 or predeclared three-wave diagnostic timing pins."""
import csv
import datetime as dt
import importlib.util
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
contract = Path(__file__).with_name('runtime-telemetry-contract-7174.py')
spec = importlib.util.spec_from_file_location('telemetry_contract', contract)
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
h.fixtures()
assert sys.argv[1:] in (['bc'], ['runtime'])
bc = sys.argv[1] == 'bc'
c = root / ('benchmarks/runs/20260910-7174-runtime-telemetry-BC' if bc else
            'benchmarks/runs/20260910-7175-runtime-telemetry-runtime')
assert not c.exists()
authority = root / 'benchmarks/runs/20260910-7172-runtime-telemetry-build'
b = json.loads((authority / 'manifest.json').read_text())
deps = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    deps[str(p.relative_to(root))] = digest
bind(authority / 'manifest.json')
for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
for rel, digest in b['binaries'].items(): bind(root / rel, digest)
for kind in ('control', 'release', 'bc'):
    p = authority / f'logs/{kind}-ctest.log'
    assert '100% tests passed out of 116' in p.read_text()
    bind(p)
old = root / 'benchmarks/runs/20260910-7166-vivify-revalidation-BC-gate'
om = json.loads((old / 'manifest.json').read_text())
bind(old / 'manifest.json')
for rel, digest in om['pin_sha256'].items(): bind(old / rel, digest)
full = root / 'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
bind(full / 'manifest.json')
bind(root / 'benchmarks/reports/2026-09-10-vivify-revalidation-full50-7168/summary.json')
for campaign in (old, full):
    for folder in ('provisional', 'results'):
        for p in (campaign / folder).glob('*.json'):
            assert classify(json.loads(p.read_text()), folder == 'results')['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), p
if not bc:
    gate = root / 'benchmarks/runs/20260910-7174-runtime-telemetry-BC'
    gm = json.loads((gate / 'manifest.json').read_text())
    for rel, digest in gm['pin_sha256'].items(): bind(gate / rel, digest)
    bind(gate / 'manifest.json')
    records = {}
    for task in range(66):
        p = gate / f'results/{task:03d}.json'
        final = p.exists()
        if not final: p = gate / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text())
        assert classify(r, final)['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), p
        bind(p)
        records[task] = r
    for task in range(0, 66, 2):
        assert h.fingerprint(records[task]) == h.fingerprint(records[task+1], True), task
for name in ('pins', 'logs', 'claims', 'provisional', 'results'): (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(contract, c / 'pins/telemetry_contract.py')
arms = {}
for arm, kind in ((('current_B', 'control'), ('current_C', 'control'),
                   ('candidate_B', 'release'), ('candidate_C', 'bc')) if bc else
                  (('previous', 'control'), ('candidate', 'release'))):
    binary = root / f'build/pin-7172-runtime-telemetry-{kind}/tactix'
    arms[arm] = dict(path='pins/'+arm, sha256=sha(binary), arguments=[],
                    source_commit=b['source_commit'], source_build=str(binary.parent.relative_to(root)),
                    telemetry_ablated=kind != 'control')
    shutil.copy2(binary, c / 'pins' / arm)
rows, oracles = [], {}
if bc:
    with (old / 'matrix.tsv').open() as stream: rows = list(csv.DictReader(stream, delimiter='\t'))
    assert len(rows) == 66 and sum(r['tier'] == 'B' for r in rows) == 56
else:
    for wave in range(3):
        for rank in (31, 35, 38):
            p = full / f'results/{rank-1:03d}.json'
            if not p.exists(): p = full / f'provisional/{rank-1:03d}.json'
            r = json.loads(p.read_text())
            oracles[str(rank)] = dict(path=str(p.relative_to(root)), sha256=sha(p))
            for arm in (('previous', 'candidate') if wave == 1 else ('candidate', 'previous')):
                rows.append(dict(task=len(rows), rank=rank, arm=arm, tier='D', instance=r['instance'], expected=r['expected']))
with (c / 'matrix.tsv').open('x', newline='') as stream:
    w = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    w.writeheader(); w.writerows(rows)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace(
    '7166-vivify-revalidation-BC', '7174-telemetry-BC' if bc else '7175-telemetry-runtime').replace(
    '--array=0-65', '--array=0-' + str(len(rows)-1))
with (c / 'array.sbatch').open('x') as stream: stream.write(batch)
inputs = om['input_sha256'] if bc else json.loads((full / 'manifest.json').read_text())['input_sha256']
bind(Path(__file__)); bind(contract)
m = dict(schema='tactix-runtime-telemetry-ablation-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, input_sha256={r['instance']: inputs[r['instance']] for r in rows},
    collect_runtime_context=True, resource_contract=dict(om['resource_contract'], array='0-'+str(len(rows)-1)),
    dependencies=deps, previous_work_oracles=oracles,
    hypothesis=b['hypothesis'], test_consideration=b['test_consideration'],
    acceptance='Every outcome expected; independent models/proofs and cleanup required for final correctness. Only c timing and exactly validated c binprop telemetry may differ. Preserve all other statistics and certificate bytes/hash. No semantic or scheduling change.',
    measurement='BC is correctness-only. Runtime has three independent full-width six-task waves, ranks31/35/38 with reversed middle arm order. Advance on exact solver outcomes, not proof completion. Report paired medians/full ranges, RSS, within-arm spread (absolute claim requires <=2%). No arbitrary minimum gain; unresolved noise is inconclusive, not evidence of zero overhead.',
    decision='Bound this instrumentation hypothesis before choosing it over larger Factor structure work. Joint ablation does not isolate individual counters versus timers. Enabled build reproduces7144 exactly. Optional telemetry fields retain layout and rule_calls remain enabled. Coarse slot/solve timing and Factor diagnostics remain. Isolated diagnostic, no default promotion/fuzz waiver.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream: json.dump(m, stream, indent=2)
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
