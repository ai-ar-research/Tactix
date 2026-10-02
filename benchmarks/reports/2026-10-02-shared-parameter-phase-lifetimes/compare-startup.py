import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = next(parent for parent in Path(__file__).resolve().parents if (parent / 'CLAUDE.md').is_file())
build = root / sys.argv[1]
preserved = build / 'parameter-phase-before-default.o'
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
data = {'baseline_commit': '3327b185', 'replaced_translation_units': ['src/policies/default_policy.cpp'],
        'preserved_object_sha256': sha(preserved), 'baseline_source_sha256': sha(build / 'parameter-phase-before-default.cpp'),
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
    directory = build / 'parameter-phase-signature' / arm
    directory.mkdir(parents=True, exist_ok=True)
    output = run([str(binary), str(directory)] + (['feedback'] if arm == 'enabled' else []), root)
    outputs.append(output)
    files = {p.name: sha(p) for p in directory.iterdir() if p.is_file() and not p.name.endswith('.feedback.jsonl')}
    artifacts.append(files)
    data['comparisons'].append({'arm': arm, 'binary_sha256': sha(binary), 'output': output, 'artifacts': files})
assert outputs[0] == outputs[1] == outputs[2]
assert artifacts[0] == artifacts[1] == artifacts[2]
data['signatures_equal'] = data['artifacts_byte_equal'] = True
for path in sorted((build / 'parameter-phase-signature/enabled').glob('*.feedback.jsonl')):
    data['feedback'].append({'case': path.stem, 'sha256': sha(path), 'summary': inspector.inspect(path)})
support = ['PrepareResidualGuidance', 'AttachHeuristics', 'SeedResidualImportOrder']
reach = {}
for scenario in range(5):
    path = build / f'parameter-phase-signature/enabled/scenario-{scenario}.feedback.jsonl'
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    records = [row for row in rows if row['type'] == 'action' and row['identity'] in support]
    identities = [row['identity'] for row in records]
    if scenario == 3:
        assert identities == support
        assert records[0]['native_request']['currency'] == 'activation-variables'
        assert records[0]['native_request']['work'] == 21
        assert records[1]['native_request']['description'] == 'fresh;activation-storage=shrink'
        assert records[2]['native_request']['description'] == 'variables=21'
    if scenario in (1, 4):
        assert not records
    for record in records:
        assert record['outcome']['applications'] == 0 and not record['outcome']['progress']
    reach[str(scenario)] = [
        {'identity': row['identity'], 'work': row['native_request']['work'],
         'currency': row['native_request']['currency']} for row in records]
path = build / 'parameter-phase-signature/enabled/scenario-5.feedback.jsonl'
rows = [json.loads(line) for line in path.read_text().splitlines()]
phase = [r for r in rows if r['type'] == 'action' and r['identity'] in ('InprocessPhaseEntry', 'InprocessPhaseExit')]
assert [r['identity'] for r in phase] == ['InprocessPhaseEntry', 'InprocessPhaseExit']
assert phase[1]['native_request']['description'] == 'structured-terminal=sat;no-occurrence-session'
assert all(r['outcome']['applications'] == 0 and not r['outcome']['progress'] for r in phase)
data['phase_terminal_control'] = {'entry_exit': [r['identity'] for r in phase], 'verdict': 10, 'no_occurrence_session': True}
data['support_reach_controls'] = reach
for cnf in sorted((build / 'parameter-phase-signature/enabled').glob('*.cnf')):
    model = cnf.with_suffix('.model')
    if model.exists():
        verified, message = check_files(str(cnf), str(model))
        assert verified, message
        data['oracles'].append({'case': cnf.stem, 'kind': 'SAT', 'verified': True, 'message': message})
    else:
        work = build / 'parameter-phase-signature/checkers' / cnf.stem
        work.mkdir(parents=True, exist_ok=True)
        checkers = root / 'tools/checkers/bin'
        verified, stage, message, route = check_routed(checkers / 'dsr-trim', checkers / 'dsr-compress', checkers / 'srcheck',
            checkers / 'drat-trim', checkers / 'cake_lpr', cnf, cnf.with_suffix('.dsr'), work, timeout=60)
        assert verified, (stage, message, route)
        data['oracles'].append({'case': cnf.stem, 'kind': 'UNSAT', 'verified': True, 'route': route, 'message': message})
(build / 'parameter-phase-signature.json').write_text(json.dumps(data, indent=2) + '\n')
print(f'6 startup/residual fixtures: equal native signatures and {len(artifacts[0])} exact artifacts per arm; 3 checked proofs, 3 checked original-CNF models, 6 inspected feedback streams', flush=True)
