#!/usr/bin/env python3
"""Land the measured outcome and correctly scoped repeated-pop evidence locally."""
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-bve-runtime-landing.json'
assert not target.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for b in iter(lambda:stream.read(8<<20),b''):h.update(b)
    return h.hexdigest()
report=root/'report-to-andrew/20260908-1800-bve-runtime'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
c=root/'benchmarks/runs/20260908-5988-bve-repeat-heap'
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(c/rel)==digest
for rel,digest in m['native_source_files'].items():assert sha(root/m['native_source']/rel)==digest
v=json.loads((c/'summary.json').read_text())
assert v['full_replay'] and v['target_ordinal']==2
assert v['subject_positions']=={'candidate':8930,'kissat':8954}
assert v['snapshots'][2]['differences']==dict(pos=32,score=0,positive=0,negative=0,value=0,retired=0,retry=0)
runtime=root/'benchmarks/runs/20260908-5970-bve-active-pop-ranks1-40'
v=json.loads((runtime/'parent-work-validation.json').read_text())
assert v['all40_certificate_identity'] and v['all40_other_nontiming_statistics_identity']
checks={}
commands=[(name,['python3',f'tools/docs/{name}.py']) for name in
          ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary','check-proof-rule-sync','check-inventory')]
commands += [('benchmark-tests',['python3','-m','unittest','discover','-s','tools/bench','-p','test_*.py'])]
for name in ('validate-bve-repeat-heap-5987.py','compare-active-pop-full-work-5990.py','compare-bve-subtree-5993.py'):
    commands.append((name,['python3','benchmarks/probes/scratch/'+name,'--self-test']))
for name,cmd in commands:
    r=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
    assert r.returncode==0,(name,r.stdout,r.stderr)
    checks[name]=r.stdout+r.stderr
jobs='1673402,1675162,1675706,1675801,1675996'
queue=subprocess.run(['squeue','-h','-j',jobs,'-o','%i|%P|%T|%C|%m|%R'],capture_output=True,text=True,check=True).stdout
terminal=subprocess.run(['sacct','-n','-X','-j','1675845,1675945','-o','JobID,State,ExitCode','-P'],capture_output=True,text=True,check=True).stdout
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md','benchmarks/reports/2026-09-08-bve-runtime-and-repeated-pop.md',
            'benchmarks/reports/2026-09-08-repeated-pop-validated.md','benchmarks/reports/2026-09-08-bve-runtime-validation.json',
            'benchmarks/runs/20260908-5978-bve-earlier-heap/first-occurrence-validation.json'):
    retain(root/rel)
for number in range(5984,5996):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(number)+'.*'):retain(p)
for number in (5970,5985,5988,5992):
    matches=list((root/'benchmarks/runs').glob(f'20260908-{number}-*'));assert len(matches)==1
    campaign=matches[0]
    for p in campaign.rglob('*'):
        if not p.is_file() or p.stat().st_size>=5<<20:continue
        parts=p.relative_to(campaign).parts
        if any(name in parts for name in ('claims','__pycache__','traces')):continue
        if 'logs' in parts and number!=5985:continue
        if p.suffix in ('.json','.py','.sbatch','.tsv','.c','.cpp','.patch','.txt','.log'):retain(p)
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},checks=checks,
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    queue=queue,terminal=terminal,
    outcome='5962 full40 at1.116453x fixedKissat,all40 parentcertificates/otherstats exactexcept intendedcounters: no gain. Repeatedobserver5988 validates32-position permutation atsecondpop462198;5992 investigates earlier mutation.',
    next_action='Validate5992 via5993 and align repeated triggers by actualstate,then implement the source-backed cause on5962. No score/tie change without evidence.',
    cleanup='No manual deletion; audited terminal normal records confirm owned cleanup. All active pins and uncertain/large trace evidence retained. No fuzz-owner artifact access.',
    limitation='Current-source fuzz remains denied,pending;no retry,push,promotion orgoal closure. Report local,notsent. Proofchecking asynchronous.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Measure inactive-pop runtime and validate repeated heap occurrences'],cwd=root,check=True)
print('PASS:',len(owned),'explicit paths landed locally;no push')
