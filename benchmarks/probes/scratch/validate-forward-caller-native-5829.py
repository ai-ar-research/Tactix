#!/usr/bin/env python3
"""Actual native control for the fixed post-forward scheduler debt witness."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'benchmarks/runs/20260908-5829-forward-caller-native'
out.mkdir(exist_ok=False)
source = root / 'tools/solvers/variants/kissat-score-bump-observer-5696'
fixture = root / 'benchmarks/probes/scratch/forward-caller-native-5828.c'
library = source / 'build/libkissat.a'
commands = []
for name, command in [('compile', ['gcc', '-O3', '-DNDEBUG', '-I' + str(source / 'src'), fixture,
                                   library, '-lm', '-o', out / 'witness']), ('witness', [out / 'witness'])]:
    commands.append(list(map(str, command)))
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    (out / (name + '.log')).write_text(r.stdout + r.stderr)
    assert r.returncode == 0, (name, r.returncode, r.stdout, r.stderr)
    print(name, r.stdout or 'PASS', flush=True)
paths = [fixture, library, out / 'witness', source / 'src/forward.c', source / 'src/eliminate.c']
(out / 'summary.json').write_text(json.dumps(dict(commands=commands,
    hashes={str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
    result='Native partial forward quota cut leaves unit5 pending;actual caller reconnect/reset/flush derives it before BVE scheduling.',
    paired_tactix='benchmarks/runs/20260908-5826-forward-caller-debt/summary.json',
    limitation='Fixed actual-code control,no final native proof or runtime claim.'), indent=2) + '\n')
