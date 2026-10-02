import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = next(p for p in Path(__file__).resolve().parents if (p / 'CLAUDE.md').is_file())
build = root / sys.argv[1]
diagnostic = len(sys.argv) > 2 and sys.argv[2] == 'diagnostic'
baseline = '12e3c4f0'
overlay = build / 'broad-phase-baseline-include'
for header in ['src/policies/default_policy.hpp', 'src/policies/default_inprocess.hpp']:
    path = overlay / Path(header).relative_to('src')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:{header}'], cwd=root))
preserved = build / 'broad-phase-before-default.cpp'
preserved.write_bytes(subprocess.check_output(['git', 'show', f'{baseline}:src/policies/default_policy.cpp'], cwd=root))
obj = preserved.with_suffix('.o')
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if Path(e['file']) == root / 'src/policies/default_policy.cpp')

def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=180)
    if result.returncode:
        print(result.stdout, result.stderr, flush=True)
        raise RuntimeError(f'{args[0]} failed: {result.returncode}')
    return result.stdout

def compile(source, output, include=None):
    args = shlex.split(entry['command'])
    args[args.index('-o') + 1] = str(output)
    args[args.index('-c') + 1] = str(source)
    if include:
        args.insert(1, f'-I{include}')
    run(args, entry['directory'])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

compile(preserved, obj, overlay)
bridge = build / 'broad-phase-abi-bridge.cpp'
bridge.write_bytes((root / 'benchmarks/reports/2026-10-02-native-support-audit/debug-phase-abi-bridge.cpp').read_bytes())
bridge_obj = bridge.with_suffix('.o')
compile(bridge, bridge_obj)
target = 'test_inprocess_execution_signature'
args = shlex.split((build / f'tests/CMakeFiles/{target}.dir/link.txt').read_text())
args[args.index('-o') + 1] = target + '_before'
args[args.index('../CMakeFiles/tactix_lib.dir/src/policies/default_policy.cpp.o')] = str(obj)
args.insert(1, str(bridge_obj))
run(args, build / 'tests')
print('compiled preserved default and argument-only phase bridge', flush=True)
data = {'baseline_commit': baseline, 'diagnostic': diagnostic,
        'baseline_source_sha256': sha(preserved),
        'baseline_header_sha256': {str(p.relative_to(overlay)): sha(p) for p in overlay.rglob('*.hpp')},
        'bridge_sha256': sha(bridge),
        'limitations': ['Preserved solver body/headers are unadapted; argument-only phase bridge drops optional feedback.',
                        'Current shared dependencies/harness; no complete old-binary or performance claim.'],
        'comparisons': [], 'feedback': [], 'oracles': [], 'productive_controls': []}
outputs, artifacts = [], []
for arm, binary in [('before', build / 'tests' / (target + '_before')),
                    ('disabled', build / 'tests' / target), ('enabled', build / 'tests' / target)]:
    directory = build / 'broad-phase-correspondence' / arm
    directory.mkdir(parents=True, exist_ok=True)
    mode = ('diagnostics-feedback' if arm == 'enabled' else 'diagnostics') if diagnostic else ('feedback' if arm == 'enabled' else '')
    output = run([str(binary), str(directory)] + ([mode] if mode else []), root)
    outputs.append(output)
    files = {p.name: sha(p) for p in directory.iterdir() if p.is_file() and not p.name.endswith('.feedback.jsonl')}
    artifacts.append(files)
    data['comparisons'].append({'arm': arm, 'binary_sha256': sha(binary), 'output': output,
                                'output_sha256': hashlib.sha256(output.encode()).hexdigest(), 'artifacts': files})
assert outputs[0] == outputs[1] == outputs[2], 'native phase/terminal signature mismatch'
assert artifacts[0] == artifacts[1] == artifacts[2], 'certificate/model/journal/event mismatch'
data['signatures_equal'] = data['artifacts_byte_equal'] = True
spec = importlib.util.spec_from_file_location('feedback_inspector', root / 'tools/bench/inspect_policy_feedback.py')
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)
families = ['Vivify', 'Congruence', 'Backbone', 'Sweep', 'Substitute', 'BVE']
metrics = [['vivify_subsumed', 'vivify_shrunken'], ['congruence_merges', 'congruence_units'],
           ['backbone_failed'], ['sweep_units', 'sweep_equivalences'], ['substitute_vars'], ['bve_eliminated']]
for path in sorted((build / 'broad-phase-correspondence/enabled').glob('*.feedback.jsonl')):
    data['feedback'].append({'file': path.name, 'sha256': sha(path), 'summary': inspector.inspect(path)})
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    records = [row for row in rows if row['type'] == 'action']
    for row in records:
        if row['kind'] == 3:
            assert not row['outcome']['applications'] and not row['outcome']['progress']
            if row['outcome']['unit'] == 0:
                assert not row['outcome']['work'] and not row['outcome']['composition_overshoot']
    if '.phase.' in path.name:
        family = int(path.name.split('-')[1])
        selected = [row for row in records if row['identity'] == families[family]]
        effective = [row for row in selected if
            sum(row['after']['native'][key] for key in metrics[family]) >
            sum(row['before']['native'][key] for key in metrics[family])]
        assert effective, (path.name, 'missing effective target-family receipt')
        sessions = [row for row in records if row['identity'] == 'BeginBveDefinitionSession']
        assert len(sessions) == (1 if family == 5 else 0)
        data['productive_controls'].append({'file': path.name, 'family': families[family],
            'target_calls': len(selected), 'effective_receipts': len(effective),
            'native_work': sum(row['native_request']['work'] for row in selected),
            'definition_sessions': len(sessions)})
sys.path.insert(0, str(root / 'tools/fuzz'))
sys.path.insert(0, str(root / 'tools/bench'))
from modelcheck import check_files
from proof_oracle import check_routed
for cnf in sorted((build / 'broad-phase-correspondence/enabled').glob('*.cnf')):
    model = cnf.with_suffix('.model')
    if model.exists():
        verified, message = check_files(str(cnf), str(model))
        assert verified, message
        data['oracles'].append({'case': cnf.stem, 'kind': 'SAT', 'verified': True, 'message': message})
    else:
        work = build / 'broad-phase-correspondence/checkers' / cnf.stem
        work.mkdir(parents=True, exist_ok=True)
        checkers = root / 'tools/checkers/bin'
        verified, stage, message, route = check_routed(checkers / 'dsr-trim', checkers / 'dsr-compress',
            checkers / 'srcheck', checkers / 'drat-trim', checkers / 'cake_lpr',
            cnf, cnf.with_suffix('.dsr'), work, timeout=60)
        assert verified, (stage, message, route)
        data['oracles'].append({'case': cnf.stem, 'kind': 'UNSAT', 'verified': True, 'route': route, 'message': message})
(build / 'broad-phase-correspondence.json').write_text(json.dumps(data, indent=2) + '\n')
print(f'12 productive phase/complete-solve fixtures: equal signatures and {len(artifacts[0])} exact artifacts per arm; 6 checked proofs + 6 checked models; 24 inspected feedback streams', flush=True)
