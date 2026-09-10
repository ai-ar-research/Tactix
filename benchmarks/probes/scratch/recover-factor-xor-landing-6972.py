#!/usr/bin/env python3
"""Preserve frozen raw diff; emit a whitespace-clean zero-context replay patch."""
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
target = c/'landing-recovery-6972.json'; assert not target.exists()
audit = root/'benchmarks/reports/2026-09-10-factor-xor-cost-audit-6940.json'
a = json.loads(audit.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items(): assert sha(root/rel) == digest,rel
raw = c/'compile-scope.patch'
assert raw.read_text().splitlines()[23] == ' ' and raw.read_text().endswith(' \n')
parent = root/'.claude/worktrees/runtime-reduce-watermark-6730'
source = root/'.claude/worktrees/runtime-ite-pair-hoist-6969'
relative = 'src/rules/congruence/congruence.cpp'
patch = ''.join(difflib.unified_diff((parent/relative).read_text().splitlines(True),
    (source/relative).read_text().splitlines(True),fromfile='a/'+relative,tofile='b/'+relative,n=0))
assert patch and all(not line.endswith((' ','\t')) for line in patch.splitlines())
replay = c/'compile-scope-6972.patch'
with replay.open('x') as out: out.write(patch)
command = ['git','apply','--check','--unidiff-zero','--directory='+str(parent.relative_to(root)),str(replay)]
p = subprocess.run(command,cwd=root,capture_output=True,text=True)
assert p.returncode == 0,p.stderr
paths = [Path(__file__),audit,raw,replay,parent/relative,source/relative]
with target.open('x') as out:
    json.dump(dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},command=command,
        stdout=p.stdout,stderr=p.stderr,exit_code=p.returncode,
        classification='Landing6941 staged audited paths, then unchanged git diff --cached --check refused raw6969 compile-scope.patch line24: a single context space at EOF. No commit, source change or measurement invalidity. Raw frozen file is retained unchanged/hash-bound locally and omitted from the fresh staged set. New6972 zero-context patch replays the identical source edit and passes git apply --check; no whitespace gate is weakened. Only the raw path is unstaged; already staged session-owned paths are preserved for fresh audited landing.'),out,indent=2);out.write('\n')
print('PASS zero-context replay; original raw diff preserved')
