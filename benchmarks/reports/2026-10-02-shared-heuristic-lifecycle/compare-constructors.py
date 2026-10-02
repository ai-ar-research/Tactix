import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

root = next(parent for parent in Path(__file__).resolve().parents if (parent / 'CLAUDE.md').is_file())
build = root / sys.argv[1]
baseline = '64ede6d7'
output = build / 'heuristic-lifecycle-control'
output.mkdir(exist_ok=True)

def sha_bytes(value):
    return hashlib.sha256(value).hexdigest()

def run(args, cwd):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True, timeout=180)
    if result.returncode:
        raise RuntimeError(f'{args[0]} failed ({result.returncode}):\n{result.stdout}\n{result.stderr}')
    return result.stdout

heuristics = subprocess.check_output(['git', 'show', f'{baseline}:src/policies/heuristics.hpp'], cwd=root)
controller = subprocess.check_output(['git', 'show', f'{baseline}:src/policies/restart_controller.hpp'], cwd=root)
match = re.search(rb'^struct ScopedHeuristics \{.*?^\};', heuristics, re.M | re.S)
assert match
block = match.group().replace(b'ScopedHeuristics', b'PreservedScopedHeuristics')
header = output / 'preserved-constructors.hpp'
header.write_bytes(b'#include "policies/heuristics.hpp"\nnamespace tactix {\n' + block + b'\n}\n' +
                   re.sub(rb'\bRestartController\b', b'PreservedRestartController', controller))
source = root / 'tests/test_heuristic_execution.cpp'
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(item for item in db if Path(item['file']) == source)
args = shlex.split(entry['command'])
obj = output / 'control.o'
args[args.index('-o') + 1] = str(obj)
args[1:1] = ['-DTACTIX_PRESERVED_HEURISTIC_LIFECYCLE', '-include', str(header)]
compile_output = run(args, entry['directory'])
target = 'test_heuristic_execution'
link = shlex.split((build / f'tests/CMakeFiles/{target}.dir/link.txt').read_text())
binary = output / 'control'
link[link.index('-o') + 1] = str(binary)
index = next(i for i, arg in enumerate(link) if arg.endswith('test_heuristic_execution.cpp.o'))
link[index] = str(obj)
link_output = run(link, build / 'tests')
control_output = run([str(binary)], root)
current_output = run([str(build / 'tests' / target)], root)
assert control_output == current_output
assert '19 controls' in control_output
result = {
    'baseline_commit': baseline,
    'scope': '16 fresh attachment matrices, 2 prior-H transfer/restore cases, 1 residual seed/import case; full H/ranking/phase state, activation capacity, root/retired exclusion, and growth',
    'preserved_sources': {'src/policies/heuristics.hpp': sha_bytes(heuristics),
                          'src/policies/restart_controller.hpp': sha_bytes(controller)},
    'preserved_scoped_block_sha256': sha_bytes(match.group()),
    'adaptations': ['Extract ScopedHeuristics block; rename only class identifiers to prevent inline symbol interposition; current native H definitions and machinery'],
    'generated_header_sha256': sha_bytes(header.read_bytes()),
    'fixture_sha256': sha_bytes(source.read_bytes()),
    'object_sha256': sha_bytes(obj.read_bytes()),
    'binary_sha256': sha_bytes(binary.read_bytes()),
    'compile_command': args, 'link_command': link,
    'compile_output': compile_output, 'link_output': link_output,
    'control_output': control_output, 'current_output': current_output,
    'controls': 19, 'passed': True,
}
(output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')
print(control_output, end='')
