#!/usr/bin/env python3
"""Snapshot normal campaigns 5352--5359; optionally stage explicit small evidence."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
names=['5352-queue-stamps','5353-factor-stamps','5354-empty-factor','5355-empty-factor-prefix','5356-empty-factor-runtime','5357-empty-factor-BC-gate','5358-next-choice','5359-focused-choice']
campaigns=[root/'benchmarks/runs'/('20260907-'+n) for n in names]
last=campaigns[-1];dest=last/'artifact-audit.json'
staged=[root/p for p in ['PROGRESS.md','docs/implementation-notes.md','docs/scheduling-parity-ledger.md','benchmarks/reports/2026-09-07-empty-factor-housekeeping.md']]
audit=dict(manifests={},local_only=[],certificate_residue=[],manual_deletions=0,ownership='Only eight explicitly named normal campaigns inspected; no fuzz artifacts inspected.',preservation='All campaigns have unique causal or gate evidence. Active 5357 final-checker outputs stay local; no manual deletion warranted.')
for c in campaigns:
    m=json.loads((c/'manifest.json').read_text())
    for rel,sha in m.get('hashes',m.get('pin_sha256',{})).items():assert hashlib.sha256((c/rel).read_bytes()).hexdigest()==sha,(c,rel)
    audit['manifests'][c.name]=hashlib.sha256((c/'manifest.json').read_bytes()).hexdigest()
    for p in c.rglob('*'):
        if not p.is_file() or p==dest:continue
        rel=str(p.relative_to(root));size=p.stat().st_size
        if p.suffix in ('.drat','.lrat','.lpr','.dpr','.cnf'):audit['certificate_residue'].append(rel)
        active_log=c==campaigns[5] and p.parent.name=='logs' and p.name in ('1668740_64.log','1668740_65.log')
        if p.suffix in ('.json','.py','.patch','.sbatch','.log','.out','.tsv') and size<2_000_000 and not active_log:
            staged.append(p)
        else:audit['local_only'].append(dict(path=rel,bytes=size,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),mutable_active_log=active_log))
assert not audit['certificate_residue']
for name,args in [('compare-factor-stamp-checkpoint-5355.py',['--seal']),('compare-focused-choice-5359.py',[]),('seal-empty-factor-gates-5357.py',[])]:
    r=subprocess.run([sys.executable,str(root/'benchmarks/probes/scratch'/name),*args],capture_output=True,text=True,check=True)
    audit[name]=r.stdout
for job,c in zip((1668720,1668722,1668724,1668726,1668806,1668808),[campaigns[i] for i in (0,1,2,3,6,7)]):
    for i,arm in enumerate(('tactix','kissat')):
        log=(c/'logs'/f'{job}_{i}.log').read_text()
        assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
audit['stopped_clean']=12
audit['full_checked_records']={c.name:len(list((c/'results').glob('*.json'))) for c in campaigns[4:6]}
if not dest.exists():
    q=subprocess.run(['squeue','-u','hwu','-h','-o','%i|%j|%T|%M'],capture_output=True,text=True,check=True)
    audit['queue_snapshot']=q.stdout
    audit['documentation']={}
    for name in ('check-doc-size','check-progress'):
        r=subprocess.run([sys.executable,'tools/docs/'+name+'.py'],cwd=root,capture_output=True,text=True)
        assert r.returncode==0,(name,r.stdout,r.stderr)
        log=last/'logs'/(name+'.log');log.write_text(r.stdout+r.stderr);staged.append(log)
        audit['documentation'][name]=dict(exit=0,output_sha256=hashlib.sha256(log.read_bytes()).hexdigest())
    dest.write_text(json.dumps(audit,indent=2)+'\n')
staged.append(dest)
for number in range(5352,5360):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(number)+'*'):
        if p.suffix in ('.py','.c','.cpp'):staged.append(p)
staged=sorted(set(staged))
if '--stage' in sys.argv:subprocess.run(['git','add','-f','--',*map(str,staged)],cwd=root,check=True)
print(json.dumps(dict(explicit_staging_files=len(staged),certificate_residue=[],manual_deletions=0,stopped_clean=12,full_checked_records=audit['full_checked_records'])))
