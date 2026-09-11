#!/usr/bin/env python3
"""Freeze corrected lifecycle sources, negative controls and asynchronous runtime handoff."""
import datetime as dt
import difflib
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-11-lucky-lifecycle-audit-7383.json'
pack=root/'benchmarks/runs/20260911-7383-lucky-lifecycle-checkpoint'
assert not target.exists() and not pack.exists(),'successful one-shot writers are immutable'
hashes={};owned=set()
def bind(p,expected=None,own=False):
    digest=sha(p);rel=str(p.relative_to(root));assert expected is None or digest==expected,rel
    hashes[rel]=digest
    if own:owned.add(rel)
def elf(p):
    with p.open('rb') as stream:return stream.read(4)==b'\x7fELF'
changed={'PROGRESS.md','docs/implementation-notes.md'}
for name in ('root-storage-audit-7355','root-queue-audit-7360','root-queue-finalstatus-7361'):
    p=root/f'benchmarks/reports/2026-09-11-{name}.json';old=json.loads(p.read_text());bind(p)
    for rel,digest in old['hashes'].items():
        if rel not in changed:bind(root/rel,digest)
for rel in changed:bind(root/rel,own=True)
before=subprocess.check_output(['git','show','HEAD:PROGRESS.md'],text=True,cwd=root)
after=(root/'PROGRESS.md').read_text();marker=' Prior accepted frontier/history below remains frozen'
assert before[before.index(marker):]==after[after.index(marker):]
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
helpers=[p for p in Path(__file__).parent.iterdir() if p.is_file() and p.suffix in ('.py','.cpp')
         and any(p.stem.endswith('-'+str(i)) for i in range(7362,7384))]
assert len(helpers)>=27
for p in helpers:bind(p,own=True)
for name in ('2026-09-11-root-lifecycle-analysis-7367.json','2026-09-11-lucky-BC-classification-7381.json'):
    p=root/'benchmarks/reports'/name;data=json.loads(p.read_text());bind(p,own=True)
    for rel,digest in data['hashes'].items():bind(root/rel,digest)
bind(root/'benchmarks/reports/2026-09-11-lucky-lifecycle-7383.md',own=True)
local_stems=('7364-lucky-root-parent','7365-lucky-roots-prepare','7366-lucky-roots-build',
    '7368-lucky-root-proofs','7369-lucky-roots-validation','7371-lucky-reattach-prepare',
    '7372-reattach-parent','7373-lucky-reattach-build','7374-lucky-proofs',
    '7378-corrected-legacy-prepare','7379-corrected-legacy-build')
for stem in local_stems:
    c=root/('benchmarks/runs/20260911-'+stem);assert c.is_dir()
    for p in sorted(c.rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts:bind(p,own=not elf(p))
    p=c/'manifest.json'
    if not p.exists():
        assert stem in ('7366-lucky-roots-build','7368-lucky-root-proofs');continue
    m=json.loads(p.read_text())
    for rel,digest in m.get('hashes',{}).items():bind(root/rel,digest)
    for rel,digest in m.get('files',{}).items():bind(root/m['source']/rel,digest)
    def binaries(values):
        for rel,digest in values.items():
            if isinstance(digest,dict):binaries(digest)
            else:bind(root/rel,digest)
    binaries(m.get('binaries',{}))
    if 'status' in m:assert m['status'] in ('PASS','PASS_NEGATIVE_CONTROL')
build=json.loads((root/'benchmarks/runs/20260911-7373-lucky-reattach-build/manifest.json').read_text())
assert list(build['binaries']['release'].values())==list(build['binaries']['bc'].values())
source=root/build['source'];pack.mkdir()
for name,base in [('incremental.patch',root/'.claude/worktrees/runtime-debug-contracts-7339'),('cumulative.patch',root)]:
    delta=''
    for rel in sorted(build['files']):
        a,b=base/rel,source/rel
        if a.exists() and sha(a)==sha(b):continue
        delta+=''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True),fromfile='a/'+rel if a.exists() else '/dev/null',tofile='b/'+rel,n=0))
    with (pack/name).open('x') as out:out.write(delta)
    bind(pack/name,own=True)
jobs=set();campaigns={}
for stem,count in [('7362-root-lifecycle',2),('7375-lucky-BC',66),('7380-corrected-root-pair',2),('7382-corrected-root-runtime',12)]:
    c=root/('benchmarks/runs/20260911-'+stem);m=json.loads((c/'manifest.json').read_text());bind(c/'manifest.json',own=True)
    for rel,digest in m['dependencies'].items():bind(root/rel,digest)
    for rel,digest in m['pin_sha256'].items():bind(c/rel,digest,own=not elf(c/rel))
    for p in [*c.glob('launch*.json'),*(c/'claims').glob('*.json')]:
        bind(p,own=True)
        if p.name.startswith('launch'):jobs.add(str(json.loads(p.read_text())['job_id']))
    states={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text());t=r['task'];assert t==int(p.stem) and 0<=t<count
            state=classify(r,folder=='results');assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            states[t]=state;bind(p,own=True)
            if folder=='results':
                for artifact in r.get('trace_artifacts',{}).values():bind(c/artifact['path'],artifact['sha256'])
    if stem in ('7362-root-lifecycle','7380-corrected-root-pair'):assert len(states)==count and all(s['checked'] for s in states.values())
    campaigns[c.name]=dict(expected=count,published=len(states),checked=sum(s['checked'] for s in states.values()),states=states)
