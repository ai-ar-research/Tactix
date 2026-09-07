#!/usr/bin/env python3
"""Post-choice selected-variable state at the first remaining decision split."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5347-decision-stream'
new=root/'benchmarks/runs/20260907-5348-choice-state'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):
    (new/name).mkdir()
for arm,binary in (
    ('tactix',root/'build/pin-5348-choice-state/tactix'),
    ('kissat',root/'tools/solvers/variants/kissat-choice-state-5348/build/kissat')):
    shutil.copy2(binary,new/'pins'/arm)
for p in old.glob('*.patch'):
    shutil.copy2(p,new/p.name)
source=(old/'run.py').read_text().replace('"200000"','"95801"')
anchor='env.update({prefix + "_DECISION_STREAM_TRACE"'
assert source.count(anchor)==1
source=source.replace(anchor,'''env.update({prefix + "_CHOICE_STATE_TRACE": str(C/"traces"/(arm+".state.tsv")),
            "SAT_CHOICE_STATE_CONFLICT": "95801", "SAT_CHOICE_STATE_VARIABLE": "5416"})
env.update({prefix + "_DECISION_STREAM_TRACE"''')
source=source.replace("'SAT_DECISION_'","'SAT_DECISION_', 'SAT_CHOICE_'")
(new/'run.py').write_text(source)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)))
for arm,before,after,rel in (
    ('tactix','.claude/worktrees/runtime-decision-stream-5347','.claude/worktrees/runtime-choice-state-5348','src/diagnostics/event_trace.cpp'),
    ('kissat','tools/solvers/variants/kissat-decision-stream-5347','tools/solvers/variants/kissat-choice-state-5348','src/eventtrace.c')):
    r=subprocess.run(['diff','-u','--label','a/'+rel,'--label','b/'+rel,root/before/rel,root/after/rel],text=True,capture_output=True)
    assert r.returncode==1
    (new/(arm+'-choice-state.patch')).write_text(r.stdout)
m=json.loads((old/'manifest.json').read_text())
m.pop('job',None)
m.update(purpose='UNFUZZED selected-variable post-choice state at the known ordinary decision split.',
    hypothesis='5416 is either assigned differently before selection or has different decision-ranking state; distinguish before tracing a producer.',
    acceptance='Both stop zero at prefix endpoint past 95801, clean scratch; every recorded decision equals the corresponding prefix of 5347. State rows occur after the chosen decision assignment, before its propagation.',
    parent_campaign=str(old.relative_to(root)),
    source_note='Exact 5347 sources plus generic selected-variable scalar state observer; fresh immutable trace builds, no semantic edit.')
m['hashes']={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(new)
