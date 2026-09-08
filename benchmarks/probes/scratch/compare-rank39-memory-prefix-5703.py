#!/usr/bin/env python3
"""Require exact paired logical replay before using bounded ownership samples."""
import hashlib
import json
from pathlib import Path
import runpy
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5702-rank39-memory-prefix'
support=runpy.run_path(str(root/'benchmarks/probes/scratch/validate-rank39-memory-5688.py'))
support['fixtures']()
if any(not (c/'results'/(a+'.json')).exists() for a in ('candidate','control')):
    print('5702 pending');raise SystemExit(0)
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
m=json.loads((c/'manifest.json').read_text())
for name,digest in m['hashes'].items():assert sha(c/name)==digest
assert sha(root/m['input'])==m['input_sha256_compressed']
records={}
for arm in ('candidate','control'):
    p=c/'results'/(arm+'.json');r=json.loads(p.read_text())
    assert r['arm']==arm and r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    assert r['binary_sha256']==m['hashes']['pins/'+arm] and r['partial_certificate']['bytes']>0
    for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest
    records[arm]=r
assert records['candidate']['binary_sha256']==records['control']['binary_sha256']
assert records['candidate']['partial_certificate']==records['control']['partial_certificate']
expected={'decisions.tsv','prefix.tsv','boundaries.tsv','macro.tsv'}
for arm,r in records.items():
    suffixes={name.split('.',1)[1] for name in r['trace_sha256']}
    assert suffixes==(expected|({'memory.tsv'} if arm=='candidate' else set()))
for name in expected:assert records['candidate']['trace_sha256']['candidate.'+name]==records['control']['trace_sha256']['control.'+name]
rows=[dict(row=i+1,**support['parse'](line)) for i,line in enumerate((c/'traces/candidate.memory.tsv').open())]
assert rows
peaks={key:max(rows,key=lambda row:row[key]) for key in support['keys']}
result=dict(exact_paired_work_and_proof=True,parser_fixtures=11,rows=len(rows),peaks=peaks,
    evidence={arm:dict(record_sha256=sha(c/'results'/(arm+'.json')),traces=r['trace_sha256'],partial_certificate=r['partial_certificate']) for arm,r in records.items()},
    limitation=m['limitation'])
p=c/'summary.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print('5702 exact paired replay/proof PASS;rows',len(rows))
print(json.dumps(peaks['current_rss_kib'],indent=2))
