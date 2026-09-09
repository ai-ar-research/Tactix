#!/usr/bin/env python3
"""Collect selected Reorder formula and binary inputs after exact score bracketing."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6624-rank35-score-history'
c = root / 'benchmarks/runs/20260909-6629-rank35-reorder-input'
assert not c.exists()
summary = old / 'summary-6626.json'
s = json.loads(summary.read_text())
assert s['complete_own_replay'] and s['comparison']['differences']['score']['count'] == 1
first = s['comparison']['differences']['score']['first']
assert first['key'] == [3074062, 161620843, 'reorder_end', 0]
assert first['candidate']['score'] == 4.92578125 and first['previous']['score'] == 5.92578125
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(old / rel) == digest, rel
implementations = [root / '.claude/worktrees' / name / 'src/policies/ops/reorder/reorder.cpp'
                   for name in ('runtime-material-schedule-6579', 'runtime-substitute-collapse-6305')]
assert sha(implementations[0]) == sha(implementations[1])
runner = (old / 'run.py').read_text().replace('tactix-6624-', 'tactix-6629-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = """env.update(SAT_EVENT_TRACE_FORMULA_SET_PATH=str(c / 'traces' / (arm + '.formula.tsv')),
           SAT_EVENT_TRACE_FORMULA_SET_MATCH='reorder_begin',
           SAT_EVENT_TRACE_FORMULA_SET_CONFLICT='3074062',
           SAT_EVENT_TRACE_FORMULA_SET_EXTERNALS='1373',
           SAT_EVENT_TRACE_FORMULA_SET_POSE='1', SAT_EVENT_TRACE_FORMULA_SET_META='1',
           SAT_EVENT_TRACE_FORMULA_SET_VALUES='1',
           SAT_EVENT_TRACE_BINARY_SET_PATH=str(c / 'traces' / (arm + '.binaries.tsv')),
           SAT_EVENT_TRACE_BINARY_SET_MATCH='reorder_begin',
           SAT_EVENT_TRACE_BINARY_SET_CONFLICT='3074062',
           SAT_EVENT_TRACE_BINARY_SET_EXTERNALS='1373', SAT_EVENT_TRACE_BINARY_SET_ONCE='1',
           TACTIX_MACRO_ACTIVE_SET=str(c / 'traces' / (arm + '.active.tsv')),
           SAT_MACRO_ACTIVE_SET_EVENT='reorder_begin', SAT_MACRO_ACTIVE_SET_CONFLICT='3074062')
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
        m['dependencies'][str((old / 'traces' / rel).relative_to(root))] = digest
for p in implementations:
    m['dependencies'][str(p.relative_to(root))] = sha(p)
m['dependencies'][str(summary.relative_to(root))] = sha(summary)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6624-rank35-scores', '6629-reorder-input'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), cached_controls=str(old.relative_to(root)),
    input_snapshot=dict(event='reorder_begin', conflicts=3074062, decisions=161620843, external=1373),
    purpose='1373 enters Reorder with equal zero score, position and availability, then weights differ by exactly1. Both Reorder implementation files are identical. Compare all incident large clauses/poses/root values and binary multiplicities, plus active set at root, to distinguish formula input from score handling before any repair.',
    validation='Require every6624 ordinary/heap/choice/score trace and partial-proof byte/hash unchanged. Macro emit reaches dormant binary/formula snapshots before its no-output return, so no global tracer/new observer is enabled. Require nonempty declared frames at exact boundary, all incident rows and complete active set. Quantify contributions only with known endpoint/root state.',
    test_consideration='No source or observer edit. Reuse maintained binary-snapshot/represented-value tests and5785 full pose/meta/root parser fixtures. Add contribution/boundary tests before interpreting any reconstructed weight; never treat missing endpoint values as free.',
    limitation='Selected input witness, not native equivalence or final proof acceptance. Earlier removed binary or stronger simplification must not be restored merely to force the cheaper old branch. Preserve Rank32/33 closures and all proof/correctness gates.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/previous', 'run.py', 'array.sbatch')}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2)
    stream.write('\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
