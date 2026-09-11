#!/usr/bin/env python3
"""Extend exact7073 replay with one existing, selected retry-producer journal."""
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import sys
from unittest.mock import patch

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
old = root / 'benchmarks/runs/20260910-7073-rank31-bve-priority'
c = root / 'benchmarks/runs/20260911-7210-rank31-retry-producer'
selectors = ('SAT_RETRY_PRODUCER_FROM', 'SAT_RETRY_PRODUCER_TO',
             'SAT_RETRY_PRODUCER_EXTERNAL')


def wrapper():
    body = (old / 'pins/solver_environment.py').read_text()
    before = "key in ('SAT_BVE_CANDIDATE_CONFLICT', 'SAT_BVE_HEAP_CONFLICT')"
    after = "key in " + repr(('SAT_BVE_CANDIDATE_CONFLICT',
                              'SAT_BVE_HEAP_CONFLICT', *selectors))
    assert body.count(before) == 1
    body = body.replace(before, after)
    before = "            raise ValueError('invalid selected BVE conflict')"
    after = before + "\n        if key == 'SAT_RETRY_PRODUCER_EXTERNAL' and not 0 < int(value) < 2**32:\n            raise ValueError('invalid external variable')"
    assert body.count(before) == 1
    return body.replace(before, after)


def fixtures():
    namespace = {}
    exec(compile(wrapper(), 'private_solver_environment.py', 'exec'), namespace)
    wrap = namespace['with_solver_environment']
    env = dict(zip(selectors, ('0', '5187498', '355342')))
    env['TACTIX_RETRY_PRODUCER_TRACE'] = '/private/trace.tsv'
    with patch.dict(os.environ, {selectors[0]: '7'}, clear=True):
        seen = []
        def measured(command):
            seen.append(dict(os.environ))
            return {}
        invoke = wrap(measured, '/pin/solver', env)
        invoke(['/pin/checker'])
        result = invoke(['/pin/solver'])
        invoke(['/pin/checker'])
        assert seen == [{selectors[0]: '7'}, env, {selectors[0]: '7'}]
        assert result['solver_environment'] == env
    with patch.dict(os.environ, {}, clear=True):
        def fail(command):
            raise RuntimeError('fixture')
        try:
            wrap(fail, '/pin/solver', env)(['/pin/solver'])
        except RuntimeError:
            pass
        else:
            raise AssertionError('exception hidden')
        assert not os.environ
    for key in selectors:
        for value in ('', '-1', '01', '1.0', '1\n', str(2**64), 1):
            try:
                wrap(measured, '/pin/solver', {key: value})
            except ValueError:
                pass
            else:
                raise AssertionError('invalid selector accepted')
    for key, value in [('SAT_RETRY_PRODUCER_EXTERNAL', '0'),
                       ('SAT_RETRY_PRODUCER_EXTERNAL', str(2**32)),
                       ('SAT_BVE_CANDIDATE_EXTERNAL', '355342'),
                       ('SAT_BVE_CANDIDATE_EXIT_AFTER_END', '1'), ('PATH', '1')]:
        try:
            wrap(measured, '/pin/solver', {key: value})
        except ValueError:
            pass
        else:
            raise AssertionError('out-of-scope value accepted')
    print('7210: solver-only scope/restoration/failure and26 rejected controls PASS')


def main():
    fixtures()
    if '--self-test' in sys.argv:
        return
    assert not c.exists(), 'successful preparation is frozen'
    m = json.loads((old / 'manifest.json').read_text())
    s = json.loads((old / 'solver-summary-7075.json').read_text())
    initial = {arm: v['initial'][0][0] for arm, v in s['observed'].items()}
    assert initial['candidate']['subject'] == initial['previous']['subject'] == 355342
    assert initial['candidate']['pos'] == -1 and initial['previous']['pos'] == 7319
    assert all((v['positive'], v['negative']) == (9, 8) for v in initial.values())
    deps = dict(m['dependencies'])
    def bind(p, expected=None):
        actual = sha(p)
        assert expected is None or actual == expected, p
        deps[str(p.relative_to(root))] = actual
    for rel, digest in s['hashes'].items():
        bind(root / rel, digest)
    for rel, digest in m['pin_sha256'].items():
        bind(old / rel, digest)
    for task in range(2):
        p = old / f'results/{task:03d}.json'
        r = json.loads(p.read_text())
        assert classify(r, True)['checked'] and classify(r, True)['clean']
        bind(p)
    for p in (Path(__file__), old / 'manifest.json', old / 'solver-summary-7075.json'):
        bind(p)
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items():
            assert sha(root / rel) == digest, rel
    for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'traces'):
        (c / name).mkdir(parents=True)
    for p in (old / 'pins').iterdir():
        if p.is_file():
            shutil.copy2(p, c / 'pins' / p.name)
    p = c / 'pins/solver_environment.py'
    p.chmod(0o644)
    p.write_text(wrapper())
    shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
    batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
    assert batch.count('7073-rank31-priority') == 1
    (c / 'array.sbatch').write_text(batch.replace('7073-rank31-priority', '7210-rank31-retry'))
    arms = {}
    for arm, am in m['arms'].items():
        assert b'TACTIX_RETRY_PRODUCER_TRACE\0' in (c / am['path']).read_bytes()
        env = {k: v.replace(str(old), str(c)) for k, v in am['environment'].items()}
        env.update(dict(zip(selectors, ('0', '5187498', '355342'))))
        env['TACTIX_RETRY_PRODUCER_TRACE'] = str(c / 'traces' / (arm + '.retry-producer.tsv'))
        arms[arm] = dict(am, environment=env)
    m.update(schema='tactix-rank31-retry-producer-full-replay-v1',
        created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms=arms,
        dependencies=deps, parent=str(old.relative_to(root)),
        selected_external=355342, selected_conflict=5187498, initial_witness=initial,
        hypothesis='The initial selected heap differs before selected-phase mutations:355342 absent versus pos7319 despite9/8 counts. Identify retry-mark producers rather than increasing ample budget or tuning heap ties. Sparse collection advice is a hypothesis, not proven origin.',
        purpose='Explain the largest changed-work adverse transfer while retaining the general7030 repair and independent unit-cost experiments.',
        validation='Require full own Release work/certificate and exact complete7073 macro,boundary,attempt,heap traces before interpreting the only new selected producer journal. Independent checks remain asynchronous.',
        test_consideration='Existing immutable7030/6730 trace binaries and7032 strict producer parser. New wrapper fixtures cover exact solver-only scope, restoration/failure,21 malformed selector values and5 forbidden/range controls. No solver unit behavior changes.',
        limitation='Selected marks do not reconstruct prior failed attempts or full formula/native state. No native rerun, global event trace, forced exit, strategy change, source promotion or fuzz waiver.')
    m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
        [*(p for p in (c / 'pins').iterdir() if p.is_file()), c / 'matrix.tsv', c / 'array.sbatch']}
    with (c / 'manifest.json').open('x') as stream:
        json.dump(m, stream, indent=2)
        stream.write('\n')
    for p in (c / 'pins').iterdir():
        if p.is_file():
            p.chmod(p.stat().st_mode & ~0o222)
    print(c)


if __name__ == '__main__':
    main()
