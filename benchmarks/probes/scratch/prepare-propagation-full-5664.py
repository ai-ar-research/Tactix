#!/usr/bin/env python3
"""Measure all40 pairs after exact replay, B solver outcomes and initial timing."""
from pathlib import Path
source = Path(__file__).with_name('prepare-sweep-small-full-5535.py').read_text()
changes = {
    '20260908-5485-sweep-cursor-ranks1-40': '20260908-5604-factor-support-ranks1-40',
    '20260908-5522-sweep-small': '20260908-5649-propagation-cursor-validated',
    '20260908-5529-sweep-small-rank32-trace/repair-localization.json': '20260908-5650-propagation-cursor-rank33-prefix/summary.json',
    '20260908-5531-sweep-small-rank32-runtime': '20260908-5652-propagation-cursor-rank39-runtime',
    '20260908-5530-sweep-small-BC-gate': '20260908-5651-propagation-cursor-BC-gate',
    '20260908-5535-sweep-small-ranks1-40': '20260908-5664-propagation-cursor-ranks1-40',
    'build/pin-5522-sweep-small-release': 'build/pin-propagation-cursor-repair-5647-release',
    'build/pin-5524-sweep-small-bc': 'build/pin-propagation-cursor-repair-5647-bc',
}
for old, new in changes.items():
    assert old in source
    source = source.replace(old, new)
start, end = source.index("assert v['clean_stops']"), source.index("t = [json.loads")
source = source[:start] + "assert v['parent_full_replay']\n" + source[end:]
source = source.replace("records, checked = {}, 0", """stats = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
assert stats(t[0]) == stats(t[1]) and t[0]['certificate_sha256'] == t[1]['certificate_sha256']
records, checked = {}, 0""")
source = source.replace('Full unchanged Rank1-40 paired Kissat comparison of retained eligibility and real small-environment Sweep solves on5473.',
    'Full unchanged Rank1-40 paired Kissat comparison of Congruence cleanup and propagation unit-cost optimization on5592.')
source = source.replace('General Sweep lifecycle repairs close the observed Rank32 decision divergence; measure net full-suite runtime, work and memory effects, including regressions, without multiplying cross-window ratios.',
    'Measure cumulative full-suite runtime/work/memory against Kissat,including every regression. Initial exact-work Rank39 pair is near-neutral,not a replicated gain;do not substitute it for all40 pairs.')
source = source.replace('Release86/86 twice with matching hashes; trace85/85; old one-clause counterexample,12 native/SubSolver lifecycle cases and budget/state/model tests. UNFUZZED.',
    'Release89/89 twice,trace88/88;224 actual old/new state cases and complete1M per-arm trace/proof replay. Current-source fuzz5657 runs;not yet verified.')
source = source.replace('Frozen tested Release5522, observers disabled; same native Kissat, matrix, runner, checkers and resource contract as5485.',
    'Frozen tested5649 Release,observers disabled;same native Kissat,matrix,runner,checkers and resource contract as5604.')
source = source.replace('Only one changed-work pair, checks pending; replica waves remain required. This is an investigation trigger, not a replicated acceptance claim.',
    'Only one exact-work/proof pair,checks pending;all three pairs remain required. Full run is an investigation measurement,not promotion.')
exec(compile(source, __file__, 'exec'))
import subprocess
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted full Rank1-40', job)
