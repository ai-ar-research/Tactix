#!/usr/bin/env python3
"""Compare the same full BVE/boundary fixtures on frozen7110 and7116."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
out = root / 'benchmarks/runs/20260910-7121-bve-import-bounds-fixtures'
assert not out.exists()
out.mkdir()
hashes, checks, outputs = {}, [], {}
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
fixture_root = root / '.claude/worktrees/runtime-bve-import-bounds-7116'
for stem in ('7110-bve-import', '7116-bve-import-bounds'):
    authority = root / f'benchmarks/runs/20260910-{stem}-build/manifest.json'
    m = json.loads(authority.read_text()); bind(authority)
    source = root / m['source']
    for rel, digest in m['files'].items(): assert sha(source / rel) == digest, rel
    objects = sorted((root / f'build/pin-{stem}-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
    assert objects
    for obj in objects: bind(obj)
    outputs[stem] = []
    for name in ('test_bve', 'test_bve_antecedent_view'):
        fixture = fixture_root / 'tests' / (name + '.cpp'); bind(fixture)
        exe = out / (stem + '-' + name)
        commands = [['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2',
            '-fno-exceptions', '-fno-rtti', '-I' + str(source / 'src'), '-I' + str(source / 'include'),
            fixture, *objects, '-o', exe], [exe]]
        for index, command in enumerate(commands):
            command = list(map(str, command))
            p = subprocess.run(command, cwd=out, capture_output=True, text=True)
            log = out / (stem + '-' + name + f'-{index}.json')
            with log.open('x') as stream:
                json.dump(dict(command=command, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr), stream, indent=2)
            bind(log)
            assert p.returncode == 0, (log, p.stderr)
            checks.append(str(log.relative_to(root)))
            if index: outputs[stem].append(p.stdout)
        bind(exe)
assert outputs['7110-bve-import'] == outputs['7116-bve-import-bounds']
assert '496 reconstructed fixed cases OK' in outputs['7116-bve-import-bounds'][0]
bind(Path(__file__))
with (out / 'summary.json').open('x') as stream:
    json.dump(dict(status='PASS', hashes=hashes, checks=checks, outputs=outputs,
        scope='Identical full BVE and corrected view fixtures, including496 XOR arity2..6 cases, dense/sparse, reversed literal order and every input assignment. Exact stdout on7110/7116; no scheduling/semantic change claimed from warnings.'), stream, indent=2)
print('PASS full BVE/view controls both arms;496 XOR boundary/model cases per arm')
