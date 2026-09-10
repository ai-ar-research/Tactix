#!/usr/bin/env python3
"""Extend the verified common300K prefix; localize the new adverse split later."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260910-7046-collect-retry-rank31'
c = root / 'benchmarks/runs/20260910-7050-collect-retry-rank31-late'
assert not c.exists()
p = old / 'summary-7048.json'; s = json.loads(p.read_text())
assert s['comparisons']['candidate_vs_previous']['decisions']['complete_equal']
assert s['prefixes']['candidate'] == s['prefixes']['previous']
common = s['prefixes']['candidate']
assert common['conflict'] == 300000 and common['count'] == 96317845
m = json.loads((old / 'manifest.json').read_text())
dependencies = {}
def bind(path, expected=None):
    digest = sha(path)
    assert expected is None or digest == expected, path
    dependencies[str(path.relative_to(root))] = digest
for rel, digest in m['hashes'].items(): bind(old / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for arm in ('candidate', 'previous'):
    path = old / 'results' / (arm + '.json'); bind(path)
    r = json.loads(path.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    for name, digest in r['trace_sha256'].items(): bind(old / 'traces' / name, digest)
for path in (p, old / 'manifest.json', Path(__file__)): bind(path)
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
for arm in ('candidate', 'previous'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-7046-', 'tactix-7050-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('7046-collect-retry-rank31', '7050-collect-retry-rank31-late'))
m.update(stop_conflict=1000000, stream_from=300000, common_decisions=common['count'],
    prior_common_hash=common['hash'], parent=str(old.relative_to(root)), dependencies=dependencies,
    hypothesis='Actual7030 and6730 agree for96317845 decisions through300K, including the old native179823 omission. The new adverse transfer starts later. Compare the verified300K..1M suffix, preserving all actual threshold overlap rows and raw macro boundaries.',
    validation='Require each unchanged pin to reproduce its whole prior300K rolling hash and exact overlap bytes. Account for complete suffix through1M, then compare first candidate/previous difference. Native earlier split stays known, not the cause of this new change.',
    test_consideration='Existing full/suffix/overlap decision-prefix fixtures and boundary fixtures; no solver/observer/parser change. New configuration changes only stop/stream/common-prefix observer controls.',
    limitation='Stopped causal probe, not a runtime or proof acceptance gate. No native rerun or rejected5920 restoration. Finish predeclared runtime repetitions independently.')
m['hashes'] = {str(path.relative_to(c)): sha(path) for path in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for path in (c / 'pins').iterdir(): path.chmod(path.stat().st_mode & ~0o222)
print(c)
