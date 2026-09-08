#!/usr/bin/env python3
"""Measure the latest exact-work implementation stack over all Rank 1--40."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260907-5465-bound-advance-ranks1-40'
authority = runs / '20260908-5473-sweep-cursor'
timing = runs / '20260908-5475-sweep-cursor-rank35-runtime/solver-checkpoint.json'
gate = runs / '20260908-5476-sweep-cursor-BC-gate'
c = runs / '20260908-5485-sweep-cursor-ranks1-40'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
a = json.loads((authority / 'manifest.json').read_text())
t = json.loads(timing.read_text())
assert t['solver_results'] == 6 and t['same_non_timing_statistics_and_proof']
assert t['paired']['max'] < 1 and t['absolute_range_within_two_percent']
for rel, digest in a['binaries'].items():
    assert sha(root / rel) == digest
assert len(set(a['binaries'].values())) == 1
release = root / 'build/pin-5473-sweep-cursor-release/tactix'
records = {}
checked = 0
for sub in ('provisional', 'results'):
    for p in (gate / sub).glob('*.json'):
        r = json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code'] in (10, 20)
        assert not r['solve']['timed_out']
        if sub == 'results':
            assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
            check = r['model_check'] if r['solve']['exit_code'] == 10 else r['proof_check']
            assert check['exit_code'] == 0 and not check['timed_out']
            checked += 1
        records[r['task']] = str(p.relative_to(root))
assert set(range(56)).issubset(records), 'Require all B28 solver results before extending the full view'
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old / rel) == digest
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir()
for p in (old / 'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, c / 'pins' / p.name)
dest = c / 'pins/tactix'
dest.chmod(0o755)
shutil.copy2(release, dest)
patch = c / 'pins/candidate-source.patch'
patch.chmod(0o644)
shutil.copy2(authority / 'cumulative.patch', patch)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m['arms']['tactix'].update(sha256=sha(release), source_build=str(release.parent.relative_to(root)),
                           source_patch_sha256=sha(patch))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         parent_campaign=str(old.relative_to(root)), predecessor=str(old.relative_to(root)),
         candidate_authority=str(authority.relative_to(root)),
         purpose='Full unchanged Rank 1-40 paired Kissat comparison of bound lifecycle plus exact-layout watch append and Sweep membership cursors.',
         hypothesis='Two source-backed implementation optimizations have independent exact-work local gains; measure their actual aggregate effect against Kissat without multiplying cross-window ratios or changing schedules.',
         candidate_validation='Independent Release 5473/5474: identical SHA, 84/84 tests each, 588 actual old/new Sweep-ledger cases. Inherited append fixture: 180 exact-state cases. UNFUZZED.',
         build_identity='Frozen tested Release 5473, diagnostic observers disabled; same native Kissat, matrix, runner, checkers and resource contract as 5465.',
         timing_authority=dict(path=str(timing.relative_to(root)), sha256=sha(timing)),
         regression_gate=dict(campaign=str(gate.relative_to(root)), expected_solver_records=len(records),
                              checked_clean=checked, records=records,
                              limitation='Pending remainder/checks permit provisional investigation only. Full B28+C5, checked full view and latest-source fuzz remain mandatory before promotion.'),
         test_consideration='No solver or checker code change. Reuse tested runner and fixed matrix; assert parent pin hashes, identical independent candidate binaries, six exact-work timing records and expected B results. New solver unit tests are not needed for this measurement-only extension.')
m.pop('slurm_job_id', None)
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
                   [*(c / 'pins').iterdir(), c / 'array.sbatch', c / 'matrix.tsv']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
assert sha(c / 'matrix.tsv') == sha(old / 'matrix.tsv')
assert sha(c / 'pins/runtime_task.py') == sha(old / 'pins/runtime_task.py')
print(c)
