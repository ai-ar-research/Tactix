#!/usr/bin/env python3
"""Prepare isolated5962 BC and full-stream prefix; never alter prior campaigns."""
from pathlib import Path
import re

source = Path(__file__).with_name('prepare-forward-admission-jobs-5836.py')
code = source.read_text()
changes = {
    '20260908-5830-forward-admission': '20260908-5962-bve-active-pop',
    '20260908-5806-forward-unit-BC-gate': '20260908-5906-reduce-root-BC-gate',
    '20260908-5836-forward-admission-BC-gate': '20260908-5968-bve-active-pop-BC-gate',
    '5836-forward-admission-BC': '5968-bve-active-pop-BC',
    "('B', 5830, 'release'), ('C', 5831, 'bc')": "('B', 5962, 'release'), ('C', 5963, 'bc')",
    'pin-{number}-forward-admission': 'pin-{number}-bve-active-pop',
    'isolated C2 admission producer5830': 'isolated inactive BVE-pop repair5962 on retained5900',
    'Native clause-addition-only C2 marks preserve expected verdicts and improve cumulative work':
        'Skipping retired or assigned BVE payloads before the effort limit preserves expected verdicts and avoids duplicate retirement; runtime effect is unknown',
    '20260908-5837-forward-admission-rank32-prefix': '20260908-5969-bve-active-pop-rank32-prefix',
    'pin-5832-forward-admission-trace': 'pin-5964-bve-active-pop-trace',
    'tactix-5837-': 'tactix-5969-',
    '5837-admission-rank32': '5969-active-pop-rank32',
    'Remove root-only C2 admission producer;locate earliest changed work and compare with native. Earlier divergences must be reported,not hidden by the179463 witness.':
        'Remove inactive BVE applications while preserving physical ghost heap operations. Compare all decisions with cached native and retained5900; classify all first-BVE attempts. No assumed trajectory or runtime gain.',
}
for old in changes:
    assert old in code, old
code = re.sub('|'.join(re.escape(k) for k in sorted(changes, key=len, reverse=True)),
              lambda m: changes[m.group()], code)
old = "(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5771-', 'tactix-5969-'))"
new = """runner = (old / 'run.py').read_text().replace('tactix-5771-', 'tactix-5969-')
anchor = \"assert not any(key.endswith('_EVENT_TRACE') for key in env)\"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, \"env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-candidates.tsv'))\\nenv['SAT_BVE_CANDIDATE_CONFLICT'] = '271079'\\n\" + anchor)
compile(runner, 'run.py', 'exec')
(c / 'run.py').write_text(runner)"""
assert code.count(old) == 1
code = code.replace(old, new)
anchor = "m['hashes'] = {str(p.relative_to(c)): sha(p)"
assert code.count(anchor) == 1
code = code.replace(anchor, """parent = runs / '20260908-5907-reduce-root-rank32-prefix/results/candidate.json'
m['retained_parent'] = dict(record=str(parent.relative_to(root)), sha256=sha(parent))
""" + anchor)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
