#!/usr/bin/env python3
"""Validate current full replay without presuming old candidate identity."""
from collections import Counter
import json
from pathlib import Path
import runpy
support = runpy.run_path(str(Path(__file__).with_name('compare-rank33-congruence-5583.py')))
s = support['support']
s['fixtures']()
support['fixtures']()
root = s['root']
c = root / 'benchmarks/runs/20260908-5611-current-congruence-output'
if len(list((c / 'results').glob('*.json'))) != 2:
    print('5611 pending'); raise SystemExit(0)
m = json.loads((c / 'manifest.json').read_text())
parent = root / m['parent_campaign']
assert s['sha'](parent / 'clause-localization.json') == m['parent_summary_sha256']
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix', c.name)
exec(compile(source, __file__, 'exec'), {})
same = {}
for arm in ('candidate', 'kissat'):
    now = json.loads((c / 'results' / (arm + '.json')).read_text())
    old = json.loads((parent / 'results' / (arm + '.json')).read_text())
    same[arm] = {name: digest == old['trace_sha256'].get(name)
                 for name, digest in now['trace_sha256'].items()}
    same[arm]['partial_proof'] = now['partial_certificate'] == old['partial_certificate']
    if arm == 'kissat': assert all(same[arm].values())
clauses = {arm: Counter(s['formula']((c / 'traces' / (arm + '.formula.tsv')).read_text()))
           for arm in ('candidate', 'kissat')}
binaries = {arm: support['binary_snapshots']((c / 'traces' / (arm + '.binary.tsv')).read_text(),
            'tactix' if arm == 'candidate' else arm) for arm in ('candidate', 'kissat')}
assert len(binaries['candidate']) == len(binaries['kissat'])
ac, bc = clauses['candidate'], clauses['kissat']
result = dict(parent_artifact_identity=same, parser_fixtures=27,
    large_clause_difference={side: [dict(redundant=k[0], literals=k[1], copies=v)
        for k, v in sorted(diff.items())] for side, diff in [('candidate_only', ac-bc), ('kissat_only', bc-ac)]},
    binary_difference_by_pass=[s['multiset_difference'](x, y) for x,y in zip(binaries['candidate'], binaries['kissat'])],
    limitation='Current-source revalidation of actual clause difference; not proof of deletion cause or later decision causality. Full prefix comparison is separate summary.json; no complete verdict or runtime claim.')
p = c / 'clause-localization.json'
if p.exists(): assert json.loads(p.read_text()) == json.loads(json.dumps(result))
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
