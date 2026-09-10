#!/usr/bin/env python3
"""Classify every arm before interpreting the isolated on/off work contrast."""
import datetime as dt
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
from runtime_summary import counters
from transitive_ablation import validate_activation

c = root / 'benchmarks/runs/20260910-7006-transitive-largest-gap-ablation'
target = c / 'solver-summary-7008.json'
assert not target.exists(), 'successful summary is frozen'
paths = []
for task in range(8):
    p = c / f'results/{task:03d}.json'
    if not p.exists(): p = c / f'provisional/{task:03d}.json'
    paths.append(p)
if not all(p.exists() for p in paths):
    print('Ablation solver records pending; no summary written')
    raise SystemExit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): bind(root / rel, digest)
for name in ('manifest.json', 'launch.json'): bind(c / name)
bind(Path(__file__))
for name in ('transitive_ablation.py', 'test_transitive_ablation.py'):
    bind(root / 'tools/bench' / name)
names = ('candidate_on', 'kissat_on', 'candidate_off', 'kissat_off')
rows = {}
for task, path in enumerate(paths):
    rank, arm = (31, 35)[task // 4], names[task % 4]
    bind(path)
    r = json.loads(path.read_text())
    state = classify(r, path.parent.name == 'results')
    assert state['status'] in ('checked_UNSAT', 'pending_validation', 'solver_timeout',
                               'pending_cleanup_solver_timeout'), (path, state)
    assert (r['task'], r['rank'], r['arm'], r['expected']) == (task, rank, arm, 'UNSAT')
    authority = m['unprofiled_records'][f'{rank}/{arm}']
    old_path = root / authority['path']; bind(old_path, authority['sha256'])
    old = json.loads(old_path.read_text())
    assert r['instance'] == old['instance']
    assert r['solve']['solver_environment'] == m['arms'][arm]['environment']
    if path.parent.name == 'results':
        assert r['binary_sha256'] == old['binary_sha256'] == m['arms'][arm]['sha256']
        if arm == 'kissat_off': assert '--transitive=0' in r['command']
    timed_out = r['solve']['timed_out']
    work = counters(r)
    if not timed_out:
        assert all(value is not None for value in work.values())
        if arm.endswith('_on'):
            for key in ('verdict', 'certificate_bytes', 'certificate_sha256'):
                assert r[key] == old[key], (rank, arm, key)
            assert work == counters(old), (rank, arm)
            if arm == 'candidate_on':
                non_timing = lambda x: [s for s in x['solver_statistics'] if not s.startswith('c timing')]
                assert non_timing(r) == non_timing(old)
    activation = None
    if not timed_out:
        activation = validate_activation(r['solver_statistics'], arm.split('_')[0], arm.endswith('_on'))
        if arm == 'kissat_off':
            assert m['arms'][arm]['arguments'] == ['-v', '--transitive=0']
    rows[f'{rank}/{arm}'] = dict(rank=rank, arm=arm, state=state, work=work,
        solve=r['solve'], activation=activation, record=str(path.relative_to(root)),
        own_default_replay=arm.endswith('_on') and not timed_out,
        certificate_sha256=r.get('certificate_sha256'))
comparisons = {}
for rank in (31, 35):
    rr = {arm: rows[f'{rank}/{arm}'] for arm in names}
    if any(r['solve']['timed_out'] for r in rr.values()):
        comparisons[str(rank)] = dict(interpretation='Censored by timeout; retain all rows and do not price missing complete work.')
        continue
    ratios = {}
    for solver in ('candidate', 'kissat'):
        on, off = rr[solver + '_on'], rr[solver + '_off']
        ratios[solver] = {key: off['work'][key] / on['work'][key] if on['work'][key] else None
                          for key in ('conflicts', 'decisions', 'propagations')}
        ratios[solver]['seconds'] = off['solve']['wall_seconds'] / on['solve']['wall_seconds']
    ratios['relative_off_on'] = {key: ratios['candidate'][key] / ratios['kissat'][key]
                                if ratios['candidate'][key] is not None and ratios['kissat'][key] else None
                                for key in ('conflicts', 'decisions', 'propagations', 'seconds')}
    comparisons[str(rank)] = ratios
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    rows=rows, comparisons=comparisons,
    limitation='One eight-task causal screen, not repeated timing acceptance. Both off arms change work; a lower ratio does not validate a default, establish all50 improvement, isolate one binary deletion, or prove global scheduling/CDCL parity. Native command is hash-bound while provisional; final records also expose the executed command. Zero tick/application checks establish the intended ablation on completed solves. Pending proofs remain unverified.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps(comparisons, indent=2))
