#!/usr/bin/env python3
"""Freeze read-only focused-queue columns at the existing choice frontier."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5358-next-choice'
new=root/'benchmarks/runs/20260907-5359-focused-choice'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):(new/name).mkdir()
for p in old.glob('*.patch'):shutil.copy2(p,new/p.name)
for arm,src in [('tactix','build/pin-5359-focused-choice/tactix'),('kissat','tools/solvers/variants/kissat-focused-choice-5359/build/kissat')]:
    shutil.copy2(root/src,new/'pins'/arm)
for arm,before,after,file in [
    ('tactix','.claude/worktrees/runtime-empty-factor-5354','.claude/worktrees/runtime-focused-choice-5359','src/diagnostics/event_trace.cpp'),
    ('kissat','tools/solvers/variants/kissat-factor-stamps-5353','tools/solvers/variants/kissat-focused-choice-5359','src/eventtrace.c')]:
    r=subprocess.run(['diff','-u',str(root/before/file),str(root/after/file)],capture_output=True)
    assert r.returncode==1
    (new/(arm+'-focused-choice.patch')).write_bytes(r.stdout)
(new/'run.py').write_bytes((old/'run.py').read_bytes())
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
m=json.loads((old/'manifest.json').read_text());m.pop('job',None)
m.update(purpose='Read-only focused queue state after isolated empty-Factor repair; UNFUZZED.',
    hypothesis='Focused VMTF queue/cursor state differs before selection 327447 although the observed EVSIDS state matches.',
    acceptance='Zero stops and clean scratch; decisions/prefix bytes and first ten selected-state columns identical to 5358. Inspect appended focused queue columns.',
    parent_campaign=str(old.relative_to(root)),source_note='Exact 5354/5353 source plus appended read-only queue state; fresh observer builds. Legacy release_sha256 is ancestral, not current pin identity.',
    state_columns='conflicts decisions level chosen observed value assignment_level heap_position score reason stable queue_stamp search_external search_stamp observed_stamp observed_prev observed_next chosen_stamp'.split())
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
