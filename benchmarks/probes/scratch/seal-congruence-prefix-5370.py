#!/usr/bin/env python3
"""Check the extended paired stream, without treating a stop as a verdict."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5370-congruence-prefix-extended'
old=root/'benchmarks/runs/20260907-5369-congruence-prefix'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((c/'manifest.json').read_text())
for rel,expected in m['hashes'].items(): assert sha(c/rel)==expected,rel
streams={}
for i,arm in enumerate(('tactix','kissat')):
 log=(c/'logs'/f'1668830_{i}.log').read_text()
 assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
 streams[arm]=(c/'traces'/f'{arm}.decisions.tsv').read_bytes()
 assert streams[arm].startswith((old/'traces'/f'{arm}.decisions.tsv').read_bytes())
assert streams['tactix']==streams['kissat']
n=len(streams['tactix'].splitlines());assert n==475997
result=dict(stopped_clean=2,matching_decisions=n,stop_conflicts=200000,
 preceding_prefix_retained=True,next_divergence=None,
 trace_sha256={p.name:sha(p) for p in (c/'traces').iterdir()},
 limitations='Identical decision stream, not full state identity, proof/model validation or a runtime result. UNFUZZED and unpromoted.')
p=c/'summary.json'
if p.exists(): assert json.loads(p.read_text())==result
else: p.write_text(json.dumps(result,indent=2)+'\n')
print('PASS',n,'matching decisions through conflict 200000')
