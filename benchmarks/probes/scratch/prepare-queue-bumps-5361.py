#!/usr/bin/env python3
"""Freeze symmetric, read-only queue bump observations in the last common window."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5360-queue-frontier'
new=root/'benchmarks/runs/20260907-5361-queue-bumps'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for arm,src in [('tactix','build/pin-5361-queue-bumps/tactix'),('kissat','tools/solvers/variants/kissat-queue-bumps-5361/build/kissat')]:shutil.copy2(root/src,new/'pins'/arm)
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
for arm,before,after,files in [
    ('tactix','.claude/worktrees/runtime-focused-choice-5359','.claude/worktrees/runtime-queue-bumps-5361',['src/policies/heuristics.cpp']),
    ('kissat','tools/solvers/variants/kissat-focused-choice-5359','tools/solvers/variants/kissat-queue-bumps-5361',['src/inlinequeue.h','src/eventtrace.h','src/eventtrace.c'])]:
    patch=b''
    for file in files:
        r=subprocess.run(['diff','-u','--label','a/'+file,'--label','b/'+file,str(root/before/file),str(root/after/file)],capture_output=True)
        assert r.returncode==1;patch+=r.stdout
    (new/(arm+'-queue-bumps.patch')).write_bytes(patch)
source=(old/'run.py').read_text()
anchor='env[prefix + "_CHOICE_STATE_TRACE"]'
source=source.replace(anchor,'env[prefix + "_QUEUE_BUMP_TRACE"] = str(C/"traces"/(arm+".bumps.tsv"))\nenv["SAT_QUEUE_BUMP_FROM"] = "132816"\nenv["SAT_QUEUE_BUMP_TO"] = "133243"\n'+anchor)
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Read-only active queue bump sequence, conflicts 132816--133243; no semantic edit.',hypothesis='Analyzed-variable membership/order or queue transition first differs after the last common Factor boundary.',acceptance='Zero stops and clean scratch. Existing decision, prefix, state and queue boundary bytes match 5360. Compare first active bump identity/order/value, then pre-bump queue fields.',parent_campaign=str(old.relative_to(root)),source_note='Fresh 5359 source copies plus scalar read-only hooks before active queue move; legacy release_sha256 is ancestral.',bump_columns='conflicts decisions stable external stamp prev next search search_stamp queue_stamp value'.split())
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
