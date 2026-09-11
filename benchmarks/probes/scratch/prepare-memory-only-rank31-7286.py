#!/usr/bin/env python3
"""Pin one matched Rank31 memory-only diagnostic, preserving solver gates."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260911-7286-memory-only-rank31'
assert not c.exists(), 'prepared campaign immutable'
prior = root / 'benchmarks/runs/20260911-7277-realloc-rank31-memory'
m = json.loads((prior / 'manifest.json').read_text())
authority = root / 'benchmarks/runs/20260911-7291-memory-only-build/manifest.json'
b = json.loads(authority.read_text())
assert b['status'] == 'PASS'
evidence = {}


def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    evidence[str(p.relative_to(root))] = digest


bind(authority)
bind(prior / 'manifest.json')
for rel, digest in b['hashes'].items():
    bind(root / rel, digest)
for rel, digest in m['pin_sha256'].items():
    bind(prior / rel, digest)
for arm in ('candidate', 'previous'):
    control = b['binaries'][arm + '-control']
    assert control['exact_disabled_control']
    bind(root / control['path'], control['sha256'])
    oracle = m['release_records'][arm]
    bind(root / oracle['path'], oracle['sha256'])
for name in ('pins', 'logs', 'claims', 'provisional', 'results'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'run.py', 'modelcheck.py', 'drat-trim', 'checkmodel'):
    shutil.copy2(prior / 'pins' / name, c / 'pins' / name)
shutil.copy2(prior / 'matrix.tsv', c / 'matrix.tsv')
for arm in ('candidate', 'previous'):
    binary = b['binaries'][arm + '-observer']
    assert binary['observer']
    bind(root / binary['path'], binary['sha256'])
    shutil.copy2(root / binary['path'], c / 'pins' / arm)
    m['arms'][arm] = dict(path='pins/' + arm, sha256=binary['sha256'], arguments=[],
        source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',
        source_build=str(Path(binary['path']).parent))
batch = (prior / 'array.sbatch').read_text().replace(str(prior), str(c))
batch = batch.replace('7277-realloc-rank31-memory', '7286-memory-only-rank31')
with (c / 'array.sbatch').open('x') as out:
    out.write(batch)
bind(Path(__file__))
bind(Path(__file__).with_name('validate-memory-only-rank31-7286.py'))
m.update(schema='tactix-memory-only-rank31-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    evidence_sha256=evidence,
    hypothesis='Memory-only diagnostics with all general EVENT_TRACE machinery compiled off may reproduce the7267 Release RSS regression that7277 heavier trace reversed.',
    acceptance='Same original rank31 and own Release raw work/certificates including physical counters. Report both observer peaks and their differences from562920/495156KiB Release. Require reproduction of the adverse direction before cause attribution, and disclose remaining observer perturbation. Both independent proofs and cleanup remain mandatory; pending proofs permit provisional work only.',
    test_consideration='7291 full four suites, both disabled binaries byte-identical to own Release, new enabled/disabled/bad-path and capacity-suppression/temporary-owner tests. Unchanged7277 wrapper and strict memory/parser/fingerprint controls reused. Initial7285/7287/7289 observer builds exposed guarded scheduler/candidate-order/kernel storage accessors; failed logs and sources preserved. Both syntax preflights now precede full compilation.',
    limitation='One diagnostic pair, not a runtime acceptance gate. Observer changes timing and may still change allocator history. Do not claim a memory fix or map trace peaks to Release without reproduction. No Rank21 reopening or budget/schedule/guard change.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p)
    for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
