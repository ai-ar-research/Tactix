#!/usr/bin/env python3
"""Exclude duplicate frozen context patches from the index, never alter them."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
landing = root / 'benchmarks/reports/2026-09-09-bve-memory-landing.json'
m = json.loads(landing.read_text())
for rel, digest in m['hashes'].items():
    assert sha(root / rel) == digest, rel
duplicates = [f'benchmarks/runs/{name}/pins/candidate-source.patch' for name in
    ('20260908-6148-collect-memory', '20260909-6156-bve-capacity')]
parent = root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50/pins/candidate-source.patch'
for rel in duplicates:
    assert sha(root / rel) == sha(parent), rel
staged = subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines()
assert set(duplicates) <= set(staged)
assert set(staged) <= set(m['hashes']) | {str(landing.relative_to(root))}
subprocess.run(['git', 'restore', '--staged', '--', *duplicates], cwd=root, check=True)
record = root / 'benchmarks/reports/2026-09-09-bve-memory-landing-recovery.json'
assert not record.exists()
helper = Path(__file__).resolve()
record.write_text(json.dumps(dict(original_landing_sha256=sha(landing),
    helper=dict(path=str(helper.relative_to(root)), sha256=sha(helper)),
    unstaged_duplicate_patches={rel: sha(root / rel) for rel in duplicates},
    canonical_copy=dict(path=str(parent.relative_to(root)), sha256=sha(parent)),
    reason='git diff --check rejects blank context lines in newly staged copies of historical unified patches. Keep immutable run-local files and hashes,avoid duplicate staging; never rewrite submitted pins or weaken whitespace checks.',
    deletion=False, source_or_runtime_change=False), indent=2) + '\n')
subprocess.run(['git', 'add', '-f', '--', str(helper.relative_to(root)), str(record.relative_to(root))], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--shortstat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Localize BVE memory peaks and harden SLURM completion identity'], cwd=root, check=True)
owned = (set(m['hashes']) - set(duplicates)) | {str(landing.relative_to(root)), str(record.relative_to(root)), str(helper.relative_to(root))}
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root, text=True)
print('PASS: owned checkpoint clean; frozen duplicate patches remain local and hash-bound,not deleted')
