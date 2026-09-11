#!/usr/bin/env python3
"""One-shot checkpoint: preserve useful repair and diagnosed root regression."""
import datetime as dt
import difflib
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-11-root-storage-audit-7355.json'
pack=root/'benchmarks/runs/20260911-7355-root-storage-checkpoint'
assert not target.exists() and not pack.exists(),'one-shot writer, never rerun'
hashes={};owned=set()
def bind(path,expected=None,own=False):
    digest=sha(path);rel=str(path.relative_to(root))
    assert expected is None or digest==expected,(rel,digest,expected)
    hashes[rel]=digest
    if own:owned.add(rel)
prior_path=root/'benchmarks/reports/2026-09-11-root-fold-audit-7338.json'
prior=json.loads(prior_path.read_text());bind(prior_path)
changed={'PROGRESS.md','docs/implementation-notes.md'}
for rel,digest in prior['hashes'].items():
    if rel not in changed:bind(root/rel,digest)
for rel in changed:bind(root/rel,own=True)
for name in ('2026-09-11-root-storage-regression-7355.md',
             '2026-09-11-root-B14-analysis-7352.json','2026-09-11-root-phase-analysis-7354.json'):
    bind(root/'benchmarks/reports'/name,own=True)
helpers=('prepare-debug-contracts-7339.py','build-debug-contracts-7340.py',
    'check-debug-contracts-parent-7341.py','check-debug-suite-7342.py',
    'check-debug-root-proofs-7343.py','debug-contract-proofs-7343.cpp',
    'check-debug-contract-proofs-7344.py','check-debug-contract-proofs-7345.py',
    'prepare-root-storage-BC-7346.py','check-submit-root-storage-BC-7346.py',
    'prepare-root-storage-BC-7347.py','check-submit-root-storage-BC-7347.py',
    'prepare-root-collector-isolation-7348.py','check-submit-root-collector-isolation-7348.py',
    'prepare-root-decision-trace-7349.py','check-submit-root-decision-trace-7349.py',
    'root-storage-statistics-7350.py','analyze-root-B14-7351.py','analyze-root-B14-7352.py',
    'prepare-root-phase-trace-7353.py','check-submit-root-phase-trace-7353.py',
    'analyze-root-phase-7354.py','audit-root-storage-7355.py')
for name in helpers:bind(Path(__file__).with_name(name),own=True)
build_path=root/'benchmarks/runs/20260911-7340-debug-contracts-build/manifest.json'
build=json.loads(build_path.read_text());assert build['status']=='PASS';bind(build_path,own=True)
source=root/build['source']
for rel,digest in build['files'].items():bind(source/rel,digest)
for group in build['binaries'].values():
    for rel,digest in group.items():bind(root/rel,digest)
assert list(build['binaries']['release'].values())==list(build['binaries']['bc'].values())
for stem in ('7340-debug-contracts-build','7341-debug-contracts-regression',
             '7343-debug-root-proofs','7345-debug-contract-proofs'):
    data=json.loads((root/('benchmarks/runs/20260911-'+stem+'/manifest.json')).read_text())
    assert data['status'] in ('PASS','PASS_NEGATIVE_CONTROLS')
    for rel,digest in data['hashes'].items():bind(root/rel,digest)
new_local=('7339-debug-contracts-prepare','7340-debug-contracts-build','7341-debug-contracts-regression',
    '7343-debug-root-proofs','7344-debug-contract-proofs','7345-debug-contract-proofs')
for stem in new_local:
    for path in sorted((root/('benchmarks/runs/20260911-'+stem)).rglob('*')):
        if not path.is_file() or '__pycache__' in path.parts:continue
        with path.open('rb') as stream:elf=stream.read(4)==b'\x7fELF'
        bind(path,own=not elf)
# The Debug CTest process is still allowed to run. Only immutable start/build
# artifacts, and final records if already atomically published, are bound.
debug=root/'benchmarks/runs/20260911-7342-debug-contracts-debug'
for rel in ('started.json','logs/configure.json','logs/build.json'):bind(debug/rel,own=True)
debug_state='pending_final_demo; 122 earlier tests passed; no full-suite acceptance'
if (debug/'manifest.json').exists():
    db=json.loads((debug/'manifest.json').read_text());assert db['status']=='PASS' and db['tests']==123
    for rel,digest in db['hashes'].items():bind(root/rel,digest)
    bind(debug/'manifest.json',own=True);bind(debug/'logs/ctest.json',own=True)
    debug_state='PASS123'
elif (debug/'logs/ctest.json').exists():
    db=json.loads((debug/'logs/ctest.json').read_text());bind(debug/'logs/ctest.json',own=True)
    assert db['returncode']==0,('full Debug failed',db['stdout'][-4000:])
    debug_state='CTest completed successfully, helper manifest not yet published'
pack.mkdir()
for name,base in [('incremental.patch',root/'.claude/worktrees/runtime-root-watermark-7333'),
                  ('cumulative.patch',root)]:
    delta=''
    for rel in sorted(build['files']):
        old,new=base/rel,source/rel
        if old.exists() and sha(old)==sha(new):continue
        delta+=''.join(difflib.unified_diff(old.read_text().splitlines(True) if old.exists() else [],
            new.read_text().splitlines(True),fromfile='a/'+rel if old.exists() else '/dev/null',tofile='b/'+rel,n=0))
    with (pack/name).open('x') as out:out.write(delta)
    bind(pack/name,own=True)
