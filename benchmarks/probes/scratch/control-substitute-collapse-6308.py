#!/usr/bin/env python3
"""Run the current actual fixture against frozen parent objects, no solver benchmark."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6308-substitute-collapse-parent-control'
assert not c.exists();c.mkdir()
source=root/'.claude/worktrees/runtime-vivify-binary-retry-6269'
fixture=root/'.claude/worktrees/runtime-substitute-collapse-6305/tests/test_substitute_collapse.cpp'
objects=sorted((root/'build/pin-6269-vivify-binary-retry-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(c/'fixture')]
with (c/'build.log').open('x') as out:subprocess.run(command,stdout=out,stderr=subprocess.STDOUT,check=True)
p=subprocess.run([str(c/'fixture'),'--expect-old-missing'],cwd=c,text=True,capture_output=True)
(c/'fixture.log').write_text(p.stdout+p.stderr)
assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
(c/'summary.json').write_text(json.dumps(dict(command=command,fixture_sha256=sha(fixture),parent_objects={str(p.relative_to(root)):sha(p) for p in objects},stdout=p.stdout,stderr=p.stderr),indent=2)+'\n')
print(p.stdout)
