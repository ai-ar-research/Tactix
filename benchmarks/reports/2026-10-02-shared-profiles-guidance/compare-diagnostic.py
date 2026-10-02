import hashlib
import json
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path('/home/hwu/SAT-solving')
build = root / 'build/shared-policy-prep-diagnostic'
baseline = '5da1a178'
sources = ['src/policies/default_policy.cpp', 'src/strategy/online_inprocess.cpp', 'src/strategy/analyze_parameters.cpp', 'src/strategy/congruence_parameters.cpp', 'src/strategy/forward_parameters.cpp', 'src/strategy/failed_literal_parameters.cpp', 'src/strategy/instantiate_parameters.cpp', 'src/strategy/vivify_parameters.cpp', 'src/strategy/rule_parameters.cpp']
headers = ['src/policies/default_inprocess.hpp', 'src/strategy/analyze_parameters.hpp',
           'src/strategy/congruence_parameters.hpp', 'src/strategy/forward_parameters.hpp',
           'src/strategy/failed_literal_parameters.hpp', 'src/strategy/instantiate_parameters.hpp',
           'src/strategy/vivify_parameters.hpp', 'src/strategy/rule_parameters.hpp',
           'src/strategy/bve_invocation.hpp', 'src/strategy/parameter_execution.hpp']
overlay = root / 'build/before-profile-diagnostic-include'
header_hashes = {}
for header in headers:
    target_header = overlay / pathlib.Path(header).relative_to('src')
    target_header.parent.mkdir(parents=True, exist_ok=True)
    target_header.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:{header}'], cwd=root))
    header_hashes[header] = hashlib.sha256(target_header.read_bytes()).hexdigest()
db = json.loads((build / 'compile_commands.json').read_text())
data = {'baseline_commit': baseline, 'replaced_translation_units': sources,
        'preserved_headers': header_hashes,
        'adaptations': [], 'before_sources': {}, 'comparisons': [], 'oracles': []}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        print(result.stdout, result.stderr, flush=True)
        raise RuntimeError(f'{args[0]} failed with {result.returncode}')
    return result.stdout

objects = {}
for source in sources:
    preserved = root / f'build/before-profile-diagnostic-{pathlib.Path(source).name}'
    preserved.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:{source}'], cwd=root))
    data['before_sources'][source] = sha(preserved)
    entry = next(e for e in db if pathlib.Path(e['file']) == root / source)
    args = shlex.split(entry['command'])
    obj = preserved.with_suffix('.o')
    args[args.index('-o') + 1] = str(obj)
    args[args.index('-c') + 1] = str(preserved)
    args.insert(1, f'-I{overlay}')
    if '--compare-only' not in sys.argv and '--link-only' not in sys.argv:
        run(args, entry['directory'])
    objects[source] = obj
    print(f'compiled preserved {source}', flush=True)

targets = ['test_default_execution_signature']
for target in targets:
    args = shlex.split((build / f'tests/CMakeFiles/{target}.dir/link.txt').read_text())
    args[args.index('-o') + 1] = target + '_before_profile_diagnostic'
    for source, obj in objects.items():
        original = '../CMakeFiles/tactix_lib.dir/' + source + '.o'
        args[args.index(original)] = str(obj)
    if '--compare-only' not in sys.argv:
        run(args, build / 'tests')
    print(f'linked preserved {target}', flush=True)

if '--compile-only' in sys.argv:
    (root / 'build/shared-policy-prep-guidance-diagnostic-baseline.json').write_text(json.dumps(data, indent=2) + '\n')
    sys.exit(0)

sys.path[:0] = [str(root / 'tools/bench'), str(root / 'tools/fuzz')]
from proof_oracle import check_routed
from modelcheck import check_files

for target in targets:
    name = target.removeprefix('test_')
    before = build / 'tests' / (target + '_before_profile_diagnostic')
    after = build / 'tests' / target
    dirs = [build / 'guidance-diagnostic-correspondence' / (name + '-' + suffix)
            for suffix in ['before', 'after']]
    artifact_args = name.endswith('execution_signature')
    outputs = []
    for binary, directory in zip([before, after], dirs):
        directory.mkdir(parents=True, exist_ok=True)
        outputs.append(run([str(binary)] + ([str(directory), 'diagnostics'] if artifact_args else []), build / 'tests'))
    assert outputs[0] == outputs[1], target
    files = sorted(p.name for p in dirs[0].iterdir() if p.is_file() and p.suffix != '.trace')
    assert files == sorted(p.name for p in dirs[1].iterdir() if p.is_file() and p.suffix != '.trace')
    hashes = {}
    for name in files:
        assert (dirs[0] / name).read_bytes() == (dirs[1] / name).read_bytes(), name
        hashes[name] = sha(dirs[1] / name)
    data['comparisons'].append({'target': target, 'signatures_equal': True,
        'files_byte_equal': True, 'files': hashes, 'output_sha256': hashlib.sha256(outputs[1].encode()).hexdigest(),
        'output': outputs[1], 'binaries': {str(p.relative_to(root)): sha(p) for p in [before, after]}})
    for cnf in sorted(dirs[1].glob('*.cnf')):
        model = cnf.with_suffix('.model')
        if model.exists():
            ok, message = check_files(str(cnf), str(model))
            assert ok, message
            data['oracles'].append({'case': cnf.stem, 'kind': 'SAT', 'verified': True, 'message': message})
        else:
            work = build / 'guidance-diagnostic-correspondence/checkers' / target / cnf.stem
            work.mkdir(parents=True, exist_ok=True)
            verified, stage, message, route = check_routed(None, None, None, root / 'tools/checkers/bin/drat-trim',
                root / 'tools/checkers/bin/cake_lpr', cnf, cnf.with_suffix('.drat'), work, timeout=60)
            assert verified, (stage, message, route)
            data['oracles'].append({'case': cnf.stem, 'kind': 'UNSAT', 'verified': True,
                'route': route, 'message': message})
    print(f'{target}: signatures and {len(files)} artifacts identical; independent oracles passed', flush=True)

(root / 'build/shared-policy-prep-guidance-diagnostic-correspondence.json').write_text(json.dumps(data, indent=2) + '\n')
