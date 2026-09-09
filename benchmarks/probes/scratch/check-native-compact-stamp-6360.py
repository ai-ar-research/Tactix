#!/usr/bin/env python3
"""Validate first-fixed stamp ownership through the actual native collector."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6360-native-compact-stamp'
assert not c.exists()
c.mkdir()
source = root / 'tools/solvers/variants/kissat-macro-heap-6090'
fixture = Path(__file__).with_name('test-native-compact-stamp-6360.c')
library = source / 'build/libkissat.a'
command = ['gcc', '-O2', '-I'+str(source/'src'), '-I'+str(source/'build'),
           str(fixture), str(library), '-lm', '-o', str(c/'fixture')]
with (c/'build.log').open('x') as out:
    subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, check=True)
p = subprocess.run([str(c/'fixture')], text=True, capture_output=True, cwd=c)
(c/'fixture.log').write_text(p.stdout + p.stderr)
assert p.returncode == 0, (p.returncode, p.stdout, p.stderr)
(c/'summary.json').write_text(json.dumps(dict(command=command,
    fixture_sha256=sha(fixture), library_sha256=sha(library),
    source_sha256=sha(source/'src/compact.c'), output=p.stdout,
    conclusion='Native finalize_compacting clears later fixed exports BEFORE compact_queue. The first fixed representative retains its own stamp, not the last alias stamp.'), indent=2)+'\n')
print(p.stdout)
