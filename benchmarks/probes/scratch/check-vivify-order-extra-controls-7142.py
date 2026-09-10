#!/usr/bin/env python3
"""Run a new fixed API fixture against immutable existing object inventories."""
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7142-vivify-order-extra-controls'
assert not c.exists()
source = root / 'tests/test_vivify_order_contract.cpp'
hashes = {str(source.relative_to(root)): sha(source), str(Path(__file__).relative_to(root)): sha(Path(__file__))}
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def run(command, cwd, label):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
    p = c / (label + '.log')
    with p.open('x') as stream: stream.write(result.stdout + result.stderr)
    bind(p)
    assert result.returncode == 0, (label, result.returncode, result.stdout, result.stderr)
    return dict(command=command, cwd=str(cwd), stdout=result.stdout, log=str(p.relative_to(root)))
c.mkdir()
arms = {}
for stem, build, baseline_name in (
    ('main-release', root / 'build/release', 'test_vivify'),
    ('7116-sanitizers', root / 'build/pin-7116-bve-import-bounds-sanitizers', 'test_bve')):
    if stem == 'main-release':
        assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root), 'production source changed'
        bind(build / 'tactix')
    else:
        p = root / 'benchmarks/runs/20260910-7118-bve-import-bounds-sanitizers/summary.json'
        bind(p); prior = json.loads(p.read_text())
        for rel, digest in prior['hashes'].items(): bind(root / rel, digest)
    p = build / 'compile_commands.json'; bind(p)
    records = [item for item in json.loads(p.read_text()) if item['file'].endswith('/' + baseline_name + '.cpp')]
    assert len(records) == 1
    record = records[0]; compile_command = shlex.split(record['command'])
    assert '-c' in compile_command and '-o' in compile_command
    old_source = Path(record['file']); bind(old_source)
    for header in (old_source.parent.parent / 'src/kernel/kernel.hpp', old_source.parent.parent / 'src/rules/vivify/vivify.hpp'):
        bind(header)
    original_object = (Path(record['directory']) / compile_command[compile_command.index('-o') + 1]).resolve()
    bind(original_object)
    p = build / ('tests/CMakeFiles/' + baseline_name + '.dir/link.txt'); bind(p)
    original_link = shlex.split(p.read_text()); assert original_link[-2:] == ['-o', baseline_name]
    old_objects = []
    for i, token in enumerate(original_link):
        if token.endswith('.o'):
            obj = (build / 'tests' / token).resolve(); bind(obj)
            original_link[i] = str(obj); old_objects.append(obj)
    assert original_object in old_objects
    # Reproduce an existing executable byte-for-byte before trusting its
    # object inventory for the new fixture. No pin file is written or relinked.
    reproduced = c / (stem + '-existing-control')
    original_link[-1] = str(reproduced)
    control = run(original_link, root, stem + '-relink-control')
    assert sha(reproduced) == sha(build / ('tests/' + baseline_name)), 'object inventory does not reproduce frozen executable'
    bind(build / ('tests/' + baseline_name)); bind(reproduced)
    obj = c / (stem + '.o')
    compile_command[compile_command.index('-o') + 1] = str(obj)
    assert compile_command.count(str(old_source)) == 1
    compile_command[compile_command.index(str(old_source))] = str(source)
    compiled = run(compile_command, Path(record['directory']), stem + '-compile')
    bind(obj)
    executable = c / (stem + '-fixture')
    link = list(original_link); link[link.index(str(original_object))] = str(obj)
    link[-1] = str(executable)
    linked = run(link, root, stem + '-link'); bind(executable)
    outcomes = [run([str(executable)], c, stem + f'-run{i}') for i in range(2)]
    assert outcomes[0]['stdout'] == outcomes[1]['stdout']
    assert re.fullmatch(r'vivify-order-contract cases=96 digest=[0-9a-f]{16}\n', outcomes[0]['stdout'])
    arms[stem] = dict(control=control, compiled=compiled, linked=linked, outcomes=outcomes,
        binary_sha256=sha(executable), exact_existing_control=True)
    print(stem, outcomes[0]['stdout'].strip(), flush=True)
prior_path = root / 'benchmarks/runs/20260910-7141-vivify-order-contract/summary.json'
bind(prior_path)
prior = json.loads(prior_path.read_text())
assert prior['status'] == 'PASS'
expected = next(iter(prior['arms'].values()))['outcomes'][0]['stdout']
assert all(arm['outcomes'][0]['stdout'] == expected for arm in arms.values())
for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
with (c / 'summary.json').open('x') as stream:
    json.dump(dict(status='PASS', hashes=hashes, arms=arms, cases=96,
        limitation='Same fixed96-case scheduler/API/state/model fixture on production-source main Release and fully instrumented7116 Debug ASan/UBSan with normal leak detection outside ptrace. Exact7141 digest required. No solver source change, new performance experiment, proof-emitting transformation or current-source fuzz claim. Ordinary and skip-sort threshold sides, priorities, reversed import/literal order, appended logical-position replacements, canonical exhausted entry, collection and bounded N1 instance surfaces are exercised. Pending structural pilot remains unapproved.'), stream, indent=2)
