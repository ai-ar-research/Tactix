#!/usr/bin/env python3
"""Locate the first selected score difference using unchanged standalone hooks."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6619-material-rank35-choice'
c = root / 'benchmarks/runs/20260909-6624-rank35-score-history'
assert not c.exists()
summary = old / 'summary-6621.json'
s = json.loads(summary.read_text())
assert s['complete_own_replay']
assert s['heap_differences']['score'] == [dict(variable=1373, candidate='4.92578125', previous='5.92578125')]
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest, rel
runner = (old / 'run.py').read_text().replace('tactix-6619-', 'tactix-6624-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = """env['TACTIX_INPROCESS_SCORE_TRACE'] = str(c / 'traces' / (arm + '.scores.tsv'))
env['SAT_INPROCESS_SCORE_EXTERNAL'] = '1373'
env['SAT_INPROCESS_SCORE_CONFLICT_FROM'] = '0'
"""
runner = runner.replace(anchor, extra + anchor)
compile(runner, str(c / 'run.py'), 'exec')
for folder in ('pins', 'logs', 'traces', 'results'):
    (c / folder).mkdir(parents=True)
for arm in ('candidate', 'previous'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
    p = old / f'results/{arm}.json'
    r = json.loads(p.read_text())
    m['dependencies'][str(p.relative_to(root))] = sha(p)
    for rel, digest in r['trace_sha256'].items():
        p = old / 'traces' / rel
        m['dependencies'][str(p.relative_to(root))] = digest
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6619-rank35-choice', '6624-rank35-scores'))
m['dependencies'][str(summary.relative_to(root))] = sha(summary)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), cached_controls=str(old.relative_to(root)),
    score_variable=1373,
    purpose='All49690 branch heap members/flags agree before split, one score differs by1 and2407 positions differ. Locate1373 first differing macro score/position/availability, especially preceding Reorder3074062. Do not assume the score difference causes the topology or that it is worse semantics.',
    validation='Require all6619 ordinary/heap/choice traces and partial-proof bytes/hash exactly before interpreting complete score history. Compare aligned macro labels/conflicts/decisions and separate score, position and availability differences. No native rerun or full timing claim.',
    test_consideration='No source/observer/runner algorithm edit. Reuse existing tested inprocess-score hook and full replay controls; selected-history parser must cover exact schema, missing rows, numeric values, repeated labels and first mismatch categories before interpretation.',
    limitation='Selected variable from the complete prior score diff, not a solver parameter. A score gap is not causal proof of branch topology or an invitation to restore removed binaries or tune ties.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/previous', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2)
    stream.write('\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
