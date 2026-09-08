#!/usr/bin/env python3
"""Freeze the follow-up observer gate before heap-variable selection."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5414-imply-fast'
assert '100% tests passed out of 76' in (c / 'logs/ctest.log').read_text()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
sources = {}
for arm, before, after, file in [
    ('tactix', '.claude/worktrees/runtime-imply-observer-5412', '.claude/worktrees/runtime-imply-fast-5414', 'src/diagnostics/event_trace.cpp'),
    ('kissat', 'tools/solvers/variants/kissat-imply-observer-5412', 'tools/solvers/variants/kissat-imply-fast-5414', 'src/eventtrace.c'),
]:
    r = subprocess.run(['diff', '-u', '--label', 'a/' + file, '--label', 'b/' + file, str(root / before / file), str(root / after / file)], capture_output=True)
    assert r.returncode == 1
    p = c / (arm + '.patch')
    assert not p.exists()
    p.write_bytes(r.stdout)
    sources[arm] = dict(parent=before, source=after, file=file, sha256=sha(root / after / file), patch_sha256=sha(p))
binaries = ['build/pin-5414-imply-fast/tactix', 'tools/solvers/variants/kissat-imply-fast-5414/build/kissat']
m = dict(purpose='IMPLY_ONLY also rejects heap insert/remove/pop before selected-variable matching and environment lookup on every heap move. Diagnostic-only; no solver-state change.',
         parent_authority='benchmarks/runs/20260907-5412-imply-observer/manifest.json',
         validation='76/76 fixed tests, including the inherited selected warm-up implication fixture. New stopped capture must reproduce 5410 prefix and decisions.',
         sources=sources, binaries={p: sha(root / p) for p in binaries},
         logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
assert not (c / 'manifest.json').exists()
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('PASS: early heap-helper gate and tests frozen')
