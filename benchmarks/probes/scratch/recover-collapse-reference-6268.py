#!/usr/bin/env python3
"""Compile native C headers as C; preserve the failed C++ setup log."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6268-collapse-retry-native'
assert not c.exists(); c.mkdir()
old = root / 'benchmarks/runs/20260909-6267-collapse-retry-fixture'
fixture = root / 'benchmarks/probes/scratch/test-native-collapse-retry-6268.c'
n = root / 'tools/solvers/variants/kissat-schedule-continue-6206'
command = ['gcc', '-O2', '-I'+str(n/'src'), '-I'+str(n/'build'), str(fixture), str(n/'build/libkissat.a'), '-lm', '-o', str(c/'native')]
with (c / 'native.log').open('w') as log:
    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run([str(c/'native')], stdout=log, stderr=subprocess.STDOUT, check=True)
objects = sorted((root/'build/pin-6231-substitute-retry-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
summary = dict(command=command, fixture_sha256=sha(fixture),
    source_objects={str(p.relative_to(root)): sha(p) for p in objects}, native_library_sha256=sha(n/'build/libkissat.a'),
    parent_fixture_sha256=sha(root/'benchmarks/probes/scratch/test-substitute-collapse-retry-6267.cpp'),
    results=dict(candidate=(old/'candidate.log').read_text(), native=(c/'native.log').read_text()),
    failed_compile=dict(path=str((old/'native.log').relative_to(root)), sha256=sha(old/'native.log'),
        cause='Native internal C struct contains export, a reserved C++ keyword; also lacks C++ initializer_list. Compile unchanged native headers via C fixture. No native executable ran in6267; no solver correctness failure.'),
    limitation='Three actual fixed collapse cases establish missing retained-endpoint advice on candidate. Selected Rank32 cause still requires6264 observation.')
(c/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(summary['results'])
