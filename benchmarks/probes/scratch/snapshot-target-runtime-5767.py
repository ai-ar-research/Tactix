#!/usr/bin/env python3
"""Freeze only a complete40 candidate score; seal B28+C5 independently."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root / 'tools/bench'))
import fixed_runtime

source=Path(__file__).with_name('seal-factor-support-5602.py').read_text()
source=source.replace('20260908-5598-factor-support-BC-gate','20260908-5754-reuse-target-BC-gate')
exec(compile(source,__file__,'exec'),{'__file__':__file__})
c=root / 'benchmarks/runs/20260908-5758-reuse-target-ranks1-40'
output=fixed_runtime.score(c)
print('5758 records',output['records'],'checks',output['checked_clean'],'ratio',output['ratio'])
p=c / 'solver-checkpoint.json'
if output['complete'] and not p.exists():
    output['observed_at']=dt.datetime.now(dt.timezone.utc).isoformat()
    output['authority']={}
    for file in [c / 'manifest.json',c / 'matrix.tsv',*sorted((c / 'provisional').glob('*.json'))]:
        output['authority'][str(file.relative_to(root))]=hashlib.sha256(file.read_bytes()).hexdigest()
    p.write_text(json.dumps(output,indent=2)+'\n')
    print('Frozen complete40 checkpoint; do not overwrite')
