#!/usr/bin/env python3
"""Link the fixed replay driver to frozen actual Tactix objects;test tie cases."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6100-rank37-substitute-deactivation/replay-controls'
assert not c.exists();c.mkdir()
source=root/'.claude/worktrees/runtime-macro-heap-6090'
build=root/'build/pin-6090-macro-heap-trace'
link=build/'tests/CMakeFiles/test_event_trace.dir/link.txt'
tokens=shlex.split(link.read_text())
objects=[(build/'tests'/p).resolve() for p in tokens if p.endswith('.o') and 'test_event_trace.cpp.o' not in p]
assert objects and all(p.exists() for p in objects)
driver=Path(__file__).with_name('replay-heap-removals-6103.cpp')
cmd=[tokens[0],'-std=c++17','-O2','-DTACTIX_ENABLE_EVENT_TRACE=1','-I'+str(source/'src'),'-I'+str(source/'include'),str(driver),*map(str,objects),'-o',str(c/'replay')]
p=subprocess.run(cmd,capture_output=True,text=True);(c/'build.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
header='# tactix-heap-set\t1\n# external\tposition\tscore\tassigned\tactive\n'
def heap(name,variables):
    path=c/name;path.write_text(header+''.join(f'{v}\t{i}\t1\t0\t1\n' for i,v in enumerate(variables)));return path
incoming=heap('small-incoming.tsv',range(1,9))
(c/'small-left.txt').write_text('2\n3\n');(c/'small-right.txt').write_text('3\n2\n')
left=heap('small-left.tsv',[1,8,7,4,5,6]);right=heap('small-right.tsv',[1,7,8,4,5,6])
args=[str(c/'replay'),str(incoming),str(c/'small-left.txt'),str(c/'small-right.txt'),str(left),str(right)]
p=subprocess.run(args,capture_output=True,text=True);(c/'small-replay.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
bad=subprocess.run(args[:-1]+[str(left)],capture_output=True,text=True)
assert bad.returncode==1 and 'differing final positions' in bad.stdout
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(c/'build-summary.json').write_text(json.dumps(dict(command=cmd,object_hashes={str(p.relative_to(root)):sha(p) for p in objects},driver_sha256=sha(driver),binary_sha256=sha(c/'replay'),positive=p.stdout,negative=bad.stdout,test_consideration='Actual linked heap algorithm;two equal-score removal orders produce different expected poses;wrong expected pose must fail.'),indent=2)+'\n')
print('PASS: actual heap replay built;two tie-order fixtures and wrong-pose rejection')
