#!/usr/bin/env python3
"""Preserve frozen context patches locally; stage equivalent zero-context evidence."""
import difflib
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
prior=root/'benchmarks/reports/2026-09-09-collapse-outcome-audit-6335.json'
a=json.loads(prior.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items():assert sha(root/rel)==digest,rel
build=root/'benchmarks/runs/20260909-6327-collapse-schedule-build'
p=json.loads((build/'parent.json').read_text())
s=json.loads((build/'summary.json').read_text())
for rel in s['changes']:
    before=root/p['parent']/rel;after=root/p['source']/rel
    original=build/(Path(rel).name+'.patch')
    expected=''.join(difflib.unified_diff(before.read_text().splitlines(True),after.read_text().splitlines(True),fromfile='a/'+rel,tofile='b/'+rel))
    assert original.read_text()==expected
    binding=str(original.relative_to(root));assert binding in a['owned_hashes']
    a['local_hash_bound'][binding]=dict(bytes=original.stat().st_size,sha256=a['owned_hashes'].pop(binding))
    target=build/(Path(rel).name+'-unidiff-zero.patch');assert not target.exists()
    patch=''.join(difflib.unified_diff(before.read_text().splitlines(True),after.read_text().splitlines(True),fromfile='a/'+rel,tofile='b/'+rel,n=0))
    assert all(line.rstrip()==line for line in patch.splitlines())
    target.write_text(patch);a['owned_hashes'][str(target.relative_to(root))]=sha(target)
for q in (prior,Path(__file__),Path(__file__).with_name('land-collapse-clean-6338.py')):
    a['owned_hashes'][str(q.relative_to(root))]=sha(q)
a['patch_encoding']='Initial6336 landing stopped at git diff --cached --check: a blank context line in frozen test_event_trace.cpp.patch is whitespace in Git. No source whitespace or solver failure. Frozen context patches remain unchanged and hash-bound locally; equivalent zero-context patches generated from the same bound source pair. Apply with git apply --unidiff-zero. No semantic change or benchmark rerun.'
target=root/'benchmarks/reports/2026-09-09-collapse-clean-audit-6337.json'
assert not target.exists();target.write_text(json.dumps(a,indent=2)+'\n');print(target)
