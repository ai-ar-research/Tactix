#!/usr/bin/env python3
"""Isolate a layout-only ablation while preserving the historical watermark."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
authority = root/'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
m = json.loads(authority.read_text())
parent = root/m['source']
for rel, digest in m['files'].items():
    assert sha(parent/rel) == digest, rel
evidence = [root/'benchmarks/runs/20260909-6754-rank38-watermark-cost-runtime/solver-checkpoint-6756.json',
    root/'benchmarks/runs/20260909-6760-watermark-layout/summary.json',
    root/'benchmarks/reports/2026-09-09-watermark-followup-audit-6761.json']
for p in evidence[:2]:
    s = json.loads(p.read_text())
    for rel, digest in s['hashes'].items():
        assert sha(root/rel) == digest, rel
for name in ('20260909-6737-reduce-watermark-BC-gate', '20260909-6743-reduce-watermark-ranks1-50',
             '20260909-6754-rank38-watermark-cost-runtime'):
    for folder in ('provisional', 'results'):
        for p in (root/'benchmarks/runs'/name/folder).glob('*.json'):
            status = classify(json.loads(p.read_text()), folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
source = root/'.claude/worktrees/runtime-watermark-layout-6763'
c = root/'benchmarks/runs/20260909-6763-watermark-layout-build'
assert not source.exists() and not c.exists()
shutil.copytree(parent, source, symlinks=True, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
(c/'logs').mkdir(parents=True)
with (c/'parent.json').open('x') as out:
    json.dump(dict(path=str(authority.relative_to(root)), sha256=sha(authority),
        evidence={str(p.relative_to(root)):sha(p) for p in evidence},
        scope='Move the unchanged four-byte historical Reduce marker into existing padding after the watch-connection flag. Restore old watched-search member offsets; no initializer, field use, guard, instance, proof, collector, scheduling or production change.',
        hypothesis='Three exact-work pairs show a5.89% Rank38 cost increase with nearly identical instructions and6.06% more cycles. Compiled offsets show an8-byte watch-and-later shift. Restore offsets without removing marker behavior, then require exact trajectory/proof and matched timing to discriminate layout cost from other code-generation effects.',
        test_consideration='No new semantic behavior. Existing288 watermark states and all source suites cover constructor, promotion/retirement, aliasing, genuine versus implementation-only packing, public surface and repeated Reduce. Add an independently compiled member-offset regression requiring every old field restored and the new4-byte marker wholly inside the former flag-to-pointer padding. Keep trace-tail ABI test in full suites.',
        acceptance='Independent Release/BC/trace suites and matching release hashes, fixed layout regression, B28+C5 against6730 with exact work/proofs, then Rank38 and Rank46 exact-work/proof timing. Require three matched waves and retain all adverse outcomes. No promotion without required fuzz; failed cost hypothesis means no layout-causality claim.'), out, indent=2)
    out.write('\n')
print(source)
