#!/usr/bin/env python3
"""Final suites, actual old/new/native occurrence controls, independent proofs."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
c = root / 'benchmarks/runs/20260908-5870-forward-preclean'
control = root / 'benchmarks/runs/20260908-5875-forward-preclean-controls'
control.mkdir(exist_ok=False)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(log, command, env=None):
    assert not log.exists()
    with log.open('w') as stream:
        result = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT, env=env)
    assert result.returncode == 0, (log, result.returncode, log.read_text()[-10000:])
    print(log.name, 'PASS', flush=True)
for name, number in [('release', 5870), ('bc', 5871), ('trace', 5872), ('journal', 5873)]:
    build = root / f'build/pin-{number}-forward-preclean-{name}'
    run(c / f'logs/{name}-build-sealed.log', ['cmake', '--build', build, '-j8'])
    run(c / f'logs/{name}-ctest-sealed.log', ['ctest', '--test-dir', build, '--output-on-failure', '-j1'])
header = root / 'benchmarks/runs/20260908-5859-forward-caller-edges-final/forward-fixture.hpp'
hashes = {str(header.relative_to(root)): sha(header)}
commands = []
for label, number, name, expected in [('old', 5849, 'forward-caller', 1), ('new', 5870, 'forward-preclean', 0)]:
    source = root / f'.claude/worktrees/runtime-{name}-{number}'
    build = root / f'build/pin-{number}-{name}-release'
    assert header.read_text() == (source / 'tests/test_forward_unit.cpp').read_text().split('static void independent_tail(')[0]
    link = shlex.split((build / 'tests/CMakeFiles/test_forward_unit.dir/link.txt').read_text())
    objects = [(build / 'tests' / p).resolve() for p in link if p.endswith('.o')][1:]
    fixture = here / 'forward-preclean-control-5872.cpp'
    command = [link[0], '-O3', '-DNDEBUG', '-std=c++20', '-fno-exceptions', '-fno-rtti',
               f'-DEXPECT_INCOMING={expected}', '-I' + str(source / 'src'), '-I' + str(source / 'include'),
               '-I' + str(header.parent), fixture, *objects, '-o', control / label]
    commands.append(list(map(str, command)))
    run(control / f'{label}-compile.log', command)
    run(control / f'{label}.log', [control / label])
    for p in [fixture, *objects, control / label]: hashes[str(p.relative_to(root))] = sha(p)
native = root / 'tools/solvers/variants/kissat-score-bump-observer-5696'
fixture = here / 'forward-preclean-native-5873.c'
library = native / 'build/libkissat.a'
command = ['gcc', '-O3', '-DNDEBUG', '-I' + str(native / 'src'), fixture, library, '-lm', '-o', control / 'native']
commands.append(list(map(str, command)))
run(control / 'native-compile.log', command)
run(control / 'native.log', [control / 'native'])
for p in [fixture, library, native / 'src/watch.c', control / 'native']: hashes[str(p.relative_to(root))] = sha(p)
(control / 'summary.json').write_text(json.dumps(dict(commands=commands, hashes=hashes,
    outcome='Actual old/new/native controls: incoming-satisfied occurrences old1,new0,native0;newly satisfied after connection retain1 in all arms. Both Tactix states finish with valid original SAT models.'), indent=2) + '\n')

script = here / 'validate-forward-unit-5801.py'
for name, pin, test, count in [('continuation-proofs', '5870-forward-preclean-release', 'forward_unit', 16),
                              ('caller-proofs', '5870-forward-preclean-release', 'forward_caller', 6),
                              ('journal-proofs', '5873-forward-preclean-journal', 'forward_caller', 6)]:
    code = script.read_text().replace('20260908-5798-forward-unit/fixed-proofs', '20260908-5870-forward-preclean/' + name)
    code = code.replace('pin-5798-forward-unit-release/tests/test_forward_unit', f'pin-{pin}/tests/test_{test}')
    code = code.replace('len(records) == 16', f'len(records) == {count}')
    code = code.replace('checked_micro_proofs=16', f'checked_micro_proofs={count}')
    code = code.replace('state_model_cases=17', f'state_model_cases={17 if count == 16 else 6}')
    code = code.replace('17 state/model cases, 8 DRAT and 8 LRAT independently verified proofs', f'{name}: {count} independently verified proofs')
    env_keys = ['TACTIX_RULE_JOURNAL', 'TACTIX_RULE_JOURNAL_RULE', 'TACTIX_RULE_JOURNAL_CANDIDATE']
    assert not any(key in os.environ for key in env_keys)
    if name == 'journal-proofs':
        os.environ.update(TACTIX_RULE_JOURNAL=str(c / 'caller-journal.tsv'), TACTIX_RULE_JOURNAL_RULE='Propagate',
                          TACTIX_RULE_JOURNAL_CANDIDATE='post-forward-connected')
    try:
        exec(compile(code, str(script), 'exec'), {'__file__': str(script), '__name__': '__main__'})
    finally:
        if name == 'journal-proofs':
            for key in env_keys: del os.environ[key]
script = here / 'validate-forward-unit-edges-5813.py'
code = script.read_text()
for old, new in {'20260908-5813-forward-unit-edges': '20260908-5876-forward-preclean-edges',
                 'pin-5798-forward-unit-release': 'pin-5870-forward-preclean-release',
                 'runtime-forward-unit-5798': 'runtime-forward-preclean-5870', 'against5798': 'against5870'}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(script), 'exec'), {'__file__': str(script), '__name__': '__main__'})
journal = c / 'caller-journal.tsv'
rows = [line.split('\t') for line in journal.read_text().splitlines() if not line.startswith('#')]
assert [r[14] for r in rows] == ['applied', 'applied', 'declined', 'applied', 'declined', 'applied'] + ['refuted'] * 6
summaries = [c / (name + '/summary.json') for name in ['continuation-proofs', 'caller-proofs', 'journal-proofs']]
summaries += [control / 'summary.json', root / 'benchmarks/runs/20260908-5876-forward-preclean-edges/summary.json']
(c / 'final-proof-validation.json').write_text(json.dumps(dict(checked_proofs=40, journal_active_outcomes=[r[14] for r in rows],
    hashes={str(p.relative_to(root)): sha(p) for p in [*summaries, journal]},
    test_consideration='Three new occurrence-lifetime tests,old/new/native controls,40 checked DRAT/LRAT proofs including preconnection deletion before refutation and active-journal paths. No checker acceptance changes.'), indent=2) + '\n')
print('PASS: final suites, actual three-arm controls and40 checked proofs')
