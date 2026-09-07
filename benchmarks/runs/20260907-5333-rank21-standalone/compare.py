#!/usr/bin/env python3
"""Require checked completion, then locate the first Tactix boundary split."""
import hashlib
import json
from pathlib import Path

C=Path(__file__).resolve().parent
manifest=json.loads((C/'manifest.json').read_text())
result=[json.loads((C/'results'/f'{i:03d}.json').read_text()) for i in range(3)]
rows,hashes={},{}
for i,r in enumerate(result):
    assert r['task']==i and r['rank']==21 and r['valid']
    assert r['solve']['exit_code']==10 and not r['solve']['timed_out']
    assert r['model_check']['result']=='verified' and r['model_check']['exit_code']==0
    assert r['artifact_deleted'] and not r['cleanup_pending']
    arm=r['arm']; assert r['binary_sha256']==manifest['arms'][arm]['sha256']
    raw=(C/'traces'/(arm+'.tsv')).read_bytes()
    rows[arm]=[line.split('\t') for line in raw.decode().splitlines() if not line.startswith('#')]
    assert rows[arm]
    hashes[arm]=hashlib.sha256(raw).hexdigest()
a,b=rows['core'],rows['sequential']
first=next((dict(index=i,core=x,sequential=y)
            for i,(x,y) in enumerate(zip(a,b)) if x[2:]!=y[2:]),None)
first_state=next((dict(index=i,core=x,sequential=y)
                  for i,(x,y) in enumerate(zip(a,b)) if x[2:13]!=y[2:13]),None)
sweeps={arm:[r for r in stream if r[2].lower()=='sweep'] for arm,stream in rows.items()}
print(json.dumps(dict(checked_clean=3,row_counts={a:len(r) for a,r in rows.items()},
    first_core_sequential_difference=first,first_non_tick_difference=first_state,
    sweeps=sweeps,trace_sha256=hashes,
    caveat='Standalone boundary counts localize the changed transaction; they do not prove ordered clause identity. Instrumented times are not timing acceptance.'),indent=2))
