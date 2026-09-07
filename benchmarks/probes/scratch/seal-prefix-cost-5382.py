#!/usr/bin/env python3
"""Retain the observed common stop, work counters and trace-free diagnostic cost."""
import hashlib
import json
from pathlib import Path
import re
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5382-prefix-cost'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items(): assert sha(c / rel) == expected
arms = {}
for i, a in enumerate(('tactix', 'kissat')):
    r = json.loads((c / 'results' / (a + '.json')).read_text())
    assert r['scratch_deleted'] and r['measurement']['exit_code'] == 0 and not r['measurement']['timed_out']
    assert r['verdict'] == 'UNKNOWN' and r['binary_sha256'] == m['hashes']['pins/' + a]
    log = (c / 'logs' / f'1669003_{i}.log').read_text(); assert 'scratch_deleted True' in log
    s = '\n'.join(r['statistics'])
    counters = {n: int(re.search(r'^c ' + n + r'\s*:?\s*(\d+)', s, re.M).group(1)) for n in ('conflicts', 'decisions', 'propagations')}
    assert counters['conflicts'] == 1000002 and counters['decisions'] == 1924776
    arms[a] = dict(counters=counters, measurement=r['measurement'], node=r['node'])
result = dict(stopped_clean=2, arms=arms,
              wall_ratio=arms['tactix']['measurement']['wall_seconds'] / arms['kissat']['measurement']['wall_seconds'],
              result_sha256={p.name: sha(p) for p in (c / 'results').glob('*.json')},
              conclusion='Both actual stops overshoot the requested million by two conflicts, with identical conflict/decision counters but different propagation counts. One trace-free proof-on diagnostic shows residual cost and memory overhead; no exact-work, whole-state or decision-grade timing claim.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(result['conclusion'], result['wall_ratio'])
