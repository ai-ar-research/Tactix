#!/usr/bin/env python3
"""Record exact SLURM identities and land only this checkpoint's owned files."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
p = root / 'benchmarks/reports/2026-09-09-expanded-ranks-audit-6204.json'
audit = json.loads(p.read_text())
target = root / 'benchmarks/reports/2026-09-09-expanded-ranks-landing-6205.json'
assert not target.exists()
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True)
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in audit[group].items(): assert sha(root / rel) == digest, rel
accounting = {}
for name, launch in audit['campaigns'].items():
    cmd = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
           '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    p = subprocess.run(cmd, capture_output=True, text=True, check=True)
    lines = p.stdout.strip().splitlines()
    assert len(lines) == 2
    for task, line in enumerate(lines):
        fields = line.split('|')
        assert fields[:3] == [launch['job_id'] + '_' + str(task), 'COMPLETED', '0:0'], line
        assert fields[3] >= launch['accounting_since'], line
    accounting[name] = dict(command=cmd, stdout=p.stdout)
cmd = ['squeue', '-h', '-j', '1677122,1677050,1676920', '-o', '%i|%T|%M|%N']
p = subprocess.run(cmd, capture_output=True, text=True, check=True)
target.write_text(json.dumps(dict(accounting=accounting, live_owned_queue=dict(command=cmd, stdout=p.stdout),
    live_campaigns={'1677122': 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50',
        '1677050': 'benchmarks/runs/20260909-6171-learned-sort-BC-gate',
        '1676920': 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'},
    audit_sha256=sha(root / 'benchmarks/reports/2026-09-09-expanded-ranks-audit-6204.json'),
    outcome='Rank32 incoming schedule size/position split with equal selected scores; full observer replayPASS. AddedRank50 closes100K,Rank49 firstsplit9274.6107 all50checked,latest42/50 andBC65/66 at frozen6197. All50 ratio1.160932,added10 ratio2.790411. No promotion/push.'), indent=2) + '\n')
owned = sorted(set(audit['owned_hashes']) | {str(target.relative_to(root)),
    'benchmarks/reports/2026-09-09-expanded-ranks-audit-6204.json'})
subprocess.run(['git', 'add', '-f', '--', *owned], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Expose added-rank search losses and Rank32 incoming schedule divergence'], cwd=root, check=True)
assert not subprocess.check_output(['git', 'status', '--porcelain', '--', *owned], cwd=root, text=True)
print('PASS: owned checkpoint clean; no push or source promotion')
