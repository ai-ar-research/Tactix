#!/usr/bin/env python3
"""Stage completed5571 only after its strict replay and all hashes validate."""
from pathlib import Path
import hashlib
import json
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5571-rank32-factor-heap'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()
summary=json.loads((c/'heap-localization.json').read_text())
assert summary['parent_replay'] and summary['unequal_scores']==[]
assert summary['candidate_only']==[-491572,119877,393688]
assert summary['kissat_only']==[394573]
m=json.loads((c/'manifest.json').read_text())
for name,digest in m['hashes'].items():assert sha(c/name)==digest
paths=[c/'heap-localization.json',root/'PROGRESS.md',Path(__file__).resolve()]
for arm,evidence in summary['evidence'].items():
    p=c/'results'/(arm+'.json')
    assert sha(p)==evidence['result_sha256']
    r=json.loads(p.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==m['hashes']['pins/'+arm]
    paths.append(p)
    for name,digest in evidence['traces'].items():
        p=c/'traces'/name
        assert sha(p)==digest
        if not name.endswith('.decisions.tsv'):paths.append(p)
subprocess.run(['git','add','-f','--',*(str(p.relative_to(root)) for p in paths)],cwd=root,check=True)
print('Staged completed late heap evidence:',len(paths))
