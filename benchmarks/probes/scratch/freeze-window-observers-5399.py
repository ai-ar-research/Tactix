#!/usr/bin/env python3
"""Freeze isolated diagnostic-only source deltas and build validation."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5399-window-observers'
body = (c / 'logs/ctest.log').read_text()
assert '100% tests passed out of 76' in body and '***Failed' not in body
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
patches = {}
for arm, before, after, file in [
    ('tactix', '.claude/worktrees/runtime-eliminate-dense-entry-5391', '.claude/worktrees/runtime-window-observer-5399', 'src/diagnostics/event_trace.cpp'),
    ('kissat', 'tools/solvers/variants/kissat-retry-producers-5377', 'tools/solvers/variants/kissat-window-observer-5399', 'src/eventtrace.c'),
]:
    r = subprocess.run(['diff', '-u', '--label', 'a/' + file, '--label', 'b/' + file,
                        str(root / before / file), str(root / after / file)], capture_output=True)
    assert r.returncode == 1
    p = c / (arm + '-window.patch')
    assert not p.exists()
    p.write_bytes(r.stdout)
    patches[arm] = dict(parent=before, source=after, file=file,
                        sha256=sha(root / after / file), patch_sha256=sha(p))
binaries = ['build/pin-5399-window-observer/tactix',
            'tools/solvers/variants/kissat-window-observer-5399/build/kissat']
m = dict(purpose='Compile-time-gated read-only decision stream lower-bound filter; cumulative prefix hash remains unfiltered.',
         environment='SAT_DECISION_STREAM_CONFLICT_FROM, default zero; no production selection or semantic state change.',
         validation='76/76 full trace CTests, 21.22 seconds. Initial sealer expected a different CTest summary phrase; corrected to the actual successful log before any freeze. Actual old/new observer byte-equivalence is the separate 5400 SLURM gate.',
         build_note='Native initially configured with -g but was reconfigured to original -O3 -DNDEBUG before any compilation/execution. Existing diagnostic snprintf truncation warning retained; new filter adds no formatting.',
         sources=patches, binaries={p: sha(root / p) for p in binaries},
         logs={p.name: sha(p) for p in (c / 'logs').iterdir()})
p = c / 'manifest.json'
assert not p.exists()
p.write_text(json.dumps(m, indent=2) + '\n')
print('5399: observer-only deltas frozen, full trace tests pass')
