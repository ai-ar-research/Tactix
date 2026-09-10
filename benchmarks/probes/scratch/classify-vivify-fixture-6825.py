#!/usr/bin/env python3
"""Preserve the failed fixed-fixture source before correcting renamed-main UB."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6822-vivify-snapshot-build'
target = c/'fixture-failure-6825.json'
assert not target.exists()
fixture = root/'.claude/worktrees/runtime-vivify-snapshot-6822/tests/test_vivify_snapshot.cpp'
included = fixture.with_name('test_vivify_count_cache.cpp')
link = json.loads((c/'logs/previous-test_vivify_snapshot-link.json').read_text())
run = json.loads((c/'logs/previous-test_vivify_snapshot-run.json').read_text())
assert 'control reaches end of non-void function' in link['stderr'] or 'no return statement' in link['stderr']
assert run['exit_code'] == -11
assert '#define main vivify_count_cache_matrix' in fixture.read_text()
assert 'return 0;' not in included.read_text()
paths = [fixture, included, c/'previous-test_vivify_snapshot', c/'previous-vivify.o',
         *sorted((c/'logs').glob('*.json'))]
for p in (fixture, included):
    dest = c/('failed-'+p.name)
    assert not dest.exists()
    shutil.copy2(p, dest)
    paths.append(dest)
out = dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},
    diagnosis='Fixture renamed a C++ main without adding an explicit return. Only main has implicit return0. Compiler warns and unchanged parent-linked fixture exitsSIGSEGV after that UB. Candidate object was not compiled or run; this is not solver soundness evidence. Retain logs/source, add explicit return in the private test helper and rerun separate6824 actual old/new controls.',
    no_solver_input='Fixed unit fixture only; no SLURM benchmark or fuzz campaign involved.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
