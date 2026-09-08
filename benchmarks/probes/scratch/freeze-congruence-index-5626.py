#!/usr/bin/env python3
"""Bind the validated source and compare actual old/new/native fixed cases."""
import difflib
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5626-congruence-index-validated'
source = root / '.claude/worktrees/runtime-congruence-index-repair-5623'
parent = root / '.claude/worktrees/runtime-factor-root-support-5592'
review = root / 'benchmarks/runs/20260908-5623-congruence-index-repair'
native = root / 'benchmarks/runs/20260908-5618-congruence-index-fixture'
builds = {arm: root / ('build/pin-congruence-index-repair-5623-' + arm) for arm in ('release','bc','trace')}
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix') == '351bef360ee6427cf40ea3450f3777a615779c3377cb26721a4435ad2b79073e'
for arm in builds:
    assert '100% tests passed out of ' + str(87 if arm == 'trace' else 88) in (review / 'logs' / ('ctest-' + arm + '.log')).read_text()
c.mkdir(exist_ok=False)
(c / 'logs').mkdir()
def run(name, command):
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    (c / 'logs' / (name + '.log')).write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (name,r.returncode)
    return r.stdout
old = root / 'build/pin-5592-factor-root-support-release'
command = shlex.split((old / 'tests/CMakeFiles/test_kernel.dir/link.txt').read_text())
objects = [str((old / 'tests' / p).resolve()) for p in command if p.endswith('.o') and 'test_kernel.cpp.o' not in p]
old_bin = c / 'old-fixture'
run('compile-old', [command[0], '-std=c++20', '-O3', '-DTACTIX_EXPECT_RETAINED_CONGRUENCE_INDEX',
    '-I' + str(parent / 'src'), str(source / 'tests/test_congruence_index_cleanup.cpp'), *objects, '-o', str(old_bin)])
before = run('old', [old_bin]).splitlines()
after = run('new', [builds['release'] / 'tests/test_congruence_index_cleanup']).splitlines()
assert before[-1] == after[-1] == 'PASS:1200 actual Congruence cleanup cases, pure extraction and exact proof deletions'
fixture = json.loads((native / 'summary.json').read_text())
assert sha(native / 'native') == fixture['hashes'][str((native / 'native').relative_to(root))]
reference = [list(map(int,row.split())) for row in run('native', [native / 'native']).splitlines()]
before, after = [[list(map(int,row.split())) for row in data[:-1]] for data in (before,after)]
assert len(before) == len(after) == len(reference) == 75 and after == reference
changes = [dict(parameters=a[:3], previous=a[3], candidate=b[3]) for a,b in zip(before,after) if a != b]
assert len(changes) == 24 and all(row['previous']==0 and row['candidate']==1 for row in changes)
files = ['src/rules/congruence/congruence.cpp', 'tests/test_congruence_index_cleanup.cpp']
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff((base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True), fromfile='a/' + name if (base / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
(c / 'repair.patch').write_text(patch(parent,files))
(c / 'cumulative.patch').write_text(patch(root,[str(p.relative_to(root)) for p in sorted((root/'src').rglob('*')) if p.is_file()]))
evidence = root / 'benchmarks/runs/20260908-5613-congruence-clause-values/value-localization.json'
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Retire visited root-satisfied irredundant clauses during applying XOR/ITE indexing, including no-merge calls; preserve pure public extraction and native size/position scope.',
    evidence=dict(path=str(evidence.relative_to(root)),sha256=sha(evidence)),
    review=dict(path=str((review/'REPORT.md').relative_to(root)),sha256=sha(review/'REPORT.md'),outcome='PASS: test expected pre-rebuild DRAT order. Solver unchanged; only corrected fixture in review copy.'),
    test_consideration='88/88 each independent Release,87/87 trace.1200 actual old/new cases;75 native index cases agree with candidate,24 targeted differences. Exact DRAT/LRAT deletion bytes, duplicate/redundant controls, public purity/guard/instance, root values and native cap are covered.',
    cases=1200,native_cases=75,changes=changes,
    status='Isolated UNFUZZED validated successor to5592; no runtime claim or promotion. Fuzz5610 covers5592,not this source. B28+C5 and complete Rank1-40 required.',
    files={name:sha(source/name) for name in files},
    binaries={str((b/'tactix').relative_to(root)):sha(b/'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},logs={p.name:sha(p) for p in (c/'logs').iterdir()},
    reference_identity=fixture['hashes'],old_object_identity={str(Path(p).relative_to(root)):sha(Path(p)) for p in objects},
    full_suite_logs={str(p.relative_to(root)):sha(p) for p in (review/'logs').glob('ctest-*.log')})
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('PASS:1200 actual old/new cases;75 native cases agree;24 targeted cleanup differences;88/88 twice,87/87 trace')
