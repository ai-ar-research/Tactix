#!/usr/bin/env python3
"""Identify the single owned prior audit omitted by the6974 staged-set guard."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
p = root/'benchmarks/reports/2026-09-10-factor-xor-landing-audit-6973.json'
a = json.loads(p.read_text())
prior = a['prior_audit']
assert prior['path'] == 'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json'
assert sha(root/prior['path']) == prior['sha256']
staged = set(subprocess.check_output(['git','diff','--cached','--name-only'],cwd=root,text=True).splitlines())
extra = staged - (set(a['owned_hashes']) | {str(p.relative_to(root))})
assert extra == {prior['path']},extra
target = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build/ownership-preflight-6975.json'
with target.open('x') as out:
    json.dump(dict(hashes={str(q.relative_to(root)):sha(q) for q in (Path(__file__),p,root/prior['path'])},
        staged_extra=sorted(extra),classification='6974 refused before git add/commit because frozen prior audit6940 remained staged from6941 but was omitted from6973 owned_hashes. Exact prior_audit SHA proves it is checkpoint-owned, not unrelated user work. No mutation occurred in failed6974. Fresh6976 explicitly owns6940 and6973; fresh6977 retains the strict staged-subset and whitespace gates. No solver test is warranted for this ownership-only correction.'),out,indent=2);out.write('\n')
print('PASS identified sole staged extra as hash-bound prior audit6940')
