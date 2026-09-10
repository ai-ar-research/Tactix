#!/usr/bin/env python3
"""Observe four selected heap histories without changing any solver binary."""
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
old = root / 'benchmarks/runs/20260910-7068-rank31-selected-bve'
c = root / 'benchmarks/runs/20260910-7073-rank31-bve-priority'

def wrapper():
    body = (old / 'pins/solver_environment.py').read_text()
    before = "selected_bve = key == 'SAT_BVE_CANDIDATE_CONFLICT'"
    after = "selected_bve = key in ('SAT_BVE_CANDIDATE_CONFLICT', 'SAT_BVE_HEAP_CONFLICT')"
    assert body.count(before) == 1
    return body.replace(before, after)

def fixtures():
    namespace = {}; exec(compile(wrapper(), 'private_solver_environment.py', 'exec'), namespace)
    wrap = namespace['with_solver_environment']; key = 'SAT_BVE_HEAP_CONFLICT'
    env = {key: '5187498', 'TACTIX_EVENT_TRACE_BVE_HEAP': '355342,355344,497326,497345'}
    with patch.dict(os.environ, {key: '5'}, clear=True):
        seen = []
        def measured(command):
            seen.append(dict(os.environ)); return {}
        invoke = wrap(measured, '/pin/solver', env)
        invoke(['/pin/checker']); result = invoke(['/pin/solver']); invoke(['/pin/checker'])
        assert seen == [{key: '5'}, env, {key: '5'}]
        assert result['solver_environment'] == env
    with patch.dict(os.environ, {}, clear=True):
        def fail(command): raise RuntimeError('test')
        try: wrap(fail, '/pin/solver', env)(['/pin/solver'])
        except RuntimeError: pass
        else: raise AssertionError('exception hidden')
        assert not os.environ
    for value in ('', '-1', '01', '1.0', '1\n', str(2**64), 1):
        try: wrap(measured, '/pin/solver', {key: value})
        except ValueError: pass
        else: raise AssertionError('invalid heap conflict accepted')
    for key in ('SAT_BVE_SCHEDULE_CONFLICT', 'SAT_BVE_CANDIDATE_EXIT_AFTER_END', 'PATH'):
        try: wrap(measured, '/pin/solver', {key: '1'})
        except ValueError: pass
        else: raise AssertionError('unrelated control accepted')
    print('PASS: private heap selector, exact solver/checker scope, restoration/failure, seven bad values and three forbidden controls')

def main():
    fixtures()
    if '--self-test' in sys.argv: return
    assert not c.exists()
    assert not any(k.startswith(('SAT_', 'TACTIX_', 'KISSAT_')) for k in os.environ)
    m = json.loads((old / 'manifest.json').read_text())
    s = json.loads((old / 'effect-summary-7072.json').read_text())
    assert s['previous_only_eliminations'] == [355342] and not s['candidate_only_eliminations']
    subjects = s['next_subjects']; assert subjects == [355342, 355344, 497326, 497345]
    deps = dict(m['dependencies'])
    def bind(p, expected=None):
        value = sha(p); assert expected is None or value == expected, p
        deps[str(p.relative_to(root))] = value
    for rel, digest in s['hashes'].items(): bind(root / rel, digest)
    for rel, digest in m['pin_sha256'].items(): bind(old / rel, digest)
    for p in (Path(__file__), old / 'manifest.json', old / 'effect-summary-7072.json'):
        bind(p)
    for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'traces'):
        (c / name).mkdir(parents=True)
    for p in (old / 'pins').iterdir():
        if p.is_file(): shutil.copy2(p, c / 'pins' / p.name)
    p = c / 'pins/solver_environment.py'; p.chmod(0o644); p.write_text(wrapper())
    shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
    batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
    assert batch.count('7068-rank31-bve') == 1
    (c / 'array.sbatch').write_text(batch.replace('7068-rank31-bve', '7073-rank31-priority'))
    arms = {}
    for arm, am in m['arms'].items():
        env = {k: v.replace(str(old), str(c)) for k, v in am['environment'].items()}
        env.update(TACTIX_BVE_HEAP_TRACE=str(c / 'traces' / (arm + '.bve-heap.tsv')),
                   TACTIX_EVENT_TRACE_BVE_HEAP=','.join(map(str, subjects)),
                   SAT_BVE_HEAP_CONFLICT='5187498')
        arms[arm] = dict(am, environment=env)
    m.update(schema='tactix-selected-bve-priority-full-replay-v1',
        created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms=arms, dependencies=deps,
        parent=str(old.relative_to(root)), subjects=subjects,
        purpose='Distinguish incoming selected heap score/position from later producer-triggered movement behind the lost355342 elimination.',
        hypothesis='The equal-budget late BVE effect is driven by priority/order, not a ceiling. Trace355342/355344 and the initial497326/497345 pair through initial and changed heap states, retaining root,size,score,occurrences and trigger.',
        validation='Expected full UNSAT; exact whole own Release work/certificate; every byte of7068 macro,boundary and candidate traces; strict selected four-subject heap framing before interpretation. Normal independent proof checks and cleanup remain asynchronous.',
        test_consideration='Existing binaries/observers and6196 heap reader. Private wrapper only adds the read-only heap conflict selector; preparation tests solver-only application, restoration/failure, invalid values and forbidden controls. No maintained tool, submitted pin or solver source changes.',
        limitation='Selected heap snapshots are not the full schedule or formula. No tie-break tuning, native rerun, global event trace, forced stop, source promotion or budget change.')
    m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
        [*(p for p in (c / 'pins').iterdir() if p.is_file()), c / 'matrix.tsv', c / 'array.sbatch']}
    for group in ('dependencies', 'protected_campaign_hashes'):
        for rel, digest in m[group].items(): assert sha(root / rel) == digest, rel
    with (c / 'manifest.json').open('x') as stream:
        json.dump(m, stream, indent=2); stream.write('\n')
    for p in (c / 'pins').iterdir():
        if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)

if __name__ == '__main__': main()
