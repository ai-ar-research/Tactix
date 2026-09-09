#!/usr/bin/env python3
"""Keep the tested original fixture frozen; land a whitespace-only successor."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/probes/scratch/test-native-schedule-continue-6214.c'
new = root / 'benchmarks/probes/scratch/test-native-schedule-continue-6222.c'
assert new.read_text() == old.read_text().rstrip() + '\n'
build = root / 'benchmarks/runs/20260909-6206-schedule-continue-build'
assert sha(old) == json.loads((build / 'summary.json').read_text())['fixture_sha256']
native = root / 'tools/solvers/variants/kissat-schedule-continue-6206'
binary = build / 'native-final-fixture-6222'
assert not binary.exists()
subprocess.run(['gcc', '-O2', '-I' + str(native / 'src'), str(new),
                str(native / 'build/libkissat.a'), '-lm', '-o', str(binary)], check=True)
result = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
assert result.stdout.startswith('PASS: native stopped/continued empty snapshots')
audit = root / 'benchmarks/reports/2026-09-09-membership-admission-audit-6220.json'
a = json.loads(audit.read_text())
owned = set(a['owned_hashes']) | {str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-09-membership-admission-landing-6221.json'}
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= owned
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items(): assert sha(root / rel) == digest, rel
old_rel = str(old.relative_to(root))
subprocess.run(['git', 'restore', '--staged', '--', old_rel], cwd=root, check=True)
owned.remove(old_rel)
record = root / 'benchmarks/reports/2026-09-09-membership-landing-recovery-6223.json'
assert not record.exists()
record.write_text(json.dumps(dict(original_fixture=dict(path=old_rel, sha256=sha(old), kept_local=True),
    successor=dict(path=str(new.relative_to(root)), sha256=sha(new)), test_stdout=result.stdout,
    reason='Initial landing failed only Git whitespace check:6214 has an extra blank EOF line. Original tested/hash-bound bytes retained locally unchanged.6222 removes only trailing blank whitespace,then recompiles and passes the same actual native snapshot fixture. No solver source, run, report or manifest was changed.',
    previous_landing_sha256=sha(root / 'benchmarks/reports/2026-09-09-membership-admission-landing-6221.json'),
    cleanup='No files deleted in recovery; original fixture remains local/hash-bound. No push or promotion.'), indent=2) + '\n')
owned.update((str(new.relative_to(root)), str(record.relative_to(root)), str(Path(__file__).relative_to(root))))
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Resolve Rank32 schedule membership and localize late Rank50 admission'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root, text=True)
print('PASS: clean local owned checkpoint; original fixture retained; no push')
