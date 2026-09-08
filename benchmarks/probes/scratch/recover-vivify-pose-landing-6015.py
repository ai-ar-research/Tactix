#!/usr/bin/env python3
"""Preserve hash-bound unified-diff context lines; check every source normally."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
landing = root / 'benchmarks/reports/2026-09-08-vivify-pose-landing.json'
target = root / 'benchmarks/reports/2026-09-08-vivify-pose-landing-recovery.json'
assert not target.exists()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads(landing.read_text())
for rel, digest in m['hashes'].items(): assert sha(root / rel) == digest, rel
for rel, digest in m['andrew_report'].items(): assert sha(root / rel) == digest, rel
exemptions = {
    'benchmarks/runs/20260908-6008-clause-pose-series/kissat-observer.patch': [6],
    'benchmarks/runs/20260908-6008-clause-pose-series/tactix-observer.patch': [4, 6, 50],
}
for rel, lines in exemptions.items():
    content = (root / rel).read_text().splitlines()
    assert [i + 1 for i, line in enumerate(content) if line != line.rstrip()] == lines
    assert all(content[i - 1] == ' ' for i in lines)
    # The one space is the required context-line prefix, not an added source line.
    assert content[0].startswith('--- a/') and content[1].startswith('+++ b/')
    r = subprocess.run(['git', 'diff', '--cached', '--check', '--', rel], cwd=root, capture_output=True, text=True)
    assert r.returncode == 2
    assert r.stdout.count('trailing whitespace.') == len(lines), r.stdout
command = ['git', 'diff', '--cached', '--check', '--', '.', *(':(exclude)' + p for p in exemptions)]
subprocess.run(command, cwd=root, check=True)
target.write_text(json.dumps(dict(landing_sha256=sha(landing), exact_artifact_exemptions=exemptions,
    reason='Four single-space lines encode blank context in frozen unified diffs. They are not trailing whitespace in source changes. Preserve submitted manifest hashes; all other staged files pass the ordinary whitespace check.',
    frozen_hashes_verified=True, source_whitespace_check=command), indent=2) + '\n')
subprocess.run(['git', 'add', '-f', '--', str(target.relative_to(root)), str(Path(__file__).relative_to(root))], cwd=root, check=True)
subprocess.run(command, cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Localize BVE clause-order drift to the second Vivify call'], cwd=root, check=True)
print('PASS: exact frozen-patch context exception; all source checks pass; local commit, no push')
