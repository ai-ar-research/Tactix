#!/usr/bin/env python3
"""Extend the general root-reuse repair after causal and solver prerequisites."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
target = runs / '20260908-5734-root-reuse-ranks1-40'
if target.exists():
    print('Existing campaign; inspect its submission instead of recreating:', target)
    raise SystemExit(0)
trace = runs / '20260908-5726-root-reuse-rank33-prefix/summary.json'
timing = runs / '20260908-5728-root-reuse-rank33-runtime'
gate = runs / '20260908-5727-root-reuse-BC-gate'
if not trace.exists() or not all((timing / 'provisional' / f'{i:03}.json').exists() for i in (0, 1)):
    print('Pending causal summary or first timing pair; no campaign created')
    raise SystemExit(0)
seen = set()
for folder in ('provisional', 'results'):
    for p in (gate / folder).glob('*.json'):
        r = json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code'] in (10, 20)
        assert not r['solve']['timed_out']
        seen.add(r['task'])
if not set(range(56)).issubset(seen):
    print('Pending B28 solver outcomes; no campaign created')
    raise SystemExit(0)

source = Path(__file__).with_name('prepare-sweep-small-full-5535.py').read_text()
changes = {
    '20260908-5485-sweep-cursor-ranks1-40': '20260908-5712-assignment-level-ranks1-40',
    '20260908-5522-sweep-small': '20260908-5718-root-conflict-reuse',
    '20260908-5529-sweep-small-rank32-trace/repair-localization.json': '20260908-5726-root-reuse-rank33-prefix/summary.json',
    '20260908-5531-sweep-small-rank32-runtime': '20260908-5728-root-reuse-rank33-runtime',
    '20260908-5530-sweep-small-BC-gate': '20260908-5727-root-reuse-BC-gate',
    '20260908-5535-sweep-small-ranks1-40': '20260908-5734-root-reuse-ranks1-40',
    'build/pin-5522-sweep-small-release': 'build/pin-5718-root-conflict-reuse-release',
    'build/pin-5524-sweep-small-bc': 'build/pin-5720-root-conflict-reuse-bc',
    'Full unchanged Rank1-40 paired Kissat comparison of retained eligibility and real small-environment Sweep solves on5473.':
        'Full unchanged Rank1-40 paired Kissat comparison of zero-jump conflict reuse on5672.',
    'General Sweep lifecycle repairs close the observed Rank32 decision divergence; measure net full-suite runtime, work and memory effects, including regressions, without multiplying cross-window ratios.':
        'Measure the general root-driving conflict repair across all40 pairs, including regressions; no tuned constants, deadlines or benchmark-specific behavior. Work may change.',
    'Release86/86 twice with matching hashes; trace85/85; old one-clause counterexample,12 native/SubSolver lifecycle cases and budget/state/model tests. UNFUZZED.':
        'Release91/91 twice,trace90/90;288 old/new transition fixtures,24 exact native matches and288 independently checked micro-proofs. Current-source fuzz remains required.',
    'Frozen tested Release5522, observers disabled; same native Kissat, matrix, runner, checkers and resource contract as5485.':
        'Frozen tested Release5718,observers disabled;same native Kissat,matrix,runner,checkers and resource contract as5712.'}
for old, new in changes.items():
    assert old in source, old
    source = source.replace(old, new)
start, end = source.index("comparison = v['decision_comparisons']"), source.index('t = [json.loads')
source = source[:start] + """assert v['native_parent_replay'] and v['earlier_common_literal_digest_equal']
first = v['first_decision_difference']
if first is not None:
    assert min(int(first[arm][0]) for arm in ('candidate', 'kissat')) > 182093
""" + source[end:]
exec(compile(source, __file__, 'exec'))
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal(), job
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted full Rank1-40', job)
