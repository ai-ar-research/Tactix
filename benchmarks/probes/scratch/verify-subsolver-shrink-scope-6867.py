#!/usr/bin/env python3
"""Verify exact shrink-only source scope and measure the compiled footprint."""
from pathlib import Path
import json
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods
c = root/'benchmarks/runs/20260910-6857-subsolver-shrink-build'
b = json.loads((c/'manifest.json').read_text())
source = root/b['source']; parent = root/b['parent']
changed = []
for rel,digest in b['files'].items():
    assert sha(source/rel) == digest
    if not (parent/rel).exists() or sha(parent/rel) != digest: changed.append(rel)
assert set(changed) == {'src/engine/subsolver.cpp','tests/test_subsolver_shrink.cpp'}
old = (parent/'src/engine/subsolver.cpp').read_text()
new = (source/'src/engine/subsolver.cpp').read_text()
body = old[old.index('bool SubSolver::propagate()'):old.index('void SubSolver::backtrack(')]
assert body.count('ws.resize(j);') == 2
comment = '        // Compaction only removes entries; expose that invariant so the\n        // vector operation cannot introduce a generic grow/initialize path.\n'
assert new.replace(comment,'').replace('ws.erase(ws.begin() + j, ws.end());','ws.resize(j);') == old
sizes = {}
for arm,stem in (('previous','6730-reduce-watermark'),('candidate','6857-subsolver-shrink')):
    pin = root/f'build/pin-{stem}-release/tactix'
    rows = subprocess.check_output(['nm','-S','--defined-only',str(pin)],text=True).splitlines()
    matching = [line.split() for line in rows if line.endswith(' _ZN6tactix9SubSolver9propagateEv')]
    assert len(matching) == 1
    address,size,kind,symbol = matching[0]
    sizes[arm] = dict(address='0x'+address,bytes=int(size,16),binary_sha256=sha(pin))
annotation = root/'benchmarks/runs/20260910-6856-subsolver-propagation-annotation/summary.json'
a = json.loads(annotation.read_text())
for rel,digest in a['hashes'].items(): assert sha(root/rel) == digest
parsed = parse_periods((annotation.parent/'annotation.txt').read_text(),'tactix::SubSolver::propagate()')
assert parsed.total_period == a['total_period']
period = parsed.range_period(0x4e8e02,0x4e8e38)
program = 1621888383441
header = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/profiles/002-self.txt'
assert f'# Event count (approx.): {program}' in header.read_text()
paths = [Path(__file__),c/'manifest.json',c/'control-6858.json',annotation,header,
    source/'src/engine/subsolver.cpp',parent/'src/engine/subsolver.cpp',source/'src/engine/subsolver.hpp',
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/kitten.c',root/'tools/solvers/variants/cadical-extdiag/src/kitten.c']
out = dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},changed=changed,compiled=sizes,
    sampled_success_exit=dict(begin='0x4e8e02',end='0x4e8e38',period=period,
        percent_function=100*period/parsed.total_period,percent_program=100*period/program),
    scope='j never exceeds n: each visited watch contributes at most one survivor; conflict copies only the remaining suffix. Erasing[j,end) preserves capacity/order and is empty when nothing moved. SubWatch is trivial. Both exits change; all assignment/proof/counter/budget/propagation order and source outside these statements remains exact. No private representation/API/header change.',
    reference='Kissat and CaDiCaL compact with read/write pointers and publish the final watch end directly. Both retain unread suffix on conflict. This matches that shrink-only operation with the existing std::vector representation; no change to embedded binary tagging or reference width.',
    limitation='Sampled exit region includes useful list bookkeeping and is only about0.355% of program periods; not removable-cost ceiling or predicted savings. Code-size reduction is not measured speedup. Native work differs on39. Keep exact-work B/C and three paired timing waves;current-source fuzz/memory/full50 acceptance open.')
with (c/'scope-6867.json').open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(json.dumps(dict(compiled=sizes,sampled_success_exit=out['sampled_success_exit']),indent=2))
