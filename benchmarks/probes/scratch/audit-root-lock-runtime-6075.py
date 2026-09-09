#!/usr/bin/env python3
"""Fresh normal-job/full-runtime checkpoint; never rewrite the repair audit."""
from pathlib import Path
source = Path(__file__).with_name('audit-root-lock-6067.py')
code = source.read_text()
for old, new in {
    '2026-09-08-root-reason-repair-validation.json': '2026-09-08-root-lock-runtime-validation.json',
    '2026-09-08-root-reason-repair.md': '2026-09-08-root-lock-runtime.md',
    "jobs = '1673402,1676660,1676661,1676727'": "jobs = '1673402,1676660,1676661,1676727,1676768,1676769'",
    "'-j', '1676660,1676661,1676727'": "'-j', '1676660,1676661,1676727,1676768,1676769'",
    'General6049 root-reason repair:62 checked fixed proofs,four suites,18 old/new cases,3 native cases;6054 observer98/98.6060 closes55 reductions and69828932 decisions through300K. BC66 expected;full6061 submitted.':
        'Current6049 full6061 has40 expected solves,7443.116s/6810.066s=1.092957983x;goal unmet. Rank37 largest loss;6070 matches77120545 decisions then splits272979/76953349.6076 captures missing heap/choice fields. No new solver change,promotion or isolated unit-cost claim.',
}.items():
    assert old in code, old
    code = code.replace(old, new)
extra = '''
c = run_dir(6061)
for name in ('solver-checkpoint.json', 'next-losses.json', 'work-cost-summary.json'):
    bind(c / name)
s = json.loads((c / 'solver-checkpoint.json').read_text())
assert s['records'] == s['expected_solves'] == 40 and not s['runtime_target_met']
assert abs(s['ratio'] - 1.0929579830797531) < 1e-12
c = run_dir(6070)
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
for rel, digest in m['diagnostic_source_hashes'].items():
    assert sha(root / m['diagnostic_source'] / rel) == digest, rel
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
if (c / 'results/candidate.json').exists():
    r = json.loads((c / 'results/candidate.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    bind(c / 'results/candidate.json')
for name in ('summary.json', 'boundary-localization.json'):
    if (c / name).exists():
        bind(c / name)
c = run_dir(6076)
m = json.loads((c / 'manifest.json').read_text())
for rel, digest in m['hashes'].items():
    assert sha(c / rel) == digest, rel
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
for p in (c / 'results').glob('*.json'):
    r = json.loads(p.read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    bind(p)
'''
assert code.count('checks = {}') == 1
code = code.replace('checks = {}', extra + '\nchecks = {}')
anchor = 'for label, command in commands:'
assert code.count(anchor) == 1
code = code.replace(anchor, "commands.append(('rank37-boundary-tests', ['python3', 'benchmarks/probes/scratch/localize-root-lock-rank37-6073.py', '--self-test']))\n" + anchor)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
