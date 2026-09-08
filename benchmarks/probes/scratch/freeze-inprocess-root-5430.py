#!/usr/bin/env python3
"""Freeze general phase-entry closure after actual old/new and full fixed tests."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5430-inprocess-root'
source = root / '.claude/worktrees/runtime-inprocess-root-5430'
parent = root / '.claude/worktrees/runtime-rephase-root-5417'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for name,count in [('release',78),('trace',77),('bc',78)]:
    assert f'100% tests passed out of {count}' in (c / 'logs' / f'ctest-{name}.log').read_text()
old = (c / 'logs/old-entry-fixture.log').read_text()
assert old.count('root value 0, propagated 0, search ticks 0') == 2
fixture_log = root / 'build/pin-5430-inprocess-root-release/Testing/Temporary/LastTest.log'
text = fixture_log.read_text()
for marker in ('probe entry: root value 1, propagated 3, search ticks 0',
               'eliminate entry: root value 1, propagated 3, search ticks 6'):
    assert marker in text
files = ['src/policies/default_policy.cpp','tests/test_inprocess_root.cpp']
def patch(before, paths):
    result = ''
    for f in paths:
        a,b = before/f,source/f
        result += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],b.read_text().splitlines(True),fromfile='a/'+f if a.exists() else '/dev/null',tofile='b/'+f))
    return result
for name,body in [('repair.patch',patch(parent,files)),('cumulative.patch',patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))]:
    assert not (c/name).exists()
    (c/name).write_text(body)
builds = ['build/pin-5430-inprocess-root-release/tactix','build/pin-5431-inprocess-root-trace/tactix','build/pin-5432-inprocess-root-bc/tactix']
assert sha(root/builds[0]) == sha(root/builds[2])
m = dict(purpose='Restore root closure before Probe/Eliminate rule admission and dense construction, with reference-specific phase-saving and search-tick scope.',
    source=str(source.relative_to(root)),parent=str(parent.relative_to(root)),production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    references=['Kissat probe.c:probe and kissat_probe, eliminate.c:eliminate, backtrack.c:kissat_backtrack_propagate_and_flush_trail','CaDiCaL elim.cpp elimination entry root propagation; same root-fixpoint prerequisite, separate architecture'],
    semantics='Policy composition only: Propagate and Restart abstract guards/effects and public instances unchanged. Probe entry suppresses phase saving/search ticks; Eliminate entry uses search propagation before the simplifier scope. Existing root proof service handles terminal conflict; no new proof vocabulary.',
    fixture='Actual old run_phase with empty pipelines misses retained-root consequence -1: 0 propagations in both; Eliminate exit falsely closes the cursor. New run_phase derives -1 at root with 3 scans; Probe search ticks 0, Eliminate 6; phase saving and canonical restart count checked. Both root-conflict exits restore scope and leave no dense attachment.',
    old_fixture_log_sha256=sha(c/'logs/old-entry-fixture.log'),
    status='Isolated UNFUZZED semantic candidate, independent of metadata 5426. No runtime acceptance or production promotion.',
    files={f:sha(source/f) for f in files},binaries={p:sha(root/p) for p in builds},
    patches={p.name:sha(p) for p in c.glob('*.patch')},logs={p.name:sha(p) for p in (c/'logs').iterdir()})
assert not (c/'manifest.json').exists()
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('PASS: trace 77/77, independent Release 78/78 each, identical Release hashes; phase-entry source frozen')
