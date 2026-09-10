#!/usr/bin/env python3
"""Preserve the parent's unsatisfiable fixture setup before correcting it."""
from pathlib import Path
import json
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6841-sweep-rootfree-control'
log = c/'logs/previous-test_sweep_rootfree-run.json'
data = json.loads(log.read_text())
assert data['exit_code'] == -6 and '!original_models.empty()' in data['stderr']
fixture = root/'.claude/worktrees/runtime-sweep-rootfree-6838/tests/test_sweep_rootfree.cpp'
assert '{6,5,-1}' in fixture.read_text()
archive = c/'failed-test_sweep_rootfree.cpp'
assert not archive.exists()
shutil.copyfile(fixture, archive)
paths = [log, archive, Path(__file__), Path(__file__).with_name('control-sweep-rootfree-6841.py')]
with (c/'fixture-failure-6842.json').open('x') as stream:
    json.dump(dict(classification='Parent fixture asserts before environment construction: root mode4 assigns -5,-6; binary (5 or 1) forces1 while clause(6 or 5 or -1) forces-1. Contradictory test setup, not candidate solver failure. Replace the last sign with+1 so the exhaustive nonempty-model oracle applies.',
        original_fixture_sha256=sha(fixture), hashes={str(p.relative_to(root)):sha(p) for p in paths}), stream, indent=2)
    stream.write('\n')
print('Frozen fixture failure6842')
