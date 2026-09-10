#!/usr/bin/env python3
"""Land only the hash-audited diagnostic checkpoint; never retry denied push."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
audit = root/'benchmarks/reports/2026-09-09-watermark-followup-audit-6761.json'
a = json.loads(audit.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items():
        assert sha(root/rel) == digest, rel
owned = sorted(set(a['owned_hashes']) | {str(audit.relative_to(root))})
assert str(Path(__file__).relative_to(root)) in owned
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root)
subprocess.run(['git', 'add', '-f', '--', *owned], cwd=root, check=True)
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged and staged <= set(owned)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Audit full50 watermark costs and localize Rank21 search split'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *owned], cwd=root)
print('Owned checkpoint clean. Production unchanged; no push retry.')
