#!/usr/bin/env python3
"""Freeze tested read-only Factor event selection, separate from runtime pins."""
import difflib
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5456-factor-observer'
assert '100% tests passed out of 81' in (c/'logs/ctest-tactix.log').read_text()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sources={}
for arm,before,after,files in [
 ('tactix','.claude/worktrees/runtime-eliminate-limit-5450','.claude/worktrees/runtime-factor-observer-5456',
  ['src/diagnostics/event_trace.hpp','src/diagnostics/event_trace.cpp','tests/test_factor_observer.cpp']),
 ('kissat','tools/solvers/variants/kissat-imply-fast-5414','tools/solvers/variants/kissat-factor-observer-5456',['src/eventtrace.c'])]:
    patch=''
    for name in files:
        a,b=root/before/name,root/after/name
        patch+=''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],b.read_text().splitlines(True),
                       fromfile='a/'+name if a.exists() else '/dev/null',tofile='b/'+name))
    p=c/(arm+'.patch');assert not p.exists();p.write_text(patch)
    sources[arm]=dict(parent=before,source=after,files={n:sha(root/after/n) for n in files},patch_sha256=sha(p))
binaries=['build/pin-5456-factor-observer-trace/tactix','tools/solvers/variants/kissat-factor-observer-5456/build/kissat']
m=dict(purpose='Read-only SAT_EVENT_TRACE_FACTOR_ONLY selection rejects unrelated events and out-of-window Factor records before expensive snapshot extraction. Standalone prefix/boundary observers stay independent.',
       validation='81/81 fixed trace tests, including selected family/window and inherited phase-budget fixture. Real capture must exactly preserve each predecessor stream through its stop.',
       sources=sources,binaries={n:sha(root/n) for n in binaries},logs={p.name:sha(p) for p in (c/'logs').iterdir()})
p=c/'manifest.json';assert not p.exists();p.write_text(json.dumps(m,indent=2)+'\n')
print(json.dumps(m['binaries'],indent=2))
