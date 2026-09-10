#!/usr/bin/env python3
"""Complete only the audited owned index after preserving raw disassembly."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
audit = root/'benchmarks/reports/2026-09-09-watermark-layout-landing-audit-6785.json'
a = json.loads(audit.read_text())
for group in ('owned_hashes', 'retained_raw_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items():
        assert sha(root/rel) == digest, rel
owned = set(a['owned_hashes']) | {str(audit.relative_to(root))}
raw = set(a['retained_raw_hashes'])
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= owned | raw
assert not subprocess.check_output(['git', 'ls-tree', '--name-only', 'HEAD', '--', *sorted(raw)], cwd=root)
subprocess.run(['git', 'restore', '--staged', '--', *sorted(raw)], cwd=root, check=True)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged and staged <= owned and not staged & raw
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Test watermark layout cost and repair bounded Rank21 diagnostics'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root)
for rel, digest in a['retained_raw_hashes'].items():
    assert sha(root/rel) == digest
print('Owned checkpoint clean. Raw disassembly retained unchanged locally. Production unchanged; no push retry.')
