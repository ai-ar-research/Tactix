#!/usr/bin/env python3
"""Measure actual compiled member offsets without modifying either solver."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
c = root/'benchmarks/runs/20260909-6760-watermark-layout'
assert not c.exists()
c.mkdir()
source = Path(__file__).with_name('watermark-layout-6760.cpp')
compiler = Path('/cm/local/apps/gcc/13.1.0/bin/c++')
hashes = {str(source.relative_to(root)): sha(source), str(Path(__file__).relative_to(root)): sha(Path(__file__))}
records = {}
for arm, tag in (('previous', '6654-congruence-empty-gates'), ('candidate', '6730-reduce-watermark')):
    authority = root/f'benchmarks/runs/20260909-{tag}-build/manifest.json'
    m = json.loads(authority.read_text())
    hashes[str(authority.relative_to(root))] = sha(authority)
    src = root/m['source']
    for rel, digest in m['files'].items():
        assert sha(src/rel) == digest, rel
    command = [str(compiler), '-std=c++20', '-O3', '-DNDEBUG', '-Wall', '-Wextra',
        '-Wno-invalid-offset', '-fno-exceptions', '-fno-rtti', '-I'+str(src/'src'),
        '-I'+str(src/'include'), str(source), '-o', str(c/arm)]
    if arm == 'candidate':
        command.insert(1, '-DWATERMARK_CURRENT')
    r = subprocess.run(command, capture_output=True, text=True)
    (c/(arm+'.compile.log')).write_text(r.stdout+r.stderr)
    assert r.returncode == 0, (arm, r.stderr)
    out = subprocess.check_output([str(c/arm)], text=True)
    (c/(arm+'.out')).write_text(out)
    records[arm] = dict(command=command, offsets={k:int(v) for k,v in (line.split('=') for line in out.splitlines())})
    for p in (c/arm, c/(arm+'.out'), c/(arm+'.compile.log')):
        hashes[str(p.relative_to(root))] = sha(p)
out = dict(records=records, hashes=hashes,
    limitation='Compiled sizeof/offsetof evidence only, not runtime attribution. No solver or submitted source changed. C++ offsetof extension on this compiler; native solver not needed to measure a Tactix-only layout change.',
    test_consideration='Read-only fixed representation probe; compile and exact current/parent offsets are its oracle. No production test change needed. Any chosen layout ablation must preserve all288 watermark regressions and full work/proof replay.')
with (c/'summary.json').open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps(records, indent=2))
