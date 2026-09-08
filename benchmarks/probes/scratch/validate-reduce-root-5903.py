#!/usr/bin/env python3
"""Actual old/new/native controls and independently checked Reduce witnesses."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
c = root / 'benchmarks/runs/20260908-5900-reduce-root'
out = c / 'controls'
out.mkdir(exist_ok=False)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(name,cmd):
    p = out / (name + '.log')
    assert not p.exists()
    r = subprocess.run(cmd,cwd=root,capture_output=True,text=True)
    p.write_text(r.stdout+r.stderr)
    assert r.returncode == 0,(cmd,r.returncode,r.stdout,r.stderr)
    print(name,'PASS',flush=True)
    return dict(command=list(map(str,cmd)),exit_code=r.returncode,log_sha256=sha(p))
fixture = root / '.claude/worktrees/runtime-reduce-root-5900/tests/test_reduce_root_guidance.cpp'
commands=[]
hashes={str(fixture.relative_to(root)):sha(fixture)}
oldsource=root / '.claude/worktrees/runtime-forward-caller-5849'
oldbuild=root / 'build/pin-5849-forward-caller-release'
link=shlex.split((oldbuild / 'tests/CMakeFiles/test_forward_unit.dir/link.txt').read_text())
objects=[(oldbuild / 'tests' / p).resolve() for p in link if p.endswith('.o')][1:]
commands.append(run('old-compile',[link[0],'-O3','-DNDEBUG','-std=c++20','-fno-exceptions','-fno-rtti',
    '-DTACTIX_EXPECT_OLD_REDUCE_ROOT','-I'+str(oldsource / 'src'),'-I'+str(oldsource / 'include'),fixture,*objects,'-o',out / 'old']))
commands.append(run('old',[out / 'old']))
new=root / 'build/pin-5900-reduce-root-release/tests/test_reduce_root_guidance'
commands.append(run('new',[new]))
native=root / 'tools/solvers/variants/kissat-score-bump-observer-5696'
nativefixture=here / 'reduce-root-native-5902.c'
commands.append(run('native-compile',['gcc','-O3','-DNDEBUG','-I'+str(native / 'src'),nativefixture,native / 'build/libkissat.a','-lm','-o',out / 'native']))
commands.append(run('native',[out / 'native']))
for p in [*objects,out / 'old',new,nativefixture,native / 'build/libkissat.a',out / 'native']:
    hashes[str(p.relative_to(root))]=sha(p)
(out / 'summary.json').write_text(json.dumps(dict(commands=commands,hashes=hashes,
    outcome='Actual old caller retains newly derived roots in queue/heap;new DefaultPolicy and native remove them. Raw public Reduce remains policy-independent. Truncated suffix,no-op,original SAT model and refutation states pass.'),indent=2)+'\n')
script=here / 'validate-forward-unit-5801.py'
code=script.read_text().replace('20260908-5798-forward-unit/fixed-proofs','20260908-5900-reduce-root/reduce-proofs')
code=code.replace('pin-5798-forward-unit-release/tests/test_forward_unit','pin-5900-reduce-root-release/tests/test_reduce_root_guidance')
code=code.replace('len(records) == 16','len(records) == 6').replace('checked_micro_proofs=16','checked_micro_proofs=6').replace('state_model_cases=17','state_model_cases=3')
code=code.replace('17 state/model cases, 8 DRAT and 8 LRAT independently verified proofs','Reduce root guidance:3 state/model cases,3 DRAT and3 LRAT independently verified proofs')
exec(compile(code,str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