old_progress=subprocess.check_output(['git','show','HEAD:PROGRESS.md'],cwd=root,text=True)
current=(root/'PROGRESS.md').read_text();marker=' Prior accepted frontier/history below remains frozen'
assert old_progress[old_progress.index(marker):]==current[current.index(marker):]
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
campaign_specs={name:row['expected'] for name,row in prior['campaigns'].items()}
campaign_specs.update({'20260911-7347-root-storage-BC':66,'20260911-7348-root-collector-isolation':3,
    '20260911-7349-root-decision-trace':2,'20260911-7353-root-phase-trace':2})
campaigns={};jobs=set()
for name,count in campaign_specs.items():
    c=root/'benchmarks/runs'/name;states={};new=name.startswith(('20260911-7347','20260911-7348','20260911-7349','20260911-7353'))
    if new:
        manifest=json.loads((c/'manifest.json').read_text());bind(c/'manifest.json',own=True)
        for rel,digest in manifest['dependencies'].items():bind(root/rel,digest)
        for rel,digest in manifest['pin_sha256'].items():
            path=c/rel
            with path.open('rb') as stream:elf=stream.read(4)==b'\x7fELF'
            bind(path,digest,own=not elf)
    for folder in ('provisional','results'):
        for path in sorted((c/folder).glob('*.json')):
            r=json.loads(path.read_text());task=r['task'];assert task==int(path.stem) and 0<=task<count
            state=classify(r,folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(path,state)
            states[task]=state;bind(path,own=True)
            # Final proof-bound trace artifacts only. Earlier completed trace
            # snapshots additionally have independent7352/7354 bindings.
            if new and folder=='results':
                for artifact in r.get('trace_artifacts',{}).values():
                    bind(c/artifact['path'],artifact['sha256'])
    campaigns[name]=dict(expected=count,published=len(states),checked=sum(s['checked'] for s in states.values()),records=states)
    for path in c.iterdir():
        if path.name=='launch.json' or path.name.endswith('-submission.json'):
            jobs.add(str(json.loads(path.read_text())['job_id']))
            if new:bind(path,own=True)
    if new:
        for path in (c/'claims').glob('*.json'):bind(path,own=True)
        # SLURM logs are intentionally not packaged while checkers can append.
scheduler={}
for label,command in [
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),'-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])]:
    result=subprocess.run(command,text=True,capture_output=True,timeout=30);assert result.returncode==0,result.stderr
    scheduler[label]=dict(command=command,stdout=result.stdout,stderr=result.stderr)
andrew=root/'report-to-andrew/20260911-1038-root-storage-regression'
for name in ('report.tex','report.pdf','report.log'):bind(andrew/name)
assert (andrew/'report.pdf').read_bytes().startswith(b'%PDF-') and 'Overfull' not in (andrew/'report.log').read_text()
commands=[[sys.executable,'-m','unittest','discover','-s',path,'-p','test_*.py'] for path in ('tools/bench','tools/slurm')]
commands += [[sys.executable,'tools/docs/'+name+'.py'] for name in ('check-doc-size','check-progress','check-section-refs',
    'check-rule-policy-boundary','check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name] for name in ('validate-clause-full50-7312.py',
    'check-submit-root-storage-BC-7347.py','check-submit-root-collector-isolation-7348.py',
    'check-submit-root-decision-trace-7349.py','check-submit-root-phase-trace-7353.py','root-storage-statistics-7350.py')]
commands += [[sys.executable,'benchmarks/probes/scratch/analyze-root-phase-7354.py','--self-test']]
checks={}
for command in commands:
    result=subprocess.run(command,cwd=root,text=True,capture_output=True);assert result.returncode==0,(command,result.stdout,result.stderr)
    checks[' '.join(command)]=dict(exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr)
    print('PASS',' '.join(command),flush=True)
root_proofs=json.loads((root/'benchmarks/runs/20260911-7343-debug-root-proofs/manifest.json').read_text())
contract_proofs=json.loads((root/'benchmarks/runs/20260911-7345-debug-contract-proofs/manifest.json').read_text())
for stem in ('7343-debug-root-proofs','7345-debug-contract-proofs'):
    c=root/('benchmarks/runs/20260911-'+stem)
    assert not any(c.rglob('*.cnf')) and not any(c.rglob('*.proof')) and not any(c.rglob('*.out'))
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,owned_paths=sorted(owned),
        campaigns=campaigns,scheduler=scheduler,checks=checks,source=build['source'],source_binaries=build['binaries'],
        debug_state=debug_state,checked_generated_bytes_removed=root_proofs['removed_bytes']+contract_proofs['removed_bytes'],
        patch_application='git apply --unidiff-zero',
        preserved_failures='7344 unreachable collector input;7346 expected text instead of structured log path;7351 normal/warmup counter assumption. No failed solver/proof oracle waived.',
        decision='Retain isolated collector correction and tests; root-storage changed-work regression localized to BVE/22861. Not a speedup, acceptance, source promotion or goal completion.',
        next='Selected BVE heap/ordering/retry state for22861 atC26295 with whole replay. Independent checks and full Debug continue; no new long timing waves until premise resolved.',
        limitation='Latest-source fuzz/consolidation/full50/synchronization remain open. No denied access/transfer/push retry. Old campaigns and all prior writer outputs remain frozen.'),out,indent=2)
print(target)
