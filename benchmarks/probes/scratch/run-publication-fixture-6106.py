#!/usr/bin/env python3
"""Measure the short actual-rule lifecycle fixture on the immutable parent."""
import json
from pathlib import Path
import shlex
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'benchmarks/runs/20260908-6100-rank37-substitute-deactivation/publication-fixture'
assert not out.exists();out.mkdir()
build=root/'build/pin-6090-macro-heap-trace';source=root/'.claude/worktrees/runtime-macro-heap-6090'
tokens=shlex.split((build/'tests/CMakeFiles/test_event_trace.dir/link.txt').read_text())
objects=[str((build/'tests'/p).resolve()) for p in tokens if p.endswith('.o') and 'test_event_trace.cpp.o' not in p]
cmd=[tokens[0],'-std=c++17','-O2','-DTACTIX_ENABLE_EVENT_TRACE=1','-I'+str(source/'src'),'-I'+str(source/'include'),str(Path(__file__).with_name('substitute-publication-fixture-6105.cpp')),*objects,'-o',str(out/'fixture')]
p=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stderr
p=subprocess.run([str(out/'fixture')],capture_output=True,text=True)
(out/'run.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(p.stdout,p.stderr)
print(p.stdout)
