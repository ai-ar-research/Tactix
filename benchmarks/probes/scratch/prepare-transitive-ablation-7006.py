#!/usr/bin/env python3
"""Two-rank, four-arm causal diagnostic; no solver-source or default mutation."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha

current = root / 'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
native = root / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
profile = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles'
c = root / 'benchmarks/runs/20260910-7006-transitive-largest-gap-ablation'
assert not c.exists()
cm = json.loads((current / 'manifest.json').read_text())
nm = json.loads((native / 'manifest.json').read_text())
ps = json.loads((profile / 'solver-summary-7004.json').read_text())
assert set(ps['arms']) == {'31/candidate', '31/kissat', '35/candidate', '35/kissat'}
for rel, digest in ps['hashes'].items():
    assert sha(root / rel) == digest, rel
for campaign, manifest in ((current, cm), (native, nm)):
    for rel, digest in manifest['pin_sha256'].items():
        assert sha(campaign / rel) == digest, rel
for task in range(4):
    p = profile / f'results/{task:03d}.json'
    if not p.exists(): p = profile / f'provisional/{task:03d}.json'
    assert classify(json.loads(p.read_text()), p.parent.name == 'results')['status'] in ('checked_UNSAT', 'pending_validation')

names = ('candidate_on', 'kissat_on', 'candidate_off', 'kissat_off')
rows, records = [], {}
with (native / 'matrix.tsv').open(newline='') as stream:
    inputs = {int(r['rank']): r for r in csv.DictReader(stream, delimiter='\t') if r['arm'] == 'tactix'}
for rank in (31, 35):
    for name in names:
        candidate = name.startswith('candidate')
        p = (current / f'results/{rank-1:03d}.json' if candidate
             else native / f'results/{2*rank-1:03d}.json')
        record = json.loads(p.read_text())
        state = classify(record, True)
        assert state['checked'] and state['clean'] and record['expected'] == 'UNSAT'
        assert record['instance'] == inputs[rank]['instance'] and record['rank'] == rank
        expected = cm['arms']['tactix']['sha256'] if candidate else nm['arms']['kissat']['sha256']
        assert record['binary_sha256'] == expected
        records[f'{rank}/{name}'] = dict(path=str(p.relative_to(root)), sha256=sha(p))
        rows.append(dict(task=len(rows), rank=rank, arm=name, tier='D',
                         instance=record['instance'], expected='UNSAT'))
assert len(rows) == 8 and len({r['instance'] for r in rows}) == 2
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'checkmodel', 'modelcheck.py', 'drat-trim'):
    shutil.copy2(native / 'pins' / name, c / 'pins' / name)
shutil.copy2(root / 'tools/bench/solver_environment.py', c / 'pins/solver_environment.py')
shutil.copy2(Path(__file__).with_name('transitive-ablation-task-7006.py'), c / 'pins/run.py')
arms = {}
for name in names:
    candidate = name.startswith('candidate')
    origin = cm['arms']['tactix'] if candidate else nm['arms']['kissat']
    binary = current / 'pins/tactix' if candidate else native / 'pins/kissat'
    shutil.copy2(binary, c / 'pins' / name)
    arguments = list(origin['arguments'])
    if name == 'kissat_off': arguments.append('--transitive=0')
    environment = {'TACTIX_SLOT_DISABLE': 'TransitiveReduce'} if name == 'candidate_off' else {}
    arms[name] = dict(origin, path='pins/' + name, arguments=arguments, environment=environment)
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (native / 'array.sbatch').read_text()
assert batch.count('--array=0-99') == 1
batch = batch.replace(str(native), str(c)).replace('--array=0-99', '--array=0-7')
batch = batch.replace('/pins/runtime_task.py', '/pins/run.py').replace('6121-fresh-accepted50', '7006-transitive-ablation')
(c / 'array.sbatch').write_text(batch)
dependencies = {}
for p in (Path(__file__), Path(__file__).with_name('transitive-ablation-task-7006.py'),
          root / 'tools/bench/solver_environment.py', root / 'tools/bench/test_solver_environment.py',
          profile / 'solver-summary-7004.json', current / 'manifest.json', native / 'manifest.json',
          root / 'benchmarks/runs/20260908-5910-preclean-rank31-choice/choice-validation.json',
          root / 'benchmarks/runs/20260909-6637-rank35-binary-life/summary-6642.json',
          root / '.claude/worktrees/runtime-reduce-watermark-6730/src/policies/default_policy.cpp',
          root / 'kissat/src/options.h'):
    dependencies[str(p.relative_to(root))] = sha(p)
protected = json.loads((current / 'launch.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root / rel) == digest, rel
m = dict(schema='tactix-transitive-largest-gap-ablation-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, input_sha256={r['instance']: nm['input_sha256'][r['instance']] for r in rows},
    collect_runtime_context=True, resource_contract=dict(nm['resource_contract'], array='0-7'),
    dependencies=dependencies, protected_campaign_hashes=protected, unprofiled_records=records,
    purpose='Test whether TransitiveReduce downstream search effects materially explain31/35 excess work, rather than its small direct cost. Four on/off solver combinations, not a default change.',
    hypothesis='Rank35 legitimate extra deletion precedes its selected later Reorder/choice split. Rank31 has a separate known cleanup omission and serves as transfer control; do not assume its cause is TransitiveReduce. Profile7004 confirms dominant search cost with nearly twice native conflicts.',
    comparison='Per rank compare each solver off/on conflicts, decisions, propagation and runtime; compare the two off/on ratios. Require on-arm exact own reference work/certificate replay and source-backed zero TransitiveReduce activation on off arms. Report all losses/timeouts; one wave is causal screening, not timing acceptance.',
    decision='If turning the mechanism off selectively removes substantial Tactix excess search, localize the responsible on-path simplification/scheduling difference. Otherwise reject this explanation and prioritize another demonstrated worse-work boundary. Never adopt feature-off from this projection.',
    validation='All expected solver verdicts independently checked and zero residue; pending proofs permit provisional work only. Source and binary hashes are unchanged. Off-work/certificates may differ; on arms must exactly replay their own checked6743/6121 records. Verify native option and zero transitive counters plus current zero applications/checks/ticks.',
    test_consideration='Six new environment-scope tests cover exact path matching, restoration on success/failure, absent/pre-existing values, checker isolation, empty settings, immutable inputs and rejected malformed/non-solver keys. Reuse262 benchmark and22 watcher tests. No solver semantics or rule guard changed.',
    authorization='Explicit user resumed after06:00 and prioritized largest gaps. Existing strategy diagnostic overrides only. Rank21 remains shelved; no promotion, FlatLists, fuzz access, denied push/transfer retry or fixed7239.010s denominator refresh.',
    session_authority='Explicit user continue after Sep10 06:00EDT; replacement stopping time pending. Fresh isolated diagnostic only.',
    selection=[dict(rank=31), dict(rank=35)])
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
