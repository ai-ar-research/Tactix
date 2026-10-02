import hashlib
import json
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path('/home/hwu/SAT-solving')
build = root / 'build/shared-policy-prep-diagnostic'
baseline = '352932d5'
source = 'src/policies/random_controller_policy.cpp'
headers = ['src/policies/random_controller_policy.hpp', 'src/policies/restart_controller.hpp']
overlay = root / 'build/before-feedback-diagnostic-include'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=120)
    if result.returncode:
        print(result.stdout, result.stderr, flush=True)
        raise RuntimeError(f'{args[0]} failed with {result.returncode}')
    return result.stdout


data = {'baseline_commit': baseline, 'preserved_headers': {}, 'before_sources': {},
        'comparisons': [], 'oracles': [], 'feedback_inspection': []}
for header in headers:
    target = overlay / pathlib.Path(header).relative_to('src')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:{header}'], cwd=root))
    data['preserved_headers'][header] = sha(target)
preserved = root / 'build/before-feedback-diagnostic-random_controller_policy.cpp'
preserved.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:{source}'], cwd=root))
data['before_sources'][source] = sha(preserved)
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if pathlib.Path(e['file']) == root / source)
args = shlex.split(entry['command'])
obj = preserved.with_suffix('.o')
args[args.index('-o') + 1] = str(obj)
args[args.index('-c') + 1] = str(preserved)
args.insert(1, f'-I{overlay}')
run(args, entry['directory'])
print('compiled preserved random controller and headers', flush=True)
target = 'test_controller_execution_signature'
args = shlex.split((build / f'tests/CMakeFiles/{target}.dir/link.txt').read_text())
args[args.index('-o') + 1] = target + '_before_feedback'
args[args.index('../CMakeFiles/tactix_lib.dir/' + source + '.o')] = str(obj)
run(args, build / 'tests')
print('linked preserved controller signature fixture', flush=True)

sys.path[:0] = [str(root / 'tools/bench'), str(root / 'tools/fuzz')]
from proof_oracle import check_routed
from modelcheck import check_files
from inspect_policy_feedback import inspect

before = build / 'tests' / (target + '_before_feedback')
after = build / 'tests' / target
dirs = [build / 'feedback-diagnostic-correspondence' / suffix for suffix in ['before', 'after', 'observed']]
outputs = []
for binary, directory, extra in zip([before, after, after], dirs, [[], [], ['feedback']]):
    directory.mkdir(parents=True, exist_ok=True)
    outputs.append(run([str(binary), str(directory)] + extra, build / 'tests'))
assert outputs[0] == outputs[1] == outputs[2], 'native output or requests changed'
files = sorted(p.name for p in dirs[0].iterdir() if p.is_file() and p.suffix != '.trace')
for directory in dirs[1:]:
    assert files == sorted(p.name for p in directory.iterdir()
                           if p.is_file() and p.suffix not in ('.trace', '.jsonl'))
    for name in files:
        assert (dirs[0] / name).read_bytes() == (directory / name).read_bytes(), name
    data['comparisons'].append({'directory': str(directory.relative_to(root)),
        'signatures_equal': True, 'files_byte_equal': True,
        'files': {name: sha(directory / name) for name in files}})
data['output'] = outputs[1]
data['output_sha256'] = hashlib.sha256(outputs[1].encode()).hexdigest()
data['binaries'] = {str(p.relative_to(root)): sha(p) for p in [before, after]}
for cnf in sorted(dirs[1].glob('*.cnf')):
    model = cnf.with_suffix('.model')
    if model.exists():
        ok, message = check_files(str(cnf), str(model))
        assert ok, message
        data['oracles'].append({'case': cnf.stem, 'kind': 'SAT', 'verified': True, 'message': message})
    else:
        work = build / 'feedback-diagnostic-correspondence/checkers' / cnf.stem
        work.mkdir(parents=True, exist_ok=True)
        verified, stage, message, route = check_routed(None, None, None,
            root / 'tools/checkers/bin/drat-trim', root / 'tools/checkers/bin/cake_lpr',
            cnf, cnf.with_suffix('.drat'), work, timeout=60)
        assert verified, (stage, message, route)
        data['oracles'].append({'case': cnf.stem, 'kind': 'UNSAT', 'verified': True,
                               'route': route, 'message': message})
for path in sorted(dirs[2].glob('*.feedback.jsonl')):
    summary = inspect(path)
    data['feedback_inspection'].append({'path': str(path.relative_to(root)), 'sha256': sha(path),
                                        'summary': summary})
for unchanged in ['src/policies/default_policy.cpp', 'src/policies/default_policy.hpp']:
    original = subprocess.check_output(['git', 'show', f'{baseline}:{unchanged}'], cwd=root)
    assert (root / unchanged).read_bytes() == original
    data.setdefault('unchanged_default_sources', {})[unchanged] = sha(root / unchanged)
(root / 'build/shared-policy-prep-feedback-diagnostic-correspondence.json').write_text(json.dumps(data, indent=2) + '\n')
print(f'16 controller cases: signatures and {len(files)} artifacts identical in preserved/current/observed; '
      f'{len(data["oracles"])} independent oracles and {len(data["feedback_inspection"])} offline inspections passed',
      flush=True)
