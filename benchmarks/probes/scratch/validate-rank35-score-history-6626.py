#!/usr/bin/env python3
"""Replay first, then localize all selected score-state fields before the split."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from score_history import read, compare

c = root / 'benchmarks/runs/20260909-6624-rank35-score-history'
target = c / 'summary-6626.json'
assert not target.exists()
arms = ('candidate', 'previous')
if any(not (c / f'results/{arm}.json').exists() for arm in arms):
    print('Score histories still pending; no summary written')
    sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(p, digest=None):
    actual = sha(p)
    if digest is not None:
        assert actual == digest, p
    hashes[str(p.relative_to(root))] = actual
    return actual
for rel, digest in m['hashes'].items():
    bind(c / rel, digest)
for rel, digest in m['dependencies'].items():
    bind(root / rel, digest)
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
parent = root / m['cached_controls']
histories = {}; replays = {}
for arm in arms:
    p = c / f'results/{arm}.json'; bind(p)
    r = json.loads(p.read_text())
    old = json.loads((parent / f'results/{arm}.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['arm'] == arm
    assert r['binary_sha256'] == old['binary_sha256'] == sha(c / 'pins' / arm)
    assert r['partial_certificate'] == old['partial_certificate']
    assert set(r['trace_sha256']) - set(old['trace_sha256']) == {arm + '.scores.tsv'}
    for name, digest in r['trace_sha256'].items():
        bind(c / 'traces' / name, digest)
    for name, digest in old['trace_sha256'].items():
        assert r['trace_sha256'][name] == digest, (arm, name, 'Replay changed: stop interpretation')
    histories[arm] = read((c / f'traces/{arm}.scores.tsv').read_text(), solver='tactix', external=m['score_variable'])
    replays[arm] = dict(partial_proof_equal=True, ordinary_heap_choice_equal=True)
out = dict(rank=35, variable=m['score_variable'], complete_own_replay=True, replay=replays,
    cutoff_before_first_split=[3074183, 161631565],
    row_counts={arm: len(rows) for arm, rows in histories.items()},
    counter_resets={arm: [dict(previous=a, current=b) for a, b in zip(rows, rows[1:])
                         if b['decisions'] < a['decisions']] for arm, rows in histories.items()},
    comparison=compare(histories['candidate'], histories['previous'], before=(3074183, 161631565)),
    selected_reorder={arm: [r for r in rows if r['conflicts'] == 3074062 and r['label'].startswith('reorder_')]
                      for arm, rows in histories.items()}, hashes=hashes,
    limitation='Every emitted history row validated after full own replay; repeated/unmatched sites retained. Same macro/slot snapshots bracket an update, not exact mutation causality or full-formula identity. Warm-up decision resets at conflict0 are explicit. Rows after the first split are validated but excluded from causal bracketing. Partial proof is not final acceptance.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps({key: value for key, value in out.items() if key not in ('hashes', 'comparison')}, indent=2))
print(json.dumps({key: value for key, value in out['comparison'].items() if key != 'unmatched'}, indent=2))
