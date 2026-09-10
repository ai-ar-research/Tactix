#!/usr/bin/env python3
"""Extend the actually equal one-million-conflict paired prefix to two million."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix
old = root / 'benchmarks/runs/20260910-7050-collect-retry-rank31-late'
c = root / 'benchmarks/runs/20260910-7055-collect-retry-rank31-2m'
assert not c.exists()
p = old / 'summary-7052.json'; s = json.loads(p.read_text())
assert s['decisions']['complete_equal'] and all(s['inherited300k'].values())
common = read_prefix(old / 'traces/candidate.prefix.tsv')
assert common == read_prefix(old / 'traces/previous.prefix.tsv')
assert common['conflict'] == 1000000 and common['count'] == 123392789
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
    assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
    for name, digest in r['trace_sha256'].items(): bind(old / 'traces' / name, digest)
for path in (p, old / 'manifest.json', Path(__file__)): bind(path)
for name in ('pins', 'logs', 'results', 'traces'): (c / name).mkdir(parents=True)
for arm in ('candidate', 'previous'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-7050-', 'tactix-7055-'))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('7050-collect-retry-rank31-late', '7055-collect-retry-rank31-2m'))
m.update(stop_conflict=2000000, stream_from=1000000, common_decisions=common['count'],
    prior_common_hash=common['hash'], parent=str(old.relative_to(root)), dependencies=dependencies,
    hypothesis='7030 and6730 make123392789 identical decisions through1M conflicts. Locate the new adverse transfer later, comparing their complete1M..2M suffix and actual threshold overlap.',
    validation='Require whole cached1M hash and exact actual overlap bytes before comparing complete suffixes. Retain all boundaries. Earlier native179823 mismatch is not newly caused by7030.',
    test_consideration='Existing decision-prefix full/suffix/overlap and boundary fixtures cover unchanged observers, runner and parser. Configuration-only stop/stream extension; no new solver logic.',
    limitation='Stopped paired diagnostic, not a proof or performance acceptance gate. No native rerun or restoration of rejected5920. Full50 transfer is independent.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as out:
    json.dump(m, out, indent=2); out.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
