#!/usr/bin/env python3
"""Bind the validated profile priorities and rejected heap-repair premise."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from inprocessing_cost import read_timers

c = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles'
target = c / 'cost-priority-7010.json'
assert not target.exists()
p = c / 'solver-summary-7004.json'
s = json.loads(p.read_text())
hashes = {str(p.relative_to(root)): sha(p)}
for rel, digest in s['hashes'].items():
    assert sha(root / rel) == digest, rel
rows = {}
for key, arm in s['arms'].items():
    p = root / arm['record']
    r = json.loads(p.read_text())
    hashes[str(p.relative_to(root))] = sha(p)
    rows[key] = dict(work=arm['work'], diagnostic_seconds=arm['diagnostic_seconds'],
                     self_samples=arm['self_samples'][:16])
    if arm['arm'] == 'candidate':
        rows[key]['timers'] = read_timers(r['solver_statistics'])
old_manifest = root / 'benchmarks/runs/20260908-5870-forward-preclean/manifest.json'
current_manifest = root / 'benchmarks/runs/20260909-6730-reduce-watermark-build/manifest.json'
parts = []
for manifest in (old_manifest, current_manifest):
    m = json.loads(manifest.read_text())
    path = root / m['source'] / 'src/policies/heuristics.cpp'
    expected = m['files'].get('src/policies/heuristics.cpp')
    if expected is not None:
        assert sha(path) == expected
    else:
        # Early5870 records changed files only, not a complete source manifest.
        # Bind retained text now; do not claim historical executable identity.
        patch = manifest.parent / 'cumulative.patch'
        assert sha(patch) == m['patches']['cumulative.patch']
        hashes[str(patch.relative_to(root))] = sha(patch)
    hashes[str(manifest.relative_to(root))] = sha(manifest)
    hashes[str(path.relative_to(root))] = sha(path)
    body = path.read_text()
    parts.append(body[body.index('static void heap_bubble_up('):body.index('void heuristics_update_target_phases(')])
assert parts[0] == parts[1]
heap = dict(start='static void heap_bubble_up(', end='void heuristics_update_target_phases(',
            bytes=len(parts[0].encode()), sha256=hashlib.sha256(parts[0].encode()).hexdigest(),
            binding='Current6730 file is manifest-bound. Old5870 retained text is observed/hash-bound now; its partial manifest and cumulative patch are bound, but historical full-file reconstruction is not claimed.',
            implication='The existing later stamp changes are not a direct repair to heap maintenance/stable-entry insertion. No new justification to restore rejected preclean established; indirect formula effects remain possible, not proven.')
for path in (Path(__file__), Path(__file__).with_name('summarize-largest-gap-cost-7009.py'), root / 'tools/bench/inprocessing_cost.py',
             root / 'benchmarks/runs/20260908-5910-preclean-rank31-choice/choice-validation.json',
             root / 'benchmarks/runs/20260909-6637-rank35-binary-life/summary-6642.json'):
    hashes[str(path.relative_to(root))] = sha(path)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
           rows=rows, heap_source_comparison=heap,
           preparation_failure='Unsubmitted7009 summary writer assumed5870 files covered every source; its changed-file-only manifest raised KeyError before any output write. Failed helper preserved. Fresh7010 distinguishes observed old text from current full-manifest identity.',
           decision='Search propagation dominates both selected current profiles. Prioritize an excess-work causal screen: TransitiveReduce on/off in both solvers on31/35, retaining native and current on controls. No source/default edit or small-loop speedup claim.',
           limitation='Self samples are exclusive but differently scoped symbols and work cannot be priced as equal operations. Inclusive callpath ancestors overlap. Profile wrapper replaces solver perf-stat, so no solver IPC; perf itself can dominate recorded peak RSS. Use fixed unprofiled records for memory. Checks pending at7004 snapshot permit investigation only.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
