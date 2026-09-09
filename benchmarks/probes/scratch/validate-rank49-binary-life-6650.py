#!/usr/bin/env python3
"""Seal complete replay and selected four-pair mutation provenance."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from binary_lifecycle import read_lifecycle
c=root/'benchmarks/runs/20260909-6648-rank49-binary-life'
target=c/'summary-6650.json';assert not target.exists()
arms=('candidate','kissat')
if any(not (c/f'results/{arm}.json').exists() for arm in arms):
    print('Binary lifecycles pending; no summary written');sys.exit(0)
m=json.loads((c/'manifest.json').read_text());hashes={}
def bind(p,expected=None):
    actual=sha(p);assert expected is None or actual==expected,p
    hashes[str(p.relative_to(root))]=actual
for rel,digest in m['hashes'].items():bind(c/rel,digest)
for rel,digest in m['dependencies'].items():bind(root/rel,digest)
for name in ('manifest.json','launch.json'):bind(c/name)
parent=root/m['cached_controls'];rows={};pairs={tuple(p) for p in m['selected_pairs']}
for arm in arms:
    p=c/f'results/{arm}.json';bind(p)
    r=json.loads(p.read_text());old=json.loads((parent/f'results/{arm}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==arm
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
    assert r['partial_certificate']==old['partial_certificate']
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={arm+'.lifecycle.tsv'}
    for name,digest in r['trace_sha256'].items():bind(c/'traces'/name,digest)
    for name,digest in old['trace_sha256'].items():assert r['trace_sha256'][name]==digest,name
    with (c/f'traces/{arm}.lifecycle.tsv').open() as stream:
        rows[arm]=read_lifecycle(stream,'tactix' if arm=='candidate' else 'kissat',pairs)
out=dict(rank=49,pairs=m['selected_pairs'],full_own_replay=True,rows=rows,
    through_first_probe={arm:[r for r in rr if r['conflicts']<=2988] for arm,rr in rows.items()},hashes=hashes,
    limitation='Portable pair mutation provenance after full replay; internal refs/id/copy sentinels are observer metadata. Missing native rows are not an invented mutation. Read rule/source semantics before choosing a repair. No source change or runtime improvement.')
with target.open('x') as stream:json.dump(out,stream,indent=2);stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','rows')},indent=2))
