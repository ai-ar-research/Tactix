#!/usr/bin/env python3
"""Price same-scope timers across all predeclared exact-work Vivify waves."""
from pathlib import Path
import json
import statistics
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from inprocessing_cost import read_timers
c = root/'benchmarks/runs/20260910-6831-vivify-snapshot-runtime'
checkpoint = c/'solver-checkpoint-6833.json'
s = json.loads(checkpoint.read_text())
assert s['solver_outcomes'] == 18 and s['exact_per_arm_work_and_proof']
for rel, digest in s['hashes'].items(): assert sha(root/rel) == digest
rows = {}
for rel in s['hashes']:
    p = root/rel
    if p.parent.name not in ('provisional','results'): continue
    r = json.loads(p.read_text())
    if r['task'] in rows and p.parent.name == 'provisional': continue
    rows[r['task']] = r
assert set(rows) == set(range(18))
out = {}
for rank in (32,39,38):
    waves = []
    for wave in range(3):
        arms = {r['arm']:r for t,r in rows.items() if t//6 == wave and r['rank'] == rank}
        timers = {a:read_timers(r['solver_statistics']) for a,r in arms.items()}
        values = {}
        for a,t in timers.items():
            values[a] = dict(vivify_ms=t['rules']['Vivify']['ms'], slot_ms=t['slot_ms'], outside_ms=t['outside_ms'],
                vivify_applications=t['rules']['Vivify']['applications'], peak_rss_kib=arms[a]['solve']['peak_rss_kb'])
        assert values['candidate']['vivify_applications'] == values['previous']['vivify_applications']
        ratios = {k:values['candidate'][k]/values['previous'][k] for k in ('vivify_ms','slot_ms','outside_ms','peak_rss_kib')}
        waves.append(dict(wave=wave, values=values, ratios=ratios))
    out[rank] = dict(waves=waves, paired={k:dict(median=statistics.median(w['ratios'][k] for w in waves),
        min=min(w['ratios'][k] for w in waves),max=max(w['ratios'][k] for w in waves)) for k in waves[0]['ratios']})
result = dict(checkpoint=dict(path=str(checkpoint.relative_to(root)),sha256=sha(checkpoint)),ranks=out,
    conclusion='No reliable total-runtime gain: Rank32 is slower in all three pairs,39 is near neutral and38 mixed. Do not adopt6822 or repeat this campaign to select favorable noise. The local RSS decrease is recorded, not a general memory-parity claim. Pending independent certificates remain open;6730 stays current.',
    test_consideration='Unchanged maintained read_timers parser has five boundary/malformed/rounding tests. Strict6833 already verifies all18 exact work/certificates and matrix; helper asserts full cardinality and equal application counts.')
with (c/'timer-cost-6849.json').open('x') as stream:
    json.dump(result,stream,indent=2); stream.write('\n')
print(json.dumps({rank:r['paired'] for rank,r in out.items()},indent=2))
