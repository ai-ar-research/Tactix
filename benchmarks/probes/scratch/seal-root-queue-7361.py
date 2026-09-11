#!/usr/bin/env python3
"""Bind the final BC completion handoff without rewriting frozen audit7360."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-11-root-queue-finalstatus-7361.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-11-root-queue-audit-7360.json'
prior = json.loads(prior_path.read_text())
for rel, digest in prior['hashes'].items():
    if rel != 'PROGRESS.md': assert sha(root / rel) == digest, rel
assert prior['campaigns']['20260911-7347-root-storage-BC']['checked'] == 66
assert prior['scheduler']['queue']['stdout'] == ''
checks = {}
for name in ('check-doc-size', 'check-progress'):
    result = subprocess.run([sys.executable, 'tools/docs/' + name + '.py'], cwd=root, text=True, capture_output=True)
    assert result.returncode == 0, (result.stdout, result.stderr)
    checks[name] = dict(exit_code=0, stdout=result.stdout, stderr=result.stderr)
paths = [prior_path, root / 'PROGRESS.md', Path(__file__)]
with target.open('x') as out:
    json.dump(dict(hashes={str(p.relative_to(root)): sha(p) for p in paths}, checks=checks,
        prior_authority=str(prior_path.relative_to(root)),
        status='BC7347 all66 checked-clean; both new probes all4 checked-clean; full7312 all50 checked-clean. No campaign solver/checker remains pending in7360 queue snapshot.',
        debug='Only full Debug7342/session72197 remains running its final demo;122 prior passes, not a complete-suite pass.',
        note='Andrew and technical report retain their65/66 drafting snapshot; audit7360 records66/66. This seal updates living PROGRESS without changing prior source/evidence. No production promotion, push or goal completion.'), out, indent=2)
print(target)
