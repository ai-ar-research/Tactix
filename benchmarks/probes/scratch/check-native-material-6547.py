#!/usr/bin/env python3
"""Build/run a fixed native contract test without touching native build pins."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6547-native-material-control'
assert not c.exists()
c.mkdir()
native = root / 'tools/solvers/variants/kissat-macro-heap-6090'
fixture = root / 'benchmarks/probes/scratch/test-native-material-6546.c'
inputs = [fixture, native / 'build/libkissat.a']
inputs += sorted((native / 'src').glob('*.h'))
hashes = {str(p.relative_to(root)): sha(p) for p in inputs}
checks = {}
for name, extra in (('optimized', []), ('ubsan', ['-fsanitize=undefined', '-fno-sanitize-recover=all'])):
    binary = c / name
    command = ['gcc', '-O2', *extra, '-I' + str(native / 'src'), '-I' + str(native / 'build'),
               str(fixture), str(native / 'build/libkissat.a'), '-lm', '-o', str(binary)]
    build = subprocess.run(command, cwd=root, capture_output=True, text=True)
    (c / (name + '-build.log')).write_text(build.stdout + build.stderr)
    assert build.returncode == 0, build.stderr
    run = subprocess.run([str(binary)], cwd=root, capture_output=True, text=True)
    assert run.returncode == 0 and not run.stderr, (run.returncode, run.stderr)
    assert run.stdout == 'PASS native material producer and admission: 14 checks; consumer reset/rearm are explicit fixture setup\n'
    checks[name] = dict(build_command=command, stdout=run.stdout, binary_sha256=sha(binary))
    print(name, run.stdout.strip(), flush=True)
assert hashes == {rel: sha(root / rel) for rel in hashes}
with (c / 'summary.json').open('x') as out:
    json.dump(dict(hashes=hashes, checks=checks,
        limitation='Actual native inline added-material producer and linked admission guard. Consumer reset/rearm flags are test setup, not execution of forward. UBSan covers fixture and inline producer, not the prebuilt native library. No Tactix implementation, solver run, fuzz, certificate or runtime evidence.'), out, indent=2)
    out.write('\n')
