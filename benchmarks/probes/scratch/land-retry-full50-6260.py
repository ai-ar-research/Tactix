#!/usr/bin/env python3
"""Land only audited owned evidence; no push authorization is assumed."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
path = root / 'benchmarks/reports/2026-09-09-retry-full50-audit-6259.json'
a = json.loads(path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items(): assert sha(root / rel) == digest, rel
owned = set(a['owned_hashes']) | {str(path.relative_to(root))}
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= owned
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Record full Rank 1-50 repair outcome and remaining retry omissions'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root)
print('Owned checkpoint clean; production unchanged; no push.')
