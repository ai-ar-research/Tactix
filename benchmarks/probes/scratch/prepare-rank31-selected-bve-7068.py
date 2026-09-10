#!/usr/bin/env python3
"""Replay the validated full run with attempts only at the first differing BVE."""
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from solver_environment import with_solver_environment

old = root / 'benchmarks/runs/20260910-7064-rank31-full-boundaries'
c = root / 'benchmarks/runs/20260910-7068-rank31-selected-bve'
assert not c.exists()
assert not any(k.startswith(('SAT_', 'TACTIX_', 'KISSAT_')) for k in os.environ)
m = json.loads((old / 'manifest.json').read_text())
s = json.loads((old / 'solver-summary-7066.json').read_text())
assert all(r['full_release_replay'] for r in s['records'].values())
first = s['raw_first_differences']['boundaries.tsv']
assert first['candidate'].split('\t')[2:5] == ['BVE', 'end', '5187498']
assert first['previous'].split('\t')[2:5] == ['BVE', 'end', '5187498']
deps = dict(m['dependencies'])
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    deps[str(p.relative_to(root))] = digest
for rel, digest in s['hashes'].items(): bind(root / rel, digest)
for rel, digest in m['pin_sha256'].items(): bind(old / rel, digest)
for p in (Path(__file__), old / 'manifest.json', old / 'solver-summary-7066.json',
          root / 'tools/bench/solver_environment.py', root / 'tools/bench/test_solver_environment.py'):
    bind(p)
for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'traces'):
    (c / name).mkdir(parents=True)
for p in (old / 'pins').iterdir():
    if p.is_file(): shutil.copy2(p, c / 'pins' / p.name)
# Replace only this unsubmitted campaign's private wrapper. Prior campaigns
# continue using their own immutable copy; no submitted file is edited.
(c / 'pins/solver_environment.py').chmod(0o644)
shutil.copy2(root / 'tools/bench/solver_environment.py', c / 'pins/solver_environment.py')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c))
assert batch.count('7064-rank31-boundaries') == 1
batch = batch.replace('7064-rank31-boundaries', '7068-rank31-bve')
(c / 'array.sbatch').write_text(batch)
arms = {}
for arm, am in m['arms'].items():
    env = {key: value.replace(str(old), str(c)) for key, value in am['environment'].items()}
    env.update(TACTIX_BVE_CANDIDATE_TRACE=str(c / 'traces' / (arm + '.bve-candidates.tsv')),
               SAT_BVE_CANDIDATE_CONFLICT='5187498')
    with_solver_environment(lambda command: {}, c / 'pins' / arm, env)
    arms[arm] = dict(am, environment=env)
m.update(schema='tactix-selected-bve-full-replay-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=arms, dependencies=deps, parent=str(old.relative_to(root)), selected_conflict=5187498,
    purpose='Localize the first changed BVE attempt, effort or outcome in the Rank31 adverse transfer.',
    hypothesis='Retry eligibility or heap order changes candidate attempts in the first differing BVE phase. Compare complete begin/end/postflush records, ticks and limits before attributing the extra search work.',
    validation='Expected full UNSAT; exact whole own Release non-timing work and certificate; byte-identical complete7064 macro and inprocessing boundaries; strict selected-conflict attempt framing. Proof checks and cleanup asynchronous.',
    test_consideration='No solver/observer source changes. Four new harness tests cover the sole allowlisted SAT_BVE_CANDIDATE_CONFLICT selector, validation and restoration, and reject stop controls. Existing6192 candidate reader/attempt fixtures reused.',
    limitation='No native rerun, global event trace, decision stream, heap observer or forced stop. This locates changed attempts, not yet a complete source-backed repair or native first-choice cause.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
                   [*(p for p in (c / 'pins').iterdir() if p.is_file()), c / 'matrix.tsv', c / 'array.sbatch']}
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): assert sha(root / rel) == digest, rel
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
