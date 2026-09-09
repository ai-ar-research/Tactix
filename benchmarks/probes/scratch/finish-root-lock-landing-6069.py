#!/usr/bin/env python3
"""Preserve verbatim checker logs while enforcing whitespace on authored files."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
landing = root / 'benchmarks/reports/2026-09-08-root-reason-repair-landing.json'
target = root / 'benchmarks/reports/2026-09-08-root-reason-repair-landing-completion.json'
assert not target.exists()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
record = json.loads(landing.read_text())
for rel, digest in record['hashes'].items():
    assert sha(root / rel) == digest, rel
for rel, digest in record['andrew_report'].items():
    assert sha(root / rel) == digest, rel
p = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, capture_output=True, text=True)
assert p.returncode == 2 and not p.stderr
lines = p.stdout.splitlines()
assert len(lines) % 2 == 0 and lines
allowed = []
for index in range(0, len(lines), 2):
    match = re.fullmatch(r'(benchmarks/runs/20260908-6049-reduce-root-lock/[^:]+-drat\.log):8: trailing whitespace\.', lines[index])
    assert match, lines[index]
    assert re.fullmatch(r'\+c \d+ of \d+ clauses in core +', lines[index + 1]), lines[index + 1]
    rel = match[1]
    assert rel in record['hashes'] and sha(root / rel) == record['hashes'][rel]
    allowed.append(rel)
assert len(allowed) == len(set(allowed)) == 25
command = ['git', 'diff', '--cached', '--check', '--', '.', *[':(exclude)' + rel for rel in allowed]]
subprocess.run(command, cwd=root, check=True)
target.write_text(json.dumps(dict(landing_sha256=sha(landing), raw_checker_whitespace=allowed,
    disposition='6068 stopped before commit solely on25 verbatim DRAT checker logs with padded core-count lines. All frozen hashes verified; no logs edited or deleted. Exact25 files exempt only from whitespace lint, all other staged files pass unchanged check.',
    test_consideration='Require exact file scope,line8,core-count syntax,count25 and bound hashes before exemption; no solver,proof,documentation or semantic validator relaxed.',
    checks={'non_checker_log_whitespace': 'PASS'}, no_push=True), indent=2) + '\n')
subprocess.run(['git', 'add', '-f', '--', str(target.relative_to(root)), str(Path(__file__).resolve().relative_to(root))], cwd=root, check=True)
subprocess.run(command, cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Repair root-reason reduction locks and close Rank32 causal prefix'], cwd=root, check=True)
print('PASS: committed unchanged checker evidence and validated authored files; no push')
