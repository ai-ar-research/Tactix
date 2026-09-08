#!/usr/bin/env python3
"""Extend a causally validated semantic repair to the unchanged full scoring view."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5485-sweep-cursor-ranks1-40'
authority = runs / '20260908-5522-sweep-small'
trace = runs / '20260908-5529-sweep-small-rank32-trace/repair-localization.json'
timing = runs / '20260908-5531-sweep-small-rank32-runtime'
gate = runs / '20260908-5530-sweep-small-BC-gate'
c = runs / '20260908-5535-sweep-small-ranks1-40'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

a = json.loads((authority / 'manifest.json').read_text())
release = root / 'build/pin-5522-sweep-small-release/tactix'
independent = root / 'build/pin-5524-sweep-small-bc/tactix'
for rel, digest in a['binaries'].items():
    assert sha(root / rel) == digest
assert sha(release) == sha(independent)
for rel, digest in a['patches'].items():
    assert sha(authority / rel) == digest
v = json.loads(trace.read_text())
assert v['clean_stops'] == 2
comparison = v['decision_comparisons']['native']
assert comparison['first_difference'] is None
assert comparison['counts'] == [18593182, 18593182]
t = [json.loads((timing / 'provisional' / f'{i:03}.json').read_text()) for i in (0, 1)]
assert [r['arm'] for r in t] == ['candidate', 'previous']
assert all(r['expected_match'] and r['verdict'] == 'UNSAT' and
           r['solve']['exit_code'] == 20 and not r['solve']['timed_out'] for r in t)
records, checked = {}, 0
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
        records[r['task']] = dict(path=str(p.relative_to(root)), sha256=sha(p))
assert set(range(56)).issubset(records), 'Require all B28 solver results'
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
    purpose='Full unchanged Rank1-40 paired Kissat comparison of retained eligibility and real small-environment Sweep solves on5473.',
    hypothesis='General Sweep lifecycle repairs close the observed Rank32 decision divergence; measure net full-suite runtime, work and memory effects, including regressions, without multiplying cross-window ratios.',
    candidate_validation='Release86/86 twice with matching hashes; trace85/85; old one-clause counterexample,12 native/SubSolver lifecycle cases and budget/state/model tests. UNFUZZED.',
    build_identity='Frozen tested Release5522, observers disabled; same native Kissat, matrix, runner, checkers and resource contract as5485.',
    causal_authority=dict(path=str(trace.relative_to(root)), sha256=sha(trace)),
    timing_authority=dict(campaign=str(timing.relative_to(root)),
        first_pair_seconds=[r['solve']['wall_seconds'] for r in t],
        first_pair_ratio=t[0]['solve']['wall_seconds']/t[1]['solve']['wall_seconds'],
        records=[dict(path=str((timing/'provisional'/f'{i:03}.json').relative_to(root)),
                      sha256=sha(timing/'provisional'/f'{i:03}.json')) for i in (0, 1)],
        limitation='Only one changed-work pair, checks pending; replica waves remain required. This is an investigation trigger, not a replicated acceptance claim.'),
    regression_gate=dict(campaign=str(gate.relative_to(root)), expected_solver_records=len(records),
        checked_clean=checked, records=records,
        limitation='Expected solves permit provisional continuation. Checked full view, B28+C5 and latest-source fuzz remain required before promotion.'),
    test_consideration='Measurement-only extension reuses the unchanged tested runner, matrix and resource contract. Assert source/pin hashes, matching independent Release binaries, causal replay and expected B28 verdicts; no new solver behavior needing additional unit tests.')
m.pop('slurm_job_id', None)
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
                   [*(c / 'pins').iterdir(), c / 'array.sbatch', c / 'matrix.tsv']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
assert sha(c / 'matrix.tsv') == sha(old / 'matrix.tsv')
assert sha(c / 'pins/runtime_task.py') == sha(old / 'pins/runtime_task.py')
print(c)
