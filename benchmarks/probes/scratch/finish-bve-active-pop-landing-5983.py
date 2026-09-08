#!/usr/bin/env python3
"""Keep exact checker output and inherited generated fixture bytes at landing."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
landing = root / 'benchmarks/reports/2026-09-08-bve-active-pop-landing.json'
m = json.loads(landing.read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
for rel, digest in m['hashes'].items(): assert sha(root / rel) == digest, rel
for rel, digest in m['andrew_report'].items(): assert sha(root / rel) == digest
r = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, capture_output=True, text=True)
assert r.returncode == 2
exempt = {}
for line in r.stdout.splitlines():
    match = re.fullmatch(r'(.+):(\d+): (trailing whitespace\.| new blank line at EOF\.|new blank line at EOF\.)', line)
    if not match:
        assert line.startswith('+'), line
        continue
    rel, lineno, reason = match[1], int(match[2]), match[3]
    assert rel in m['hashes']
    content = (root / rel).read_text()
    if rel.endswith('.log'):
        assert rel.startswith(('benchmarks/runs/20260908-5962-bve-active-pop/',
                               'benchmarks/runs/20260908-5966-bve-active-pop-edges/'))
        assert lineno == 8 and reason == 'trailing whitespace.'
        assert re.fullmatch(r'c \d+ of \d+ clauses in core +', content.splitlines()[7])
    else:
        assert rel == 'benchmarks/runs/20260908-5966-bve-active-pop-edges/forward-fixture.hpp'
        assert lineno == 65 and reason == 'new blank line at EOF.' and content.endswith('\n\n')
    exempt[rel] = m['hashes'][rel]
assert len(exempt) == 28
staged = subprocess.check_output(['git', 'diff', '--cached', '--name-only', '-z'], cwd=root).decode().rstrip('\0').split('\0')
assert set(staged) == set(m['hashes']) | {str(landing.relative_to(root))}
subprocess.run(['git', 'diff', '--cached', '--check', '--', *[p for p in staged if p not in exempt]], cwd=root, check=True)
target = root / 'benchmarks/reports/2026-09-08-bve-active-pop-landing-data.json'
assert not target.exists()
target.write_text(json.dumps(dict(preserved_data_hashes=exempt,
    reason='27 exact drat-trim stdout logs have padded core-summary lines;one inherited generated edge fixture has a final blank line. All bytes match the frozen landing authority. Every other staged file passes whitespace checking. No evidence rewrite or repository-wide checker change.',
    landing_sha256=sha(landing)), indent=2)+'\n')
subprocess.run(['git', 'add', '-f', '--', str(target.relative_to(root)), str(Path(__file__).relative_to(root))], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check', '--', str(target.relative_to(root)), str(Path(__file__).relative_to(root))], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Repair inactive BVE pops and trace earlier heap topology'], cwd=root, check=True)
print('PASS: exact frozen checker/fixture data preserved;local commit,no push')
