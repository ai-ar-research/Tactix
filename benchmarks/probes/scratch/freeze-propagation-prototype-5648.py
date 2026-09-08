#!/usr/bin/env python3
"""Preserve the failed fixture and unsubmitted prototype for bounded repair."""
import difflib
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5641-propagation-cursor'
source = root / '.claude/worktrees/runtime-propagation-cursor-5641'
parent = root / '.claude/worktrees/runtime-congruence-index-repair-5623'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
files = ['src/kernel/kernel.cpp', 'tests/test_propagation_cursor.cpp']
patch = ''.join(''.join(difflib.unified_diff(
    (parent / name).read_text().splitlines(True) if (parent / name).exists() else [],
    (source / name).read_text().splitlines(True),
    fromfile='a/' + name if (parent / name).exists() else '/dev/null', tofile='b/' + name)) for name in files)
assert not (c / 'prototype.patch').exists() and not (c / 'manifest.json').exists()
(c / 'prototype.patch').write_text(patch)
builds = ['build/pin-5641-propagation-cursor-release', 'build/pin-5642-propagation-cursor-trace',
          'build/pin-5643-propagation-cursor-bc']
for arm in ('release', 'trace', 'bc'):
    log = (c / 'logs' / ('ctest-' + arm + '.r1.log')).read_text()
    assert 'SEGFAULT' in log and '1 tests failed' in log
record = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    status='FAILED NEW UNIT FIXTURE: segfault in all three builds. No benchmark submitted or promotion. Claude5647 owns separate-copy bounded diagnosis; original immutable.',
    purpose='Behavior-preserving local propagation cursor and per-call ignored-clause specialization; diagnostics retain per-scan cursor publication.',
    files={name: sha(source / name) for name in files}, patches={'prototype.patch': sha(c / 'prototype.patch')},
    binaries={b + '/tactix': sha(root / b / 'tactix') for b in builds},
    logs={p.name: sha(p) for p in (c / 'logs').iterdir()},
    limitation='Initial nonexistent ReservedTrail shrink_to_fit API caused compilation failure and was corrected before builds succeeded. Subsequent runtime segfault is unresolved; development and candidate submissions pause until bounded owner terminal outcome. Existing other tests pass; no failed real-instance oracle yet.')
(c / 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')
print('Preserved original failed5641 source, pins and logs')
