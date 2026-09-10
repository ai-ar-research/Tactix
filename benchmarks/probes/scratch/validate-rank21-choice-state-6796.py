#!/usr/bin/env python3
"""Accept selected assignment/queue rows only after exact whole6751 replay."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from choice_state import read_choice_state
c = root/'benchmarks/runs/20260909-6794-rank21-choice-state'
target = c/'summary-6796.json'
assert not target.exists()
if not all((c/f'results/{arm}.json').exists() for arm in ('candidate','kissat')):
    print('Choice-state outcomes pending; no summary written')
    raise SystemExit(0)
m = json.loads((c/'manifest.json').read_text())
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
arms = {}
for arm in ('candidate','kissat'):
    p = c/f'results/{arm}.json'; bind(p)
    r = json.loads(p.read_text())
    old_path = root/m['cached_controls'][arm]/f'results/{arm}.json'; bind(old_path)
    old = json.loads(old_path.read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert r['binary_sha256'] == old['binary_sha256'] == m['hashes']['pins/'+arm]
    assert r['partial_certificate'] == old['partial_certificate']
    if arm == 'kissat': assert r['prior_certificate_prefix'] == old['prior_certificate_prefix']
    assert set(r['trace_sha256']) == set(old['trace_sha256']) | {arm+'.state.tsv'}
    for rel,digest in old['trace_sha256'].items():
        assert r['trace_sha256'][rel] == digest, (arm,rel)
    for rel,digest in r['trace_sha256'].items(): bind(c/'traces'/rel,digest)
    rows = read_choice_state((c/f'traces/{arm}.state.tsv').read_text(),
        conflict=m['observed_conflict'], variable=m['observed_variable'])
    selected = []
    with (c/f'traces/{arm}.decisions.tsv').open() as stream:
        for line in stream:
            row = [int(value) for value in line.split('\t')]
            assert len(row) == 5
            if row[0] > m['observed_conflict']: break
            if row[0] == m['observed_conflict']: selected.append(row)
    assert len(rows) == len(selected) and rows
    for observed, decision in zip(rows,selected):
        assert [observed[key] for key in ('conflicts','decisions','level','chosen')] == decision[:4]
        assert observed['stable'] == 0 and decision[4] == 0
    contexts = [r for r in rows if 25451308 <= r['decisions'] <= 25451311]
    assert {25451309,25451310} <= {r['decisions'] for r in contexts}
    arms[arm] = dict(rows=len(rows), contexts=contexts, exact_original_traces=list(old['trace_sha256']),
        partial_proof_exact=True, scratch_deleted=True)
for p in (Path(__file__), root/'tools/bench/choice_state.py', root/'tools/bench/test_choice_state.py'): bind(p)
out = dict(hashes=hashes, observed_variable=m['observed_variable'], observed_conflict=m['observed_conflict'], arms=arms,
    conclusion='Both complete original50K streams/prefixes/partial proofs and boundary/macro traces replay exactly. Every selected state row matches its decision record; assignment/reason and focused queue fields may now be interpreted.',
    limitation='State is recorded at the decision callback, before propagation from that choice. Heap indices and absolute queue clocks are representation metadata, not a cross-solver cause by themselves. No final proof/runtime acceptance or repair inferred.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(json.dumps(arms,indent=2))
