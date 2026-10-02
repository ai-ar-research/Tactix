import hashlib
import importlib.util
import json
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
build = root / sys.argv[1]
diagnostic = len(sys.argv) > 2 and sys.argv[2] == 'diagnostic'
baseline = 'a4cfabe3'
overlay = build / 'frontend-feedback-baseline-include'
for header in ['src/policies/default_policy.hpp', 'src/policies/default_inprocess.hpp']:
    path = overlay / pathlib.Path(header).relative_to('src')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:{header}'], cwd=root))
source = root / 'src/policies/default_policy.cpp'
preserved = build / 'frontend-feedback-before-default.cpp'
preserved.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:src/policies/default_policy.cpp'], cwd=root))
obj = preserved.with_suffix('.o')
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if pathlib.Path(e['file']) == source)
args = shlex.split(entry['command'])
args[args.index('-o') + 1] = str(obj)
args[args.index('-c') + 1] = str(preserved)
args.insert(1, f'-I{overlay}')

def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        print(result.stdout, result.stderr, flush=True)
        raise RuntimeError(f'{args[0]} failed: {result.returncode}')
    return result.stdout

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

run(args, entry['directory'])
print('compiled preserved default', flush=True)
target = 'test_default_execution_signature'
args = shlex.split((build / f'tests/CMakeFiles/{target}.dir/link.txt').read_text())
args[args.index('-o') + 1] = target + '_before_frontend_feedback'
args[args.index('../CMakeFiles/tactix_lib.dir/src/policies/default_policy.cpp.o')] = str(obj)
run(args, build / 'tests')
before = build / 'tests' / (target + '_before_frontend_feedback')
after = build / 'tests' / target
data = {'baseline_commit': baseline, 'replaced_translation_units': ['src/policies/default_policy.cpp'],
        'baseline_source_sha256': sha(preserved), 'diagnostic': diagnostic,
        'header_sha256': {str(p.relative_to(overlay)): sha(p) for p in overlay.rglob('*.hpp')},
        'adaptations': ['None to preserved source; DefaultPolicy and persistent scheduler layouts are unchanged from a4cfabe3.'],
        'comparisons': [], 'oracles': [], 'feedback': []}
spec = importlib.util.spec_from_file_location('feedback_inspector', root / 'tools/bench/inspect_policy_feedback.py')
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
sys.path.insert(0, str(root / 'tools/fuzz'))
sys.path.insert(0, str(root / 'tools/bench'))
from proof_oracle import check_routed
from modelcheck import check_files
outputs = []
artifacts = []
for arm, binary in [('before', before), ('disabled', after), ('enabled', after)]:
    directory = build / 'frontend-feedback-correspondence' / arm
    directory.mkdir(parents=True, exist_ok=True)
    mode = ('diagnostics-feedback' if arm == 'enabled' else 'diagnostics') if diagnostic else ('feedback' if arm == 'enabled' else '')
    output = run([str(binary), str(directory)] + ([mode] if mode else []), build / 'tests')
    outputs.append(output)
    files = {p.name: sha(p) for p in directory.iterdir() if p.is_file() and not p.name.endswith('.feedback.jsonl')}
    artifacts.append(files)
    data['comparisons'].append({'arm': arm, 'output': output,
        'output_sha256': hashlib.sha256(output.encode()).hexdigest(), 'binary_sha256': sha(binary),
        'artifacts': files})
assert outputs[0] == outputs[1] == outputs[2]
assert artifacts[0] == artifacts[1] == artifacts[2]
data['signatures_equal'] = data['artifacts_byte_equal'] = True
for path in sorted((build / 'frontend-feedback-correspondence/enabled').glob('*.feedback.jsonl')):
    summary = inspector.inspect(path)
    data['feedback'].append({'case': path.stem, 'sha256': sha(path), 'summary': summary})
for cnf in sorted((build / 'frontend-feedback-correspondence/enabled').glob('*.cnf')):
    model = cnf.with_suffix('.model')
    if model.exists():
        ok, message = check_files(str(cnf), str(model))
        assert ok, message
        data['oracles'].append({'case': cnf.stem, 'kind': 'SAT', 'verified': True, 'message': message})
    else:
        work = build / 'frontend-feedback-correspondence/checkers' / cnf.stem
        work.mkdir(parents=True, exist_ok=True)
        verified, stage, message, route = check_routed(None, None, None, root / 'tools/checkers/bin/drat-trim',
            root / 'tools/checkers/bin/cake_lpr', cnf, cnf.with_suffix('.drat'), work, timeout=60)
        assert verified, (stage, message, route)
        data['oracles'].append({'case': cnf.stem, 'kind': 'UNSAT', 'verified': True, 'route': route, 'message': message})
output = build / 'frontend-feedback-correspondence.json'
output.write_text(json.dumps(data, indent=2) + '\n')
print(f'10 cases: 3 equal signatures, {len(artifacts[0])} byte-equal artifacts per arm, 5 checked proofs + 5 checked models, 10 inspected feedback streams', flush=True)
