#!/usr/bin/env python3
"""Check unchanged-parent replay before reading selected endpoint/admission/walks."""
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from transitive_selected import read_selected
c=root/'benchmarks/runs/20260909-6345-rank32-selected-transitive'
target=c/'summary.json';assert not target.exists()
subprocess.run(['python3','-m','unittest','discover','-s','tools/bench','-p','test_transitive*.py'],cwd=root,check=True)
if not all((c/'results'/f'{a}.json').exists() for a in ('candidate','kissat')):
    print('Both complete records required; no partial interpretation.');sys.exit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest,rel
binding=m['parent_summary'];assert sha(root/binding['path'])==binding['sha256']
hashes={str((c/n).relative_to(root)):sha(c/n) for n in ('manifest.json','launch.json')}
observed={}
for arm in ('candidate','kissat'):
    parent=root/m['parent_runs'][arm]
    q=c/'results'/f'{arm}.json';r=json.loads(q.read_text())
    p=parent/'results'/q.name;before=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['arm']==arm and r['binary_sha256']==before['binary_sha256']==sha(c/'pins'/arm)
    assert r['partial_certificate']==before['partial_certificate']
    assert set(r['trace_sha256'])==set(before['trace_sha256'])
    for name,digest in before['trace_sha256'].items():
        assert sha(parent/'traces'/name)==digest,name
        if name!=arm+'.transitive.tsv':assert r['trace_sha256'][name]==digest,name
    for name,digest in r['trace_sha256'].items():
        path=c/'traces'/name;assert sha(path)==digest;hashes[str(path.relative_to(root))]=digest
    for path in (p,q):hashes[str(path.relative_to(root))]=sha(path)
    with (c/'traces'/f'{arm}.transitive.tsv').open() as stream:
        observed[arm]=read_selected(stream,'tactix' if arm=='candidate' else 'kissat',m['diagnostic_pair'],m['diagnostic_conflict'])
    print(arm,'full ordinary trace/proof replay and selected framing PASS',flush=True)
result=dict(parent_replay=True,pair=m['diagnostic_pair'],conflict=m['diagnostic_conflict'],
    observed=observed,hashes=hashes,
    limitation='Current parent ordinary streams and partial proofs exact. Endpoint multiplicity differs by observer (Tactix once per conflict, native each invocation). Missing source is not missing endpoint; selected events do not expose the unprocessed schedule tail or prove the later decision split causal. No final-proof or timing acceptance.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({a:dict(total_rows=v['total_rows'],target_events=v['rows']) for a,v in observed.items()},indent=2))
