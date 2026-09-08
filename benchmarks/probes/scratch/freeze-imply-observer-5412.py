#!/usr/bin/env python3
"""Freeze the tested diagnostic-only filter and its focused fixture."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5412-imply-observer'
assert '100% tests passed out of 76' in (c / 'logs/ctest-2.log').read_text()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
sources = {}
for arm, before, after, files in [
    ('tactix', '.claude/worktrees/runtime-direct-heap-5402', '.claude/worktrees/runtime-imply-observer-5412', ['src/diagnostics/event_trace.cpp', 'tests/test_event_trace.cpp']),
    ('kissat', 'tools/solvers/variants/kissat-direct-heap-5402', 'tools/solvers/variants/kissat-imply-observer-5412', ['src/eventtrace.c']),
]:
    patch = b''
    for file in files:
        r = subprocess.run(['diff', '-u', '--label', 'a/' + file, '--label', 'b/' + file, str(root / before / file), str(root / after / file)], capture_output=True)
        assert r.returncode == 1
        patch += r.stdout
    p = c / (arm + '.patch')
    assert not p.exists()
    p.write_bytes(patch)
    sources[arm] = dict(parent=before, source=after, files={f: sha(root / after / f) for f in files}, patch_sha256=sha(p))
binaries = ['build/pin-5412-imply-observer/tactix', 'tools/solvers/variants/kissat-imply-observer-5412/build/kissat']
m = dict(purpose='IMPLY_ONLY rejects unrelated detail helpers before expensive payload construction; implication owner retains its original conflict window and selected-literal filter.',
         validation='76/76 fixed tests, including a non-vacuous warm-up implication window fixture. 5413 must reproduce 5410 prefixes/decisions.',
         initial_failure='Static overlay fixture asserted the literal spelling of the existing learned-only guard. Restored that guard unchanged and added a standalone imply-only early return. No solver crash, soundness defect, or proof/model failure; primary-owned ordinary observer implementation debugging under plan 8.2.',
         sources=sources, binaries={p: sha(root / p) for p in binaries},
         logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('PASS: tested early-filter observer frozen')
