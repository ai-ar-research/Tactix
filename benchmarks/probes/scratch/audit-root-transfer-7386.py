#!/usr/bin/env python3
"""One-shot repeated-storage/full50 handoff; preserve all older source authorities."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-11-root-transfer-audit-7386.json';assert not target.exists()
hashes={};owned=set()
def bind(p,expected=None,own=False):
    d=sha(p);rel=str(p.relative_to(root));assert expected is None or d==expected,rel;hashes[rel]=d
    if own:owned.add(rel)
def elf(p):
    with p.open('rb') as stream:return stream.read(4)==b'\x7fELF'
prior_path=root/'benchmarks/reports/2026-09-11-lucky-lifecycle-audit-7383.json'
prior=json.loads(prior_path.read_text());bind(prior_path)
for rel,digest in prior['hashes'].items():
    if rel!='PROGRESS.md':bind(root/rel,digest)
bind(root/'PROGRESS.md',own=True)
before=subprocess.check_output(['git','show','HEAD:PROGRESS.md'],text=True,cwd=root)
after=(root/'PROGRESS.md').read_text();marker=' Prior accepted frontier/history below remains frozen'
assert before[before.index(marker):]==after[after.index(marker):]
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
for name in ('assess-corrected-roots-7384.py','prepare-corrected-full50-7385.py',
    'submit-corrected-full50-7385.py','validate-corrected-full50-7385.py','audit-root-transfer-7386.py'):
    bind(Path(__file__).with_name(name),own=True)
assessment=root/'benchmarks/reports/2026-09-11-corrected-root-assessment-7384.json'
a=json.loads(assessment.read_text());assert a['expected_outcomes']==12;bind(assessment,own=True)
for rel,digest in a['hashes'].items():bind(root/rel,digest)
bind(root/'benchmarks/reports/2026-09-11-root-transfer-7386.md',own=True)
campaigns={};jobs=set()
for stem,count in [('7375-lucky-BC',66),('7382-corrected-root-runtime',12),('7385-corrected-root-ranks1-50',50)]:
    c=root/('benchmarks/runs/20260911-'+stem);m=json.loads((c/'manifest.json').read_text());bind(c/'manifest.json',own=True)
    for rel,digest in m.get('dependencies',m.get('evidence_sha256',{})).items():bind(root/rel,digest)
    for rel,digest in m['pin_sha256'].items():bind(c/rel,digest,own=not elf(c/rel))
    for p in [*c.glob('launch*.json'),*(c/'claims').glob('*.json')]:
        bind(p,own=True)
        if p.name.startswith('launch'):jobs.add(str(json.loads(p.read_text())['job_id']))
    states={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text());t=r['task'];assert t==int(p.stem) and 0<=t<count
            s=classify(r,folder=='results');assert s['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,s)
            states[t]=s;bind(p,own=True)
    if stem!='7385-corrected-root-ranks1-50':assert len(states)==count
    campaigns[c.name]=dict(expected=count,published=len(states),checked=sum(s['checked'] for s in states.values()),states=states)
debug=root/'benchmarks/runs/20260911-7342-debug-contracts-debug';debug_state=prior['debug_state']
if (debug/'manifest.json').exists():
    m=json.loads((debug/'manifest.json').read_text());assert m['status']=='PASS' and m['tests']==123
    for rel,digest in m['hashes'].items():bind(root/rel,digest)
    bind(debug/'manifest.json',own=True);bind(debug/'logs/ctest.json',own=True);debug_state='PASS123'
andrew=root/'report-to-andrew/20260911-1217-root-transfer'
for name in ('report.tex','report.pdf','report.log'):bind(andrew/name)
assert (andrew/'report.pdf').read_bytes().startswith(b'%PDF-') and 'Overfull' not in (andrew/'report.log').read_text()
commands=[[sys.executable,'-m','unittest','discover','-s',p,'-p','test_*.py'] for p in ('tools/bench','tools/slurm')]
commands += [[sys.executable,'tools/docs/'+name+'.py'] for name in ('check-doc-size','check-progress','check-section-refs',
    'check-rule-policy-boundary','check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name] for name in ('validate-clause-full50-7312.py',
    'check-submit-lucky-BC-7375.py','check-submit-corrected-runtime-7382.py','assess-corrected-roots-7384.py','validate-corrected-full50-7385.py')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name,'--self-test'] for name in ('assess-corrected-roots-7384.py','validate-corrected-full50-7385.py')]
checks={}
for cmd in commands:
    r=subprocess.run(cmd,cwd=root,text=True,capture_output=True);assert r.returncode==0,(cmd,r.stdout,r.stderr)
    checks[' '.join(cmd)]=dict(exit_code=0,stdout=r.stdout,stderr=r.stderr);print('PASS',' '.join(cmd),flush=True)
scheduler={}
for label,cmd in [('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),'-S','2026-09-11T11:31:00','-o','JobID,State%30,ExitCode,Start','-P'])]:
    r=subprocess.run(cmd,text=True,capture_output=True,timeout=30);assert r.returncode==0,r.stderr
    scheduler[label]=dict(command=cmd,stdout=r.stdout,stderr=r.stderr)
with target.open('x') as out:json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
    owned_paths=sorted(owned),prior_authority=str(prior_path.relative_to(root)),campaigns=campaigns,checks=checks,scheduler=scheduler,
    debug_state=debug_state,new_manual_removed_bytes=0,
    decision='Three complete storage pairs on each Rank25/31 justify a full50 transfer investigation. Preserve Rank25 gains and Rank31 time/RSS tradeoffs. Source remains immutable7373 and production unchanged; no new solver or rule edits.',
    hygiene='No old script/pin/source changed. No growing logs hashed or failed witness removed. Submitted guards prevented the premature final-wave attempt before task6/7 publication; later submission used all8 expected exact solver outcomes. All proofs continue asynchronously; prior audit owns earlier source/failure/cleanup evidence.',
    next='Validate all50 published outcomes before provisional aggregate scoring; retain and investigate changed-work/certificate cases, every memory/runtime regression, all established closures and the fixed native vector. Continue largest portable unit-cost/search-work gaps from the actual new table.',
    limitation='Goal unmet. No current-source fuzz, production promotion, memory parity or final full50 proof acceptance implied. Prior denied access/transfer/push boundary unchanged, no retry or bypass. Andrew local and unsent.'),out,indent=2)
print(target)
