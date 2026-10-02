import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
build = root / sys.argv[1]
preserved = build / 'startup-support-before-default.o'
assert preserved.exists()
target = 'test_startup_support_signature'
args = shlex.split((build / f'tests/CMakeFiles/{target}.dir/link.txt').read_text())
args[args.index('-o') + 1] = target + '_before'
args[args.index('../CMakeFiles/tactix_lib.dir/src/policies/default_policy.cpp.o')] = str(preserved)

def run(command, cwd):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=120)
    if result.returncode:
        print(result.stdout, result.stderr, flush=True)
        raise RuntimeError(f'{command[0]} failed: {result.returncode}')
    return result.stdout

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

run(args, build / 'tests')
data = {'baseline_commit': '378cd919', 'replaced_translation_units': ['src/policies/default_policy.cpp'],
        'preserved_object_sha256': sha(preserved), 'baseline_source_sha256': sha(build / 'startup-support-before-default.cpp'),
        'adaptations': [], 'comparisons': [], 'oracles': [], 'feedback': []}
outputs, artifacts = [], []
spec = importlib.util.spec_from_file_location('feedback_inspector', root / 'tools/bench/inspect_policy_feedback.py')
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
sys.path.insert(0, str(root / 'tools/bench'))
sys.path.insert(0, str(root / 'tools/fuzz'))
from proof_oracle import check_routed
from modelcheck import check_files
for arm, binary in [('before', build / 'tests' / (target + '_before')),
                    ('disabled', build / 'tests' / target), ('enabled', build / 'tests' / target)]:
    directory = build / 'startup-support-signature' / arm
    directory.mkdir(parents=True, exist_ok=True)
    output = run([str(binary), str(directory)] + (['feedback'] if arm == 'enabled' else []), root)
    outputs.append(output)
    files = {p.name: sha(p) for p in directory.iterdir() if p.is_file() and not p.name.endswith('.feedback.jsonl')}
    artifacts.append(files)
    data['comparisons'].append({'arm': arm, 'binary_sha256': sha(binary), 'output': output, 'artifacts': files})
assert outputs[0] == outputs[1] == outputs[2]
assert artifacts[0] == artifacts[1] == artifacts[2]
data['signatures_equal'] = data['artifacts_byte_equal'] = True
for path in sorted((build / 'startup-support-signature/enabled').glob('*.feedback.jsonl')):
    data['feedback'].append({'case': path.stem, 'sha256': sha(path), 'summary': inspector.inspect(path)})
for cnf in sorted((build / 'startup-support-signature/enabled').glob('*.cnf')):
    model = cnf.with_suffix('.model')
    if model.exists():
        verified, message = check_files(str(cnf), str(model))
        assert verified, message
        data['oracles'].append({'case': cnf.stem, 'kind': 'SAT', 'verified': True, 'message': message})
    else:
        work = build / 'startup-support-signature/checkers' / cnf.stem
        work.mkdir(parents=True, exist_ok=True)
        checkers = root / 'tools/checkers/bin'
        verified, stage, message, route = check_routed(checkers / 'dsr-trim', checkers / 'dsr-compress', checkers / 'srcheck',
            checkers / 'drat-trim', checkers / 'cake_lpr', cnf, cnf.with_suffix('.dsr'), work, timeout=60)
        assert verified, (stage, message, route)
        data['oracles'].append({'case': cnf.stem, 'kind': 'UNSAT', 'verified': True, 'route': route, 'message': message})
(build / 'startup-support-signature.json').write_text(json.dumps(data, indent=2) + '\n')
print('3 startup fixtures: equal native signatures and 8 exact artifacts per arm; 1 checked proof, 2 checked original-CNF models, 3 inspected feedback streams', flush=True)