debug=root/'benchmarks/runs/20260911-7342-debug-contracts-debug'
for name in ('started.json','logs/configure.json','logs/build.json'):bind(debug/name)
debug_state='pending final demo after122 prior passes; full Debug not accepted'
if (debug/'manifest.json').exists():
    m=json.loads((debug/'manifest.json').read_text());assert m['status']=='PASS' and m['tests']==123
    for rel,digest in m['hashes'].items():bind(root/rel,digest)
    bind(debug/'manifest.json',own=True);bind(debug/'logs/ctest.json',own=True);debug_state='PASS123'
proofs=root/'benchmarks/runs/20260911-7374-lucky-proofs';pm=json.loads((proofs/'manifest.json').read_text())
assert pm['checked_UNSAT']==72 and pm['checked_SAT']==72 and pm['negative_controls']==2 and pm['removed_bytes']==120966
assert not any(proofs.rglob('*.cnf')) and not any(proofs.rglob('*.proof')) and not any(proofs.rglob('*.out'))
andrew=root/'report-to-andrew/20260911-1153-lucky-lifecycle'
for name in ('report.tex','report.pdf','report.log'):bind(andrew/name)
assert (andrew/'report.pdf').read_bytes().startswith(b'%PDF-') and 'Overfull' not in (andrew/'report.log').read_text()
commands=[[sys.executable,'-m','unittest','discover','-s',p,'-p','test_*.py'] for p in ('tools/bench','tools/slurm')]
commands += [[sys.executable,'tools/docs/'+name+'.py'] for name in ('check-doc-size','check-progress','check-section-refs',
    'check-rule-policy-boundary','check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name] for name in ('validate-clause-full50-7312.py',
    'check-submit-root-lifecycle-7362.py','check-submit-lucky-BC-7375.py','check-submit-corrected-pair-7380.py','check-submit-corrected-runtime-7382.py')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name,'--self-test'] for name in
    ('analyze-root-lifecycle-7367.py','classify-lucky-BC-7381.py','check-submit-corrected-runtime-7382.py')]
checks={}
for cmd in commands:
    r=subprocess.run(cmd,cwd=root,text=True,capture_output=True);assert r.returncode==0,(cmd,r.stdout,r.stderr)
    checks[' '.join(cmd)]=dict(exit_code=0,stdout=r.stdout,stderr=r.stderr);print('PASS',' '.join(cmd),flush=True)
scheduler={}
for label,cmd in [('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),'-S','2026-09-11T11:08:00','-o','JobID,State%30,ExitCode,Start','-P'])]:
    r=subprocess.run(cmd,text=True,capture_output=True,timeout=30);assert r.returncode==0,r.stderr
    scheduler[label]=dict(command=cmd,stdout=r.stdout,stderr=r.stderr)
with target.open('x') as out:json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    hashes=hashes,owned_paths=sorted(owned),source=build['source'],source_binaries=build['binaries'],
    campaigns=campaigns,scheduler=scheduler,checks=checks,debug_state=debug_state,
    checked_generated_bytes_removed=120966,patch_application='git apply --unidiff-zero',
    decision='Keep isolated Lucky root-consumer and guidance-reattachment repairs. Corrected short legacy/folded arms have identical non-storage work/certificates; old fast stale-queue trajectory is not a storage unit-cost baseline. Measure Rank25/31 next; no production promotion or small-gain rejection.',
    failures='7363 strict macro replay failed:7367 classifies only observer-added begin/retier/checkpoint rows, preserving originals.7366 LSAN ptrace failure retained;7369 rerun outside sandbox passes.7368 stopped/resumed actual solve exposed inherited compact-order duplication, fixed and independently validated7372/7374. Failed inputs and logs preserved.',
    hygiene='All new campaign pins and raw runner scripts are immutable. No growing logs hashed.7374 generated checked fixtures removed;7368 partial cleanup is not assigned an invented receipt. Prior audit owns older active/campaign mapping. Obsolete sandbox watcher56264 stopped only after1681474 terminal accounting and both checked proofs; no solver canceled.',
    limitation='Goal unmet; no full50 for corrected source yet. Current-source fuzz/consolidation and synchronization remain open under prior denied access/transfer/push boundary, no retry or bypass. Andrew report local and unsent.'),out,indent=2)
print(target)
