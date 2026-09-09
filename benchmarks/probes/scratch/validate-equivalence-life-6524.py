#!/usr/bin/env python3
"""Require full own replay before identifying selected equivalence producer labels."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from binary_lifecycle import read_lifecycle
c=root/'benchmarks/runs/20260909-6522-equivalence-life'
target=c/'summary.json'; assert not target.exists()
if any(not (c/'results'/f'{a}.json').exists() for a in ('candidate','kissat')):
    print('Equivalence lifecycle pending; no output created'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text()); hashes={}; rows={}
def bind(p):
    h=sha(p); hashes[str(p.relative_to(root))]=h; return h
for rel,digest in m['hashes'].items(): assert bind(c/rel)==digest
bind(c/'manifest.json'); bind(c/'launch.json')
b=m['parent_summary']; assert bind(root/b['path'])==b['sha256']
for arm,solver in (('candidate','tactix'),('kissat','kissat')):
    parent=root/m['parent_runs'][arm]
    p=c/'results'/f'{arm}.json'; q=parent/'results'/p.name
    r=json.loads(p.read_text()); old=json.loads(q.read_text()); bind(p); bind(q)
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==old['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==old['partial_certificate']
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={f'{arm}.equivalence-life.tsv'}
    for name,digest in old['trace_sha256'].items(): assert bind(parent/'traces'/name)==digest==r['trace_sha256'][name],name
    for name,digest in r['trace_sha256'].items(): assert bind(c/'traces'/name)==digest,name
    with (c/'traces'/f'{arm}.equivalence-life.tsv').open() as f:
        rows[arm]=read_lifecycle(f,solver,{(-91779,91609),(-91609,91779)})
    print(arm,'full own1M/formula/census/partial-proof replay PASS',flush=True)
out=dict(parent_replay=True,selected_pairs=[[-91779,91609],[-91609,91779]],
    total_rows={a:len(r) for a,r in rows.items()},
    selected_conflict_rows={a:[r for r in v if r['conflicts']==227617] for a,v in rows.items()},
    first_observed_event={a:v[0] if v else None for a,v in rows.items()},hashes=hashes,
    limitation='Actual observer site labels and selected-pair copy lifecycle. Same-conflict rows can span both Congruence calls and intervening rules; use exact event order and forward snapshots, not conflict alone. No proof of first internal cause, checked local derivation, final proof or runtime benefit.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','limitation')},indent=2))
