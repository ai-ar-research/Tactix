#!/usr/bin/env python3
"""Retain immutable raw perf output locally; land only reviewable checkpoint files."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
audit = root/'benchmarks/reports/2026-09-10-inprocessing-cost-priority-audit-6818.json'
receipt = root/'benchmarks/reports/2026-09-10-inprocessing-local-profiles-6821.json'
assert not receipt.exists()
a = json.loads(audit.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items():
        if rel != 'PROGRESS.md': assert sha(root/rel) == digest, rel
raw_prefix = 'benchmarks/runs/20260910-6815-inprocessing-release-profiles/profiles/'
raw = sorted(rel for rel in a['owned_hashes'] if rel.startswith(raw_prefix) and rel.endswith('.txt'))
assert len(raw) == 8
prior_owned = set(a['owned_hashes']) | {str(audit.relative_to(root))}
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= prior_owned and set(raw) <= staged
assert not subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', 'HEAD', '--', *raw], cwd=root)
# Index-only removal of these new raw files; original bytes stay untouched.
subprocess.run(['git', 'restore', '--staged', '--', *raw], cwd=root, check=True)
owned = (prior_owned - set(raw)) | {str(Path(__file__).relative_to(root)), 'PROGRESS.md'}
checks = {}
for name in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations'):
    command = ['python3', 'tools/docs/'+name+'.py']
    r = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(stdout=r.stdout, stderr=r.stderr)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    audit=dict(path=str(audit.relative_to(root)), sha256=sha(audit)),
    correction='Landing6819 stopped at git diff --check: native perf reports contain emitted trailing whitespace. No commit made. Eight originals remain byte-identical and hash-bound locally; no report edits, data deletion or whitespace-gate weakening. PROGRESS records this correction.',
    local_raw_profiles={rel:dict(sha256=sha(root/rel), bytes=(root/rel).stat().st_size) for rel in raw},
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)}, checks=checks,
    limits='No solver change, promotion, denied push or transfer retry; audit6818 owns certificate snapshot and tests. Andrew0032 remains local/unsent. Deadline unchanged.')
with receipt.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
owned.add(str(receipt.relative_to(root)))
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= owned and not staged.intersection(raw)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Profile inprocessing costs with scheduling and CDCL parity safeguards'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root)
for rel, record in out['local_raw_profiles'].items(): assert sha(root/rel) == record['sha256']
print('Owned checkpoint clean; eight raw profiles retained unchanged locally. No push retry.')
