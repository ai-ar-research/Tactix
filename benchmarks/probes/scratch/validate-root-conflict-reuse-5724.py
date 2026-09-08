#!/usr/bin/env python3
"""Actual native/predecessor parity plus independently checked fixed micro-proofs."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-5718-root-conflict-reuse'
d=c/'fixed-validation';d.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
commands=[]
def run(label,cmd):
    commands.append([str(x) for x in cmd])
    r=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
    (d/(label+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0,(label,r.returncode,r.stdout,r.stderr)
    return r.stdout
old=root/'build/pin-5672-assignment-level-release'
source=root/'.claude/worktrees/runtime-root-conflict-reuse-5718'
fixture=source/'tests/test_root_conflict_reuse.cpp'
parent=root/'.claude/worktrees/runtime-assignment-level-5672'
link=shlex.split((old/'tests/CMakeFiles/test_assignment_level_bound.dir/link.txt').read_text())
objects=[(old/'tests'/name).resolve() for name in link if name.endswith('.o')][1:]
assert objects and all(p.is_file() for p in objects)
run('compile-previous',[link[0],'-O3','-DNDEBUG','-std=c++20','-fno-exceptions','-fno-rtti',
    '-I'+str(parent/'src'),'-I'+str(parent/'include'),str(fixture),*objects,'-o',d/'previous'])
previous=run('previous',[d/'previous','--legacy'])
native=root/'tools/solvers/variants/kissat-score-bump-observer-5696'
native_fixture=root/'benchmarks/probes/scratch/reference-root-conflict-reuse-5722.c'
run('compile-native',['gcc','-O3','-DNDEBUG','-I'+str(native/'src'),native_fixture,
    native/'build/libkissat.a','-lm','-o',d/'native'])
reference=run('native',[d/'native'])
proofs=d/'proofs';proofs.mkdir()
b=root/'build/pin-5718-root-conflict-reuse-release'
current=run('current',[b/'tests/test_root_conflict_reuse','--current',proofs])
rows=lambda text:[line.split() for line in text.splitlines() if line.startswith('PARITY ')]
assert len(rows(current))==len(rows(previous))==len(rows(reference))==24
assert rows(current)==rows(reference)
assert all(a[:5]==b[:5] and a!=b for a,b in zip(rows(current),rows(previous)))
assert 'PASS: 288 root-conflict fixtures' in previous and 'PASS: 288 root-conflict fixtures' in current
records=[]
for cnf in sorted(proofs.glob('*.cnf')):
    proof=cnf.with_suffix('.proof')
    kind='lrat' if cnf.stem.endswith('-lrat') else 'drat'
    checker=root/('tools/checkers/bin/lrat-check' if kind=='lrat' else 'tools/checkers/bin/drat-trim')
    r=subprocess.run([checker,cnf,proof],cwd=root,capture_output=True,text=True)
    log=r.stdout+r.stderr
    (d/(cnf.stem+'-check.log')).write_text(log)
    assert ('c VERIFIED' in log and r.returncode==0) if kind=='lrat' else 's VERIFIED' in log
    records.append(dict(case=cnf.stem,kind=kind,cnf_sha256=sha(cnf),proof_sha256=sha(proof),
        checker_sha256=sha(checker),exit_code=r.returncode,log_sha256=sha(d/(cnf.stem+'-check.log'))))
    proof.unlink();cnf.unlink()
assert len(records)==288 and not list(proofs.iterdir())
paths=[fixture,native_fixture,native/'src/analyze.c',native/'src/inlineassign.h',
    native/'src/learn.c',native/'build/libkissat.a',*objects,d/'previous',d/'native',b/'tests/test_root_conflict_reuse']
(d/'summary.json').write_text(json.dumps(dict(native_cases=24,all_native_states_equal=True,
    predecessor_cases=288,all24_selected_previous_states_differ=True,
    micro_proofs=records,commands=commands,hashes={str(p.relative_to(root)):sha(p) for p in paths},
    limitation='Fixed actual-code transition checks,not a benchmark or full current-source fuzz gate. All generated proof/CNF inputs deleted only after independent verification; logs and content hashes retained.'),indent=2)+'\n')
print('PASS:24 exact native transitions;288 predecessor controls;288 checked DRAT/LRAT micro-proofs',flush=True)
