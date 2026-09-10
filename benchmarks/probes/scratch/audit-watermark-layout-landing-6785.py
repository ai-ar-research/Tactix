#!/usr/bin/env python3
"""Preserve frozen objdump bytes locally while keeping the Git whitespace gate."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
prior_path = root/'benchmarks/reports/2026-09-09-watermark-layout-audit-6779.json'
target = root/'benchmarks/reports/2026-09-09-watermark-layout-landing-audit-6785.json'
assert not target.exists()
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root/rel) == digest, rel
raw = {f'benchmarks/runs/20260909-6777-watermark-propagate-code/{name}.asm'
       for name in ('before', 'watermark', 'layout')}
assert raw <= set(prior['owned_hashes'])
owned = (set(prior['owned_hashes']) - raw) | {
    str(prior_path.relative_to(root)), str(Path(__file__).relative_to(root)),
    'benchmarks/probes/scratch/land-watermark-layout-6786.py'}
staged = set(subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=root, text=True).splitlines())
assert staged <= set(prior['owned_hashes']) | {str(prior_path.relative_to(root))}
assert raw <= staged
checks = {}
for name in ('check-doc-size', 'check-progress', 'check-citations'):
    command = ['python3', 'tools/docs/'+name+'.py']
    result = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
    print(name, 'PASS', flush=True)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    retained_raw_hashes={rel:sha(root/rel) for rel in sorted(raw)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], andrew_report=prior['andrew_report'],
    evidence=prior['evidence'], campaign_states=prior['campaign_states'], diagnostics=prior['diagnostics'],
    checks=checks,
    landing_failure='6780 stopped before commit: git diff --cached --check rejected objdump-emitted trailing spaces on retq lines in three raw .asm files. No compiler or solver failure.',
    disposition='Preserve raw bytes locally and hash-bound in frozen6777/6778/6779 and this audit. Remove only these three paths from the index; do not alter/delete them or weaken whitespace checks. All submitted pins, reports and successful writers remain frozen.',
    test_consideration='No implementation change. Prior6779 binds219 benchmark/19 watcher tests and all six documentation checks; rerun status-related checks after the resume-line amendment. Explicit hash/index assertions cover the landing-only adjustment.',
    limitations=prior['limitations'])
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(target)
