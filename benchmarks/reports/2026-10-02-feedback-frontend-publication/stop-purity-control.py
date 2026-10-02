import hashlib
import json
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
build = root / sys.argv[1]
old = subprocess.check_output(['git', 'show', 'a4cfabe3:src/strategy/native_feedback.hpp'], cwd=root, text=True)
start = old.index('inline strategy::controller::FeedbackOutcome native_rule_outcome(')
opening = old.index('{', start)
depth, end = 1, opening + 1
while depth:
    depth += (old[end] == '{') - (old[end] == '}')
    end += 1
function = old[start:end].replace('native_rule_outcome(', 'preserved_native_rule_outcome(', 1)
source = build / 'feedback-stop-purity-control.cpp'
source.write_text('#include "strategy/native_feedback.hpp"\n#include <cstdio>\nnamespace tactix::execution {\n' + function + '\n}\n' + r'''
int main() {
  tactix::Kernel old, current;
  old.stop_fuzz = current.stop_fuzz = 1;
  const auto original = current.stop_fuzz_state;
  for (unsigned i = 0; i < 3; ++i) {
    tactix::execution::preserved_native_rule_outcome(old, "Factor", tactix::ApplyResult::NoOp);
    tactix::execution::native_rule_outcome(current, "Factor", tactix::ApplyResult::NoOp);
  }
  std::printf("old-fires=%llu current-fires=%llu old-state-changed=%d current-state-changed=%d\n",
    (unsigned long long)old.stats.stop_fuzz_fires, (unsigned long long)current.stats.stop_fuzz_fires,
    old.stop_fuzz_state != original, current.stop_fuzz_state != original);
  return old.stats.stop_fuzz_fires == 3 && current.stats.stop_fuzz_fires == 0 &&
    old.stop_fuzz_state != original && current.stop_fuzz_state == original ? 0 : 1;
}
''')
entry = next(item for item in json.loads((build / 'compile_commands.json').read_text())
             if item['file'] == str(root / 'tests/test_native_policy_feedback.cpp'))
args = shlex.split(entry['command'])
obj = source.with_suffix('.o')
args[args.index('-o') + 1] = str(obj)
args[args.index('-c') + 1] = str(source)
subprocess.run(args, cwd=entry['directory'], check=True)
args = shlex.split((build / 'tests/CMakeFiles/test_native_policy_feedback.dir/link.txt').read_text())
args[args.index('-o') + 1] = 'feedback_stop_purity_control'
args[args.index('CMakeFiles/test_native_policy_feedback.dir/test_native_policy_feedback.cpp.o')] = str(obj)
subprocess.run(args, cwd=build / 'tests', check=True)
result = subprocess.run([str(build / 'tests/feedback_stop_purity_control')], check=True, text=True, capture_output=True)
print(result.stdout.strip())
(build / 'feedback-stop-purity-control.json').write_text(json.dumps({
    'preserved_commit': 'a4cfabe3',
    'old_function_sha256': hashlib.sha256(old[start:end].encode()).hexdigest(),
    'old_adaptation': 'Rename helper only; function body unchanged.',
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'binary_sha256': hashlib.sha256((build / 'tests/feedback_stop_purity_control').read_bytes()).hexdigest(),
    'result': result.stdout.strip(), 'verified': True}, indent=2) + '\n')
