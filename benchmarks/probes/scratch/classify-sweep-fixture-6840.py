#!/usr/bin/env python3
"""Freeze the compile-only fixture failure before correcting the test."""
from pathlib import Path
import json
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6838-sweep-rootfree-build'
out = c/'fixture-failure-6840.json'
assert not out.exists()
log = c/'logs/previous-test_sweep_rootfree-link.json'
data = json.loads(log.read_text())
assert data['exit_code'] == 1 and 'ReservedTrail' in data['stderr']
fixture = root/'.claude/worktrees/runtime-sweep-rootfree-6838/tests/test_sweep_rootfree.cpp'
archive = c/'failed-test_sweep_rootfree.cpp'
assert not archive.exists()
shutil.copyfile(fixture, archive)
paths = [log, archive, Path(__file__), Path(__file__).with_name('control-sweep-rootfree-6839.py')]
with out.open('x') as stream:
    json.dump(dict(classification='Fixed fixture compilation failed: vector-only hash template cannot accept ReservedTrail. Neither old/new test execution nor candidate compilation occurred. Not a solver runtime or correctness verdict.',
        original_fixture=str(fixture.relative_to(root)), original_fixture_sha256=sha(fixture),
        hashes={str(p.relative_to(root)):sha(p) for p in paths}), stream, indent=2)
    stream.write('\n')
print(out)
