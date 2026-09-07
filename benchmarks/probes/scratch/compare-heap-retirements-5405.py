#!/usr/bin/env python3
"""Compare portable retirement rows inside the established common-state window."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5405-rank35-heap-lifecycle'
assert json.loads((c / 'summary.json').read_text())['clean_stops'] == 6
rows = {}
for arm in ('candidate', 'kissat'):
    rows[arm] = [line.split()[:6] for line in (c / 'traces' / ('02-' + arm + '.deact.tsv')).read_text().splitlines()
                 if int(line.split()[1]) >= 11838]
assert rows['candidate'] == rows['kissat'] and len(rows['candidate']) == 42
later = [r for r in rows['candidate'] if int(r[1]) > 13001]
assert later == [['DEACT', '13414', '109797', 'elim=0', 'fixed=1', 'inheap=1']]
result = dict(scope='conflicts 11838 through stopped 13563 endpoint; physical trail offset excluded',
              identical_retirement_rows=42, after_last_common_heap=later,
              next_action='Observe the 13414 root retirement relative to heap pops, backtrack reinsertion and score bumps. Identical retirement rows do not establish identical ordering relative to those operations.',
              all_rows=rows['candidate'])
p = c / 'retirement-summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: all 42 portable retirement rows agree; only 109797 at conflict 13414 follows the last observed common heap')
