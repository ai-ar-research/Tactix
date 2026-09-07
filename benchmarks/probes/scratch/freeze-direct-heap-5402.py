#!/usr/bin/env python3
"""Freeze observer-only source changes after the complete fixed test suite."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5402-direct-heap-observer'
assert '100% tests passed out of 76' in (c / 'logs/ctest.log').read_text()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
sources = {}
for arm, before, after, file in [
    ('tactix', '.claude/worktrees/runtime-window-observer-5399', '.claude/worktrees/runtime-direct-heap-5402', 'src/diagnostics/event_trace.cpp'),
    ('kissat', 'tools/solvers/variants/kissat-window-observer-5399', 'tools/solvers/variants/kissat-direct-heap-5402', 'src/eventtrace.c'),
]:
    r = subprocess.run(['diff', '-u', '--label', 'a/' + file, '--label', 'b/' + file,
                        str(root / before / file), str(root / after / file)], capture_output=True)
    assert r.returncode == 1
    p = c / (arm + '.patch')
    assert not p.exists()
    p.write_bytes(r.stdout)
    sources[arm] = dict(parent=before, source=after, file=file, sha256=sha(root / after / file), patch_sha256=sha(p))
binaries = ['build/pin-5402-direct-heap/tactix', 'tools/solvers/variants/kissat-direct-heap-5402/build/kissat']
m = dict(purpose='Reuse the existing one-shot heap snapshot at the post-decision prefix observer without broad event tracing.',
         environment='SAT_DECISION_HEAP_SNAPSHOT, absent by default; existing HEAP_SET selectors retain the exact boundary.',
         validation='76/76 fixed CTests pass; 5403 must reproduce 5400 cumulative prefix and selected decisions.',
         build_note='Initial Tactix compile failed due to a forward declaration in the anonymous rather than enclosing namespace. Declaration moved before that namespace and rebuilt before any solver execution. No solver or unit-test failure.',
         sources=sources, binaries={p: sha(root / p) for p in binaries},
         logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('5402 observer source and fixed tests frozen')
