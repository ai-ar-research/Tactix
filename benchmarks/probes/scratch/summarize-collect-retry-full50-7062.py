#!/usr/bin/env python3
"""Classify every full-view record before producing the inherited fixed50 table."""
from collections import Counter
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260910-7053-collect-retry-ranks1-50'
parent = root / 'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
out = root / 'benchmarks/reports/2026-09-10-collect-retry-full50-7062'
assert not out.exists()
classes = {}; records = {}; hashes = {}
for campaign in (c, parent):
    counts = Counter(); records[campaign.name] = {}
    for task in range(50):
        p = campaign / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = campaign / f'provisional/{task:03d}.json'
        if not p.exists():
            print('Complete solver view pending; no score or output written'); sys.exit(0)
        r = json.loads(p.read_text()); state = classify(r, final)
        assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'), (p, state)
        assert int(r['task']) == task and int(r['rank']) == task + 1 and r['arm'] == 'tactix'
        counts[state['status']] += 1; records[campaign.name][task+1] = r
        hashes[str(p.relative_to(root))] = sha(p)
    classes[campaign.name] = dict(counts)
source = Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code = source.read_text()
for before, after in {
    '20260909-6366-compact-stamp-ranks1-50': c.name,
    '20260909-6320-substitute-collapse-ranks1-50': parent.name,
    '2026-09-09-compact-stamp-full50-6371': out.name,
}.items():
    assert code.count(before) == 1; code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
s = json.loads((out / 'summary.json').read_text())
work = []
for rank in range(1,51):
    a,b = records[c.name][rank], records[parent.name][rank]
    project = lambda r: [line for line in r['solver_statistics']
                         if not line.startswith('c timing') and line.split()[:2] != ['c','touch']]
    work.append(dict(rank=rank, equal_except_timing_and_touch=project(a)==project(b),
                     certificate_equal=a['certificate_sha256']==b['certificate_sha256']))
for p in (Path(__file__), source, root / 'tools/bench/fixed_runtime.py', root / 'tools/bench/checked_comparison.py'):
    hashes[str(p.relative_to(root))] = sha(p)
s.update(pre_score_classification=classes, work_excluding_touch=work)
s['hashes'].update(hashes)
(out / 'summary.json').write_text(json.dumps(s, indent=2) + '\n')
print('More than touch counters changed on ranks:', [r['rank'] for r in work if not r['equal_except_timing_and_touch']])
print('Pre-score classification:', json.dumps(classes))
