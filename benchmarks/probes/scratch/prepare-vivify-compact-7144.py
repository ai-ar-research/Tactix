#!/usr/bin/env python3
"""Isolate the approved compact canonical Vivify pilot from frozen7030."""
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

authority = root / 'benchmarks/runs/20260910-7030-collect-retry-build/manifest.json'
manifest = json.loads(authority.read_text())
parent = root / manifest['source']
for rel, digest in manifest['files'].items():
    assert sha(parent / rel) == digest, rel
fixture = root / 'tests/test_vivify_order_contract.cpp'
control = root / 'benchmarks/runs/20260910-7141-vivify-order-contract/summary.json'
validation = json.loads(control.read_text())
assert sha(fixture) == validation['hashes'][str(fixture.relative_to(root))]
source = root / '.claude/worktrees/runtime-vivify-compact-7144'
campaign = root / 'benchmarks/runs/20260910-7144-vivify-compact-build'
assert not source.exists() and not campaign.exists()
inputs = [authority, control, fixture,
    root / 'benchmarks/reports/2026-09-10-bve-prototype-assessment-7135.md',
    root / 'benchmarks/reports/2026-09-10-vivify-schedule-localization-7140.md',
    root / 'benchmarks/reports/2026-09-10-vivify-order-contract-7143.md']
shutil.copytree(parent, source, symlinks=True,
                ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
shutil.copy2(fixture, source / 'tests/test_vivify_order_contract.cpp')
(campaign / 'logs').mkdir(parents=True)
with (campaign / 'parent.json').open('x') as stream:
    json.dump(dict(parent=manifest['source'], source=str(source.relative_to(root)),
        inputs={str(p.relative_to(root)): sha(p) for p in inputs},
        approval='User approved7135/7140 pilot;52460400 removes routine in-scope revamp approval pauses.',
        scope='Compact canonical homogeneous schedules: ordinary irredundant radix ranks, redundant arena poses and ref sorting. Generic const/mixed/nonreuse/prefix-flush paths retained.',
        invariants='Unchanged two-pass root/count timing, pass-time glue membership, priorities, logical ties, pose, work, proofs, public API and policy. No arena pointer survives mutation.',
        gates='At least15% lower complete Vivify Rank35 cycles and1% lower paired whole-runtime medians31/35; no runtime/RSS regression31/35/38; three paired waves; exact work/proofs, full suites, sanitizer/model/proof controls, B28+C5; full50/current-source fuzz/consolidation before promotion.',
        status='Isolated source, not built or promoted'), stream, indent=2)
print(source)
