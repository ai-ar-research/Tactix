#!/usr/bin/env python3
"""Independent fixed edge checks against the unchanged submitted candidate."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/runs/20260908-5813-forward-unit-edges'
out.mkdir(exist_ok=False)
build = root / 'build/pin-5798-forward-unit-release'
source = root / '.claude/worktrees/runtime-forward-unit-5798'
fixture = root / 'benchmarks/probes/scratch/forward-unit-edges-5812.cpp'
parent_fixture = source / 'tests/test_forward_unit.cpp'
original = parent_fixture.read_text()
assert original.count('static void independent_tail(') == 1
header = out / 'forward-fixture.hpp'
header.write_text(original.split('static void independent_tail(')[0])
link = shlex.split((build / 'tests/CMakeFiles/test_forward_unit.dir/link.txt').read_text())
objects = [(build / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
commands = []
def run(name,command):
    commands.append([str(x) for x in command])
    r = subprocess.run(command,cwd=root,capture_output=True,text=True)
    (out / (name + '.log')).write_text(r.stdout+r.stderr)
    assert r.returncode == 0,(name,r.returncode,r.stdout,r.stderr)
    return r.stdout+r.stderr
run('compile',[link[0],'-O3','-DNDEBUG','-std=c++20','-fno-exceptions','-fno-rtti',
    '-I'+str(source / 'src'),'-I'+str(source / 'include'),'-I'+str(out),fixture,
    *objects,'-o',out / 'edges'])
run('fixtures',[out / 'edges',out])
checks = []
for cnf in sorted(out.glob('*.cnf')):
    proof = cnf.with_suffix('.proof')
    lrat = cnf.stem.endswith('-lrat')
    checker = root / 'tools/checkers/bin' / ('lrat-check' if lrat else 'drat-trim')
    log = run(cnf.stem+'-check',[checker,cnf,proof])
    assert ('c VERIFIED' if lrat else 's VERIFIED') in log
    checks.append(dict(case=cnf.stem,cnf_sha256=sha(cnf),proof_sha256=sha(proof),checker_sha256=sha(checker)))
assert len(checks) == 12
(out / 'summary.json').write_text(json.dumps(dict(state_model_cases=4,checked_conflict_proofs=checks,
    commands=commands,hashes={str(p.relative_to(root)):sha(p) for p in [fixture,parent_fixture,header,*objects,out / 'edges']},
    limitation='Fixed supplementary regressions against5798,not fuzz or a modified submitted source. Each conflict assertion identifies direct C2 root refutation before any final search.'),indent=2)+'\n')
print('PASS:4 fixed states,12 independently checked direct C2 conflict proofs')
