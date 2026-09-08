#!/usr/bin/env python3
"""Stage audited completed normal evidence; keep large raw traces local and hashed."""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[3]
audit = root/'benchmarks/reports/2026-09-08-factor-input-validation.json'
record = json.loads(audit.read_text())
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()
paths=['PROGRESS.md',str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-factor-input-and-rank33.md',
    'benchmarks/reports/2026-09-08-factor-input-main-ctest.log',
    'benchmarks/probes/scratch/stage-census-5557.py']
for n in range(5564,5577):
    paths += [str(p.relative_to(root)) for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.py')]
for item in record['campaigns']:
    c=root/item['campaign']
    assert sha(c/'manifest.json')==item['manifest_sha256']
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for folder,key in [('results','result_sha256'),('provisional','provisional_sha256')]:
        for name,digest in item[key].items():
            p=c/folder/name
            assert sha(p)==digest
            paths.append(str(p.relative_to(root)))
            if folder!='results':continue
            r=json.loads(p.read_text())
            for name,digest in r.get('trace_sha256',{}).items():
                p=c/'traces'/name
                if name.endswith(('.prefix.tsv','.boundaries.tsv','.macro.tsv','.factor.tsv','.heap.tsv')):
                    assert sha(p)==digest
                    paths.append(str(p.relative_to(root)))
            profile=r.get('solve',{}).get('profile')
            if profile:
                for name,report in profile['reports'].items():
                    p=c/'profiles'/(f"{r['task']:03d}-"+name+'.txt')
                    assert sha(p)==report['sha256']
                    paths.append(str(p.relative_to(root)))
    if any('-'+str(n)+'-' in c.name for n in (5564,5565,5567,5569,5571)):
        paths += [str((c/name).relative_to(root)) for name in ('run.py','array.sbatch')]
assert all((root/name).is_file() for name in paths)
assert not any('periodic-fuzz/' in name or name.endswith(('-response.json','-stderr.log','.decisions.tsv')) for name in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit audited files:',len(set(paths)))
