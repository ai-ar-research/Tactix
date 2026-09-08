#!/usr/bin/env python3
"""Run identical fixed cases against actual predecessor and candidate objects."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5546-vivify-census'
old = root / 'build/pin-5522-sweep-small-release'
candidate = root / 'build/pin-5546-vivify-census-release'
source = root / '.claude/worktrees/runtime-vivify-census-5546/tests/test_vivify_census.cpp'
link = shlex.split((old / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((old / 'tests' / arg).resolve()) for arg in link
           if arg.endswith('.o') and 'test_kernel.cpp.o' not in arg]
target = root / 'benchmarks/probes/scratch/vivify-census-old-5548'
def run(name, command):
    path = c / 'logs' / (name + '.log')
    assert not path.exists()
    result = subprocess.run(command, cwd=c, capture_output=True, text=True)
    path.write_text(result.stdout + result.stderr)
    assert result.returncode == 0, (name, result.returncode, str(path))
    return result.stdout
assert not target.exists()
run('compile-old-5548', [link[0], '-std=c++20', '-O3',
    '-I' + str(root / '.claude/worktrees/runtime-sweep-small-5522/src'),
    str(source), *objects, '-o', str(target)])
before = run('old-cases-5548', [str(target)])
after = run('candidate-cases-5548', [str(candidate / 'tests/test_vivify_census')])
assert len(before.splitlines()) == 192
assert before == after, 'Schedule/state/proof identity failed; inspect before timing'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
paths = [source, target, candidate / 'tests/test_vivify_census', *(Path(p) for p in objects)]
result = dict(cases=192,
    coverage='Six scopes x four populations x reuse on/off x flush on/off x DRAT/LRAT. Empty/red-only/mixed/irredundant, live/garbage/priority/root-false, query purity/order, bounded+unbounded application, arena/watch/assignment/statistics/proof-byte digests.',
    result='Byte-identical actual5522 and5546 fixed-case outputs.',
    limitation='Differential fixed-state/proof-byte regression, not independently checked full benchmark verdicts or generated fuzz reachability.',
    hashes={str(p.relative_to(root)): sha(p) for p in paths})
path = c / 'state-validation.json'
assert not path.exists()
path.write_text(json.dumps(result, indent=2) + '\n')
print('PASS:192 actual old/new Vivify schedule/state/proof cases')
