#!/usr/bin/env python3
"""Link the new maintained fixture against the actual frozen old/new objects."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

c = root / 'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
target = c / 'control-6984.json'
assert not target.exists()
parent_manifest = root / 'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
m = json.loads(parent_manifest.read_text())
parent = root / m['source']
source = root / '.claude/worktrees/runtime-ite-pair-hoist-6969'
for rel, digest in m['files'].items():
    assert sha(parent / rel) == digest, rel
    if rel != 'src/rules/congruence/congruence.cpp':
        assert sha(source / rel) == digest, rel
objects = sorted((root / 'build/pin-6730-reduce-watermark-release/CMakeFiles/tactix_lib.dir').rglob('*.o'))
other = [p for p in objects if not str(p).endswith('/congruence/congruence.cpp.o')]
assert len(other) == len(objects) - 1 and other
compiled = json.loads((c / 'compile-6970.json').read_text())
for binding in compiled['objects'].values():
    assert sha(root / binding['path']) == binding['sha256'], binding
for rel, digest in compiled['hashes'].items():
    assert sha(root / rel) == digest, rel
paths = [c / 'compile-6970.json', Path(__file__), parent_manifest, source / 'tests/test_ite_pair.cpp',
         source / 'src/rules/congruence/congruence.cpp', *objects]
outputs = {}
for arm, tree in (('previous', parent), ('candidate', source)):
    obj = c / f'{arm}-congruence.o'
    assert obj.exists()
    paths.append(obj)
    outputs[arm] = {}
    for name in ('test_ite_pair', 'test_congruence', 'test_congruence_index_cleanup'):
        exe = c / f'{arm}-{name}-6984'
        assert not exe.exists()
        flags = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O3', '-DNDEBUG',
                 '-fno-exceptions', '-fno-rtti', '-I'+str(tree / 'src'), '-I'+str(tree / 'include')]
        commands = [('link', flags + [str(source / 'tests' / (name+'.cpp')), str(obj), *map(str, other), '-o', str(exe)]),
                    ('run', [str(exe)])]
        for action, command in commands:
            log = c / 'logs' / f'{arm}-{name}-{action}-6984.json'
            assert not log.exists()
            p = subprocess.run(command, capture_output=True, text=True)
            with log.open('x') as stream:
                json.dump(dict(command=command, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr), stream, indent=2)
                stream.write('\n')
            paths.append(log)
            assert p.returncode == 0, (arm, name, action, p.returncode, p.stderr[-2000:])
            if action == 'run': outputs[arm][name] = p.stdout
        paths.append(exe)
assert outputs['previous'] == outputs['candidate'], outputs
with target.open('x') as stream:
    json.dump(dict(outputs=outputs, hashes={str(p.relative_to(root)):sha(p) for p in paths},
                   scope='464 fixed public ITE cases, all sign normalizations and missing-clause subsets, ordered gate/tick digest, duplicate/reversed input, repeated extraction, exact256/over256 hub behavior and small-model entailment. Existing Congruence and1200 index-cleanup cases agree. No full builds, budget/stop controls, strongproofs, B/C, real-instance timing or fuzz acceptance yet.'), stream, indent=2)
    stream.write('\n')
print(json.dumps(outputs, indent=2))
