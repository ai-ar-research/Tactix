#!/usr/bin/env python3
"""Correct fixture admission, retain failed logs and revalidate unsubmitted pins."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import resource
import subprocess
root = Path(__file__).resolve().parents[3]
c = root/'benchmarks/runs/20260908-6107-substitute-publication'
source = root/'.claude/worktrees/runtime-substitute-publication-6107'
def run(name, cmd, accepted=(0,)):
    log=c/'logs'/f'6111-{name}.log'; assert not log.exists()
    with log.open('w') as stream:
        p=subprocess.run(cmd,stdout=stream,stderr=subprocess.STDOUT,
            preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
    assert p.returncode in accepted,(name,p.returncode,log.read_text()[-6000:])
    print(name,p.returncode,'PASS',flush=True)
parent=root/'.claude/worktrees/runtime-reduce-root-lock-6049'
oldbuild=root/'build/pin-6049-reduce-root-lock-release'
old=c/'old-fixture-6111'
run('old-build',['g++','-std=c++17','-O2','-I'+str(parent/'src'),'-I'+str(parent/'include'),
    str(source/'tests/test_substitute_publication.cpp'),
    *map(str,sorted((oldbuild/'CMakeFiles/tactix_lib.dir').rglob('*.o'))),'-o',str(old)])
run('old-negative',[str(old)],(-6,))
assert 'actual.h->scores.stack == expected.h->scores.stack' in (c/'logs/6111-old-negative.log').read_text()
def build(number,name):
    out=root/f'build/pin-{number}-substitute-publication-{name}'
    run(name+'-build',['cmake','--build',str(out),'-j6'])
    run(name+'-ctest',['ctest','--test-dir',str(out),'--output-on-failure','-j1'])
    return out/'tactix'
with ThreadPoolExecutor(max_workers=2) as pool:
    futures=[pool.submit(build,6107,'release'),pool.submit(build,6108,'bc')]
    binaries=[f.result() for f in futures]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(binaries[0])==sha(binaries[1])
files=['src/policies/default_policy.cpp','tests/test_substitute_publication.cpp']
target=c/'build-summary.json';assert not target.exists()
target.write_text(json.dumps(dict(source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),
    files={r:sha(source/r) for r in files},binaries={str(p.relative_to(root)):sha(p) for p in binaries},
    tests='Eight actual-policy cases;two complete99-testRelease suites;immutable6049 fails heap assertion.',
    corrected_fixture='First fixture selected a default preprocessing follow-up without its prerequisite; explicitly admit isolated test slot. First two search cases already passed. Solver source unchanged after first build.',
    limitation='Isolated/unfuzzed;no runtimegain/promotion.'),indent=2)+'\n')
print('PASS: independent Release pins identical',flush=True)
