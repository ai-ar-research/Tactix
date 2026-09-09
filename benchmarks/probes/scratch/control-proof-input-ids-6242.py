#!/usr/bin/env python3
"""Prove malformed LRAT comes from missing fixture input IDs on parent too."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6242-input-id-control'
assert not c.exists(); c.mkdir()
source=root/'.claude/worktrees/runtime-learned-sort-6169'; pin=root/'build/pin-6169-learned-sort-release'
fixture=root/'benchmarks/probes/scratch/test-parent-input-id-control-6242.cpp'
command=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))),'-o',str(c/'fixture')]
p=subprocess.run(command,capture_output=True,text=True); (c/'build.log').write_text(p.stdout+p.stderr); assert p.returncode==0
p=subprocess.run([str(c/'fixture'),'1','1',str(c/'input.cnf'),str(c/'bad.proof')],capture_output=True,text=True)
(c/'fixture.log').write_text(p.stdout+p.stderr); assert p.returncode==0
parent=(c/'bad.proof').read_bytes()
bad=root/'benchmarks/runs/20260909-6241-substitute-retry-fixed-proofs/1-1-0.proof'
assert parent==bad.read_bytes(), 'must bind whole malformed proof, not just first line'
assert parent.startswith(b'1 -2 1 0 0 0\n')
p=subprocess.run([str(root/'tools/checkers/bin/lrat-check'),str(c/'input.cnf'),str(c/'bad.proof')],capture_output=True,text=True)
(c/'check.log').write_text(p.stdout+p.stderr); assert 'c NOT VERIFIED' in p.stdout
(c/'summary.json').write_text(json.dumps(dict(parent_candidate_bad_proofs_identical=True,proof_sha256=sha(bad),fixture_sha256=sha(fixture),
    cause='Direct kernel_add_* used default id0 and left next_clause_id0. The frontend assigns original clause IDs; omitted by this fixture. Both actual old/new objects emit byte-identical malformed LRAT. Correct fixture import, not solver.',
    checker_output=p.stdout),indent=2)+'\n')
print('PASS: entire rejected LRAT byte-identical on frozen parent; missing input-ID fixture setup confirmed')
