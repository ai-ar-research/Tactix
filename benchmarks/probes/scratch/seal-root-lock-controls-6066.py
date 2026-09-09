#!/usr/bin/env python3
"""Bind exact final old/new fixture and separate corrected trace source."""
import difflib
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6049-reduce-root-lock'
source=root/'.claude/worktrees/runtime-reduce-root-lock-6049'
oldsource=root/'.claude/worktrees/runtime-bve-active-pop-5962'
oldbuild=root/'build/pin-5962-bve-active-pop-release'
out=c/'final-controls';out.mkdir(exist_ok=False)
fixture=source/'tests/test_reduce_root_lock.cpp'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
command=['g++','-std=c++20','-O2','-fno-exceptions','-fno-rtti','-DTACTIX_EXPECT_OLD_REDUCE_ROOT_LOCK',
         '-I'+str(oldsource/'src'),'-I'+str(oldsource/'include'),str(fixture),
         *map(str,sorted((oldbuild/'CMakeFiles/tactix_lib.dir').rglob('*.o'))),'-o',str(out/'old')]
outputs={}
for name,cmd in [('compile',command),('old',[out/'old']),('new',[root/'build/pin-6049-reduce-root-lock-release/tests/test_reduce_root_lock'])]:
    r=subprocess.run(cmd,capture_output=True,text=True)
    (out/(name+'.log')).write_text(r.stdout+r.stderr)
    assert r.returncode==0,(name,r.returncode,r.stdout,r.stderr)
    outputs[name]=r.stdout
assert len(outputs['old'].splitlines())==len(outputs['new'].splitlines())==19
normalized=lambda text:re.sub(r'candidates=\d+ deleted=\d+','candidates=X deleted=X',text)
assert normalized(outputs['old'])==normalized(outputs['new'])
assert outputs['old'].count('candidates=0 deleted=0')==16
assert outputs['new'].count('candidates=1 deleted=1')==16
diagnostic=root/'.claude/worktrees/runtime-reduce-root-lock-trace-6054'
d=root/'benchmarks/runs/20260908-6054-root-lock-trace-controls'
assert '100% tests passed out of 98' in (d/'logs/ctest.log').read_text()
changed=[str(p.relative_to(diagnostic)) for folder in ('src','include','tests') for p in (diagnostic/folder).rglob('*') if p.is_file() and (not (source/p.relative_to(diagnostic)).exists() or sha(p)!=sha(source/p.relative_to(diagnostic)))]
assert sorted(changed)==['src/diagnostics/event_trace.cpp','tests/test_reduce_root_trace.cpp'],changed
patch=''.join(''.join(difflib.unified_diff((source/name).read_text().splitlines(True) if (source/name).exists() else [],
    (diagnostic/name).read_text().splitlines(True),fromfile='a/'+name if (source/name).exists() else '/dev/null',tofile='b/'+name,n=0)) for name in changed)
(d/'observer.patch').write_text(patch)
for name in changed:
    p=d/'source'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(diagnostic/name,p)
result=dict(old_new_cases=18,root_cases=16,trace_suite='98/98 PASS',native_cases=3,
    hashes={str(p.relative_to(root)):sha(p) for p in [fixture,out/'old',*(out.glob('*.log')),d/'observer.patch',*(d/'logs').glob('*.log')]},
    diagnostic_files={name:sha(diagnostic/name) for name in changed},
    diagnostic_binary_sha256=sha(root/'build/pin-6054-reduce-root-lock-trace/tactix'),
    outcome='Exact final fixture confirms old0/new1 root candidate and deletion across16 state/proof/model cases;pending/non-root/stale guards agree. Trace-only clone fixes metadata and passes98 tests includingactual three-way root/pending/non-root annotation control. No runtime or fuzz claim.')
(out/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS: exact final old/new fixture, separate98-test diagnostic source')
