#!/usr/bin/env python3
"""Preserve and classify baseline-only fixture failures, not solver regressions."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
target = c/'fixture-classification-6999.json'; assert not target.exists()
paths = [Path(__file__)]
for number, fixture in ((6983,'test-ite-pair-failed-6983.cpp'),(6984,'test-ite-pair-diagnostic-6984.cpp')):
    assert not (c/f'control-{number}.json').exists()
    assert not (c/f'candidate-test_ite_pair-{number}').exists()
    path = c/'logs'/f'previous-test_ite_pair-run-{number}.json'
    log = json.loads(path.read_text())
    assert log['exit_code'] == -6
    if number == 6984: assert 'count=65 signs=0 mask=15 reverse=0 copies=1 found=65' in log['stderr']
    paths.extend([path,c/'logs'/f'previous-test_ite_pair-link-{number}.json',
                  root/'benchmarks/probes/scratch'/fixture,
                  root/'benchmarks/probes/scratch'/f'control-ite-pair-{number}.py'])
correct = c/'control-6985.json'; valid = json.loads(correct.read_text())
assert valid['outputs']['previous'] == valid['outputs']['candidate']
assert valid['outputs']['previous']['test_ite_pair'] == 'PASS 560 ITE pair cases digest=551691671817ff7b\n'
paths.append(correct)
with target.open('x') as out:
    json.dump(dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},
        classification='6983 assumed four conditional entries per signed output per full ITE.6984 diagnostic identifies unchanged baseline emits65 gates, contrary to incorrect64 cap. Actual extraction emits two entries per signed output:128 gates reach256,129 exceed. Failed original/diagnostic fixture sources and logs retained; candidate never executed in either failed attempt. Corrected560-case public fixture6985 passes both actual objects, including exact/over boundary. No solver-source change caused or fixed this test failure.',
        watcher_preflight='Initial full50 watcher invocation used offset-qualified05:11:11-04:00, which argparse rejected before monitoring. Correct local-time05:11:11 invocation started session82836; job1679487 unchanged and never resubmitted.'),out,indent=2); out.write('\n')
print(target)
