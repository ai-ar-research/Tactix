#!/usr/bin/env python3
"""Land an audited checkpoint without waiting for the live follow-up/checkers."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
audit_path = root / 'benchmarks/reports/2026-09-09-membership-admission-audit-6220.json'
a = json.loads(audit_path.read_text())
target = root / 'benchmarks/reports/2026-09-09-membership-admission-landing-6221.json'
assert not target.exists()
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True)
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in a[group].items(): assert sha(root / rel) == digest, rel
accounting = {}
for name, launch in a['campaigns'].items():
    if '6217' in name: continue
    cmd = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
           '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    p = subprocess.run(cmd, capture_output=True, text=True, check=True)
    lines = p.stdout.strip().splitlines(); assert len(lines) == 2
    for i, line in enumerate(lines):
        fields = line.split('|')
        assert fields[:3] == [launch['job_id'] + '_' + str(i), 'COMPLETED', '0:0']
        assert fields[3] >= launch['accounting_since']
    accounting[name] = dict(command=cmd, stdout=p.stdout)
cmd = ['squeue', '-h', '-j', '1677187,1677122,1676920', '-o', '%i|%T|%M|%N']
p = subprocess.run(cmd, capture_output=True, text=True, check=True)
target.write_text(json.dumps(dict(audit_sha256=sha(audit_path), terminal_accounting=accounting,
    live_queue=dict(command=cmd, stdout=p.stdout),
    next='Validate6217 allparentstreams/proofs and appendedselectedrows;thenidentify337674 retryproducer. Rank50 nextcapturealladmission inputs at823562 beforechangingwatermark policy. Keepfull50/sourcefuzz/memory gatesopen.'), indent=2) + '\n')
owned = sorted(set(a['owned_hashes']) | {str(audit_path.relative_to(root)), str(target.relative_to(root))})
subprocess.run(['git', 'add', '-f', '--', *owned], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Resolve Rank32 schedule membership and localize late Rank50 admission'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *owned], cwd=root, text=True)
print('PASS: local owned checkpoint clean;no push or production change')
