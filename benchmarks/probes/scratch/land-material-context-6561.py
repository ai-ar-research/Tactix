#!/usr/bin/env python3
"""Preserve a frozen unified diff's blank context marker while landing scope."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
audit = root / 'benchmarks/reports/2026-09-09-material-producers-audit-6559.json'
a = json.loads(audit.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items(): assert sha(root / rel) == digest, rel
owned = set(a['owned_hashes']) | {str(audit.relative_to(root))}
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged and staged <= owned
assert not subprocess.check_output(['git', 'diff', '--name-only', '--', *sorted(owned)], cwd=root)
patch = 'benchmarks/runs/20260909-6550-native-material-observer-build/native.patch'
lines = (root / patch).read_text().splitlines()
assert lines[41] == ' ' and lines[42].startswith(' void kissat_event_trace_retry_producer ')
# Require the complete diagnostic to be exactly this one format-mandated
# context marker. No global whitespace setting or source-code exemption.
expected = patch + ':42: trailing whitespace.\n+ \n'
check = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, text=True, capture_output=True)
assert check.returncode == 2 and check.stdout == expected and not check.stderr, check
syntax = subprocess.check_output(['git', 'apply', '--numstat', '--', patch], cwd=root, text=True)
records = {}
for p in sorted((root / 'benchmarks/runs/20260909-6464-activation-adverse-three-wave-runtime/results').glob('*.json')):
    r = json.loads(p.read_text())
    assert classify(r, True)['status'] in ('checked_SAT', 'checked_UNSAT')
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    records[str(p.relative_to(root))] = sha(p)
assert len(records) == 18
target = root / 'benchmarks/reports/2026-09-09-material-producers-landing-6561.json'
assert not target.exists()
with target.open('x') as out:
    json.dump(dict(audit_sha256=sha(audit), helper_sha256=sha(Path(__file__)),
        retained_patch_sha256=sha(root / patch), exact_whitespace_diagnostic=expected,
        unified_diff_syntax=syntax, completed6464_checked_clean=records,
        limitation='One literal space on unified-diff context line42 is required by that artifact format. Original patch/audit/results remain immutable. All other staged whitespace is still checked; no global configuration changed. No push.'), out, indent=2)
    out.write('\n')
owned.update((str(target.relative_to(root)), str(Path(__file__).resolve().relative_to(root))))
subprocess.run(['git', 'add', '-f', '--', str(target.relative_to(root)), str(Path(__file__).resolve().relative_to(root))], cwd=root, check=True)
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= owned
check = subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, text=True, capture_output=True)
assert check.returncode == 2 and check.stdout == expected and not check.stderr
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Localize Rank50 native material invitations with exact observer replay'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root)
print('Owned checkpoint clean; frozen patch context preserved; production unchanged; no push.')
