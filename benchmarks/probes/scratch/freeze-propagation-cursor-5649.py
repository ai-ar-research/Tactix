#!/usr/bin/env python3
"""Bind the validated cursor implementation, review, tests and actual assembly."""
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5649-propagation-cursor-validated'
source = root / '.claude/worktrees/runtime-propagation-cursor-repair-5647'
parent = root / '.claude/worktrees/runtime-congruence-index-repair-5623'
review = root / 'benchmarks/runs/20260908-5647-propagation-cursor-repair'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
builds = {arm: root / ('build/pin-propagation-cursor-repair-5647-' + arm) for arm in ('release', 'bc', 'trace')}
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix') == '032ef0ba2d3cac6f3579b2e158de36488a1110e2b586e81d5ca7c62f12cdbbb8'
for arm in builds:
    assert '100% tests passed out of ' + str(88 if arm == 'trace' else 89) in (review / 'logs' / ('ctest-' + arm + '.r1.log')).read_text()
assert 'f82475e400a7f62b' in (review / 'predecessor-objlink/run.r1.log').read_text()
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
files = ['src/kernel/kernel.cpp', 'tests/test_propagation_cursor.cpp']
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (base / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
(c / 'repair.patch').write_text(patch(parent, files))
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for arm, binary in [('previous', root / 'build/pin-congruence-index-repair-5623-release/tactix'), ('candidate', builds['release'] / 'tactix')]:
    r = subprocess.run(['objdump', '-d', '-C', '--disassemble=tactix::kernel_propagate(tactix::Kernel&, unsigned int)', binary], check=True, capture_output=True, text=True)
    (c / 'logs' / (arm + '-propagate.asm')).write_text(r.stdout)
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='General propagation unit-cost reduction: per-call ignore specialization and local trail cursor, unchanged exit state and diagnostic cursor visibility.',
    test_consideration='89/89 each independent Release;88/88 trace.224 fixed scenarios plus canonical dispatch tests;actual predecessor/new state digest f82475e400a7f62b identical. Existing micro-proof/model/API suites cover unchanged semantics;long-run gates still required.',
    review=dict(path=str((review / 'REPORT.md').relative_to(root)), sha256=sha(review / 'REPORT.md'), outcome='PASS: null raw registry instances pointer in fixture; solver unchanged by repair.'),
    files={name: sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)): sha(b / 'tactix') for b in builds.values()},
    patches={p.name: sha(p) for p in c.glob('*.patch')}, logs={p.name: sha(p) for p in (c / 'logs').iterdir()},
    review_evidence={str(p.relative_to(root)): sha(p) for p in review.rglob('*') if p.is_file() and p.name != 'test_propagation_cursor_predecessor'},
    source_baseline_sha256=sha(parent / 'src/kernel/kernel.cpp'),
    machine_code='Candidate dispatches on ignore at41370e before either trail loop;ordinary loop increments ebp at413835 and publishes cursor at exit. Previous/candidate full disassembly retained;no measured speedup yet.',
    status='Isolated UNFUZZED successor to5626. Claude5610 PASS covers5592 only,not5626 or this kernel change. B28+C5,periodic fuzz and full Rank1-40 goal remain required;no promotion.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('Validated5649:89/89 twice,88/88 trace;224 exact-state cases;no runtime claim')
