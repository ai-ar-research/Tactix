#!/usr/bin/env python3
"""Preserve the failed prototype, not an acceptance or passing correctness record."""
import difflib
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5619-congruence-index'
source = root / '.claude/worktrees/runtime-congruence-index-5619'
parent = root / '.claude/worktrees/runtime-factor-root-support-5592'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
files = ['src/rules/congruence/congruence.cpp', 'tests/test_congruence_index_cleanup.cpp']
patch = ''.join(''.join(difflib.unified_diff(
    (parent / name).read_text().splitlines(True) if (parent / name).exists() else [],
    (source / name).read_text().splitlines(True),
    fromfile='a/' + name if (parent / name).exists() else '/dev/null', tofile='b/' + name)) for name in files)
assert not (c / 'prototype.patch').exists() and not (c / 'manifest.json').exists()
(c / 'prototype.patch').write_text(patch)
builds = ['build/pin-5619-congruence-index-release', 'build/pin-5620-congruence-index-trace',
          'build/pin-5621-congruence-index-bc']
for arm in ('release', 'trace', 'bc'):
    log = (c / 'logs' / ('ctest-' + arm + '.log')).read_text()
    assert 'actual == expected' in log and '1 tests failed' in log
record = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    status='FAILED NEW UNIT FIXTURE: exact proof-output assertion aborts in each build. No benchmark submitted, no promotion. Bounded Claude5623 owns diagnosis and a separate repaired copy; this original remains immutable.',
    purpose='Translate root-satisfied XOR-index cleanup without mutating pure public extraction or changing native size/position visitation.',
    files={name: sha(source / name) for name in files}, patches={'prototype.patch':sha(c / 'prototype.patch')},
    binaries={b + '/tactix':sha(root / b / 'tactix') for b in builds},
    logs={p.name:sha(p) for p in (c / 'logs').iterdir()},
    motivating_evidence={str(p.relative_to(root)):sha(p) for p in [
        root / 'benchmarks/runs/20260908-5613-congruence-clause-values/value-localization.json',
        root / 'benchmarks/runs/20260908-5618-congruence-index-fixture/summary.json']},
    limitation='The failed exact-byte fixture does not itself identify invalid proof emission versus a faulty expectation. Independent micro-proof suites passed. Parent does not inspect bounded-review progress and waits for its terminal root cause before dependent work.')
(c / 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')
print('Preserved original failed5619 prototype and all three build/test identities')
