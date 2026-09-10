#!/usr/bin/env python3
"""Layout-safe control: every object/header belongs to its own complete arm."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6869-subsolver-binary-build'
target = c/'control-6874.json'
assert not target.exists()
m = json.loads((c/'manifest.json').read_text())
for rel,digest in m['binaries'].items(): assert sha(root/rel) == digest
hashes = {str((c/'manifest.json').relative_to(root)):sha(c/'manifest.json')}
outputs = {}
for arm, tree_name, pin_name in (
    ('previous', 'reduce-watermark-6730', '6730-reduce-watermark'),
    ('candidate', 'subsolver-binary-6869', '6869-subsolver-binary'),
):
    tree = root/('.claude/worktrees/runtime-'+tree_name)
    pin = root/('build/pin-'+pin_name+'-release')
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    assert objects and all(pin in p.parents for p in objects)
    for p in objects: hashes[str(p.relative_to(root))] = sha(p)
    flags = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O3', '-DNDEBUG', '-fno-exceptions', '-fno-rtti', '-I'+str(tree/'src'), '-I'+str(tree/'include')]
    outputs[arm] = {}
    def run(name, command):
        r = subprocess.run(command, cwd=c, text=True, capture_output=True)
        p = c/'logs'/('own-arm-'+arm+'-'+name+'.json')
        with p.open('x') as stream:
            json.dump(dict(command=command, exit_code=r.returncode, stdout=r.stdout, stderr=r.stderr), stream, indent=2); stream.write('\n')
        hashes[str(p.relative_to(root))] = sha(p)
        assert r.returncode == 0, (p, r.stderr[-2000:])
        return r.stdout
    fixture = root/m['source']/'tests/test_subsolver_binary.cpp'
    exe = c/('own-arm-'+arm+'-test_subsolver_binary')
    assert not exe.exists()
    hashes[str(fixture.relative_to(root))] = sha(fixture)
    run('link', flags+[str(fixture), *map(str,objects), '-o', str(exe)])
    outputs[arm]['test_subsolver_binary'] = run('fixture', [str(exe)])
    for name in ('test_subsolver','test_sweep'):
        exe = pin/'tests'/name
        hashes[str(exe.relative_to(root))] = sha(exe)
        outputs[arm][name] = run(name, [str(exe)])
assert outputs['previous'] == outputs['candidate'], outputs
hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, outputs=outputs,
        conclusion='Layout-safe complete own-arm objects/headers match fixed binary-watch controls and existing SubSolver/Sweep suites. Initial mixed-header6870 diagnostic is superseded for correctness acceptance, not rewritten. External proofs, B/C, runtime, memory and current-source fuzz remain separate.'), stream, indent=2); stream.write('\n')
print(json.dumps(outputs, indent=2))
