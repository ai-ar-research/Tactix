#!/usr/bin/env python3
"""Actual-code root-mark control and positive/negative observer selection."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5817-forward-mark-observer'
out = c / 'fixed-control'
out.mkdir(exist_ok=False)
build = root / 'build/pin-5817-forward-mark-trace'
source = root / '.claude/worktrees/runtime-forward-mark-observer-5817'
native = root / 'tools/solvers/variants/kissat-score-bump-observer-5696'
fixtures = root / 'benchmarks/probes/scratch'
link = shlex.split((build / 'tests/CMakeFiles/test_forward_unit.dir/link.txt').read_text())
objects = [(build / 'tests' / name).resolve() for name in link if name.endswith('.o')][1:]
commands = []
def run(label,command,env=None):
    commands.append([str(x) for x in command])
    r = subprocess.run(command,cwd=root,env=env,capture_output=True,text=True)
    (out / (label + '.log')).write_text(r.stdout+r.stderr)
    assert r.returncode == 0,(label,r.returncode,r.stdout,r.stderr)
    return r.stdout
run('compile-tactix',[link[0],'-O3','-DNDEBUG','-std=c++20','-fno-exceptions','-fno-rtti',
    '-I'+str(source / 'src'),'-I'+str(source / 'include'),fixtures / 'forward-root-mark-control-5820.cpp',
    *objects,'-o',out / 'tactix'])
run('compile-native',['gcc','-O3','-DNDEBUG','-I'+str(native / 'src'),
    fixtures / 'forward-root-mark-native-5820.c',native / 'build/libkissat.a','-lm','-o',out / 'native'])
clean = {k:v for k,v in os.environ.items() if not k.startswith(('TACTIX_','KISSAT_','SAT_'))}
base = run('tactix-off',[out / 'tactix'],clean)
positive = dict(clean,TACTIX_FORWARD_MARK_TRACE=str(out / 'positive.tsv'),SAT_FORWARD_MARK_CONFLICT='0',
    TACTIX_EVENT_TRACE_FORWARD_CLAUSE='1,2,3')
assert run('tactix-positive',[out / 'tactix'],positive) == base
negative = dict(positive,TACTIX_FORWARD_MARK_TRACE=str(out / 'negative.tsv'),SAT_FORWARD_MARK_CONFLICT='1')
assert run('tactix-negative',[out / 'tactix'],negative) == base
assert not (out / 'negative.tsv').exists()
lines = (out / 'positive.tsv').read_text().splitlines()
assert len(lines) == 8 and lines[0].startswith('boundary\tstage=before-sync') and lines[4].startswith('boundary\tstage=after-sync')
rows = [dict(field.split('=',1) for field in line.split('\t')[1:]) for line in lines]
assert rows[1]['external'] == rows[5]['external'] == '1'
assert rows[1]['value'] == rows[5]['value'] == '-1'
assert rows[1]['marked'] == '0' and rows[5]['marked'] == '1'
assert sum(int(row['marked']) for row in rows[1:4]) == 1
assert sum(int(row['marked']) for row in rows[5:8]) == 2
native_output = run('native',[out / 'native'],clean)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
paths = [fixtures / 'forward-root-mark-control-5820.cpp',fixtures / 'forward-root-mark-native-5820.c',
    out / 'tactix',out / 'native',native / 'build/libkissat.a',*objects]
(out / 'summary.json').write_text(json.dumps(dict(commands=commands,tactix=base,native=native_output,
    observer_positive_negative_pass=True,root_mark_before=0,root_mark_after=1,
    hashes={str(p.relative_to(root)):sha(p) for p in paths},
    limitation='Fixed actual-code scheduling difference and observer selection only;real Rank32 mark provenance and full parent replay still required.'),indent=2)+'\n')
print('PASS actual native/Tactix root-mark difference;positive/negative observer controls')
