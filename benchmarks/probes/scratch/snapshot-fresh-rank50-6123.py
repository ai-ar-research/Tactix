#!/usr/bin/env python3
"""Freeze a complete-membership, explicitly interim paired comparison table."""
import csv
import datetime as dt
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import checked_comparison as cc
from fixed_runtime import sha
c=root/'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
launch=json.loads((c/'launch.json').read_text());job=launch['job_id']
target=root/'benchmarks/reports/2026-09-08-fresh-rank50-6123'
assert not target.exists()
cmd=['sacct','-n','-X','--array','-j',job,'-o','JobID,State,ExitCode','-P']
p=subprocess.run(cmd,capture_output=True,text=True,check=True)
states={}
for line in p.stdout.splitlines():
    fields=line.split('|')
    match=re.fullmatch(re.escape(job)+r'_(\d+)',fields[0].strip())
    assert match and len(fields)>=3,line
    task=int(match[1]);assert task not in states
    states[task]=fields[1]
assert len(states)==100,states
summary=cc.load(c,states)
summary.update(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    phase='interim;not a final accepted reevaluation',
    accounting=dict(command=cmd,stdout=p.stdout),
    test_consideration='Nine new classification tests plus24 existing benchmark tests. Initially absent SLURM state caused an IndexError;fixed before use and covered by no-state tests. Reject empty/partial matrices,oracle/identity/cleanup errors;retain clean timeout penalties and phase-qualified resource failures.')
target.mkdir()
(target/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
(target/'states.json').write_text(json.dumps(states,indent=2)+'\n')
rows=[]
for pair in summary['pairs']:
    t,k=pair['tactix'],pair['kissat']
    rows.append(dict(rank=pair['rank'],expected=pair['expected'],instance=pair['instance'],
        tactix_status=t['status'],kissat_status=k['status'],tactix_seconds=t['seconds'],kissat_seconds=k['seconds'],
        checked_pair_ratio=pair['ratio'],tactix_peak_rss_kib=t['peak_rss_kib'],kissat_peak_rss_kib=k['peak_rss_kib'],
        memory_excess_kib=pair['memory_excess_kib'],tactix_solve_loss=pair['solve_loss'],
        unresolved_tactix_vs_checked_kissat=pair['unresolved_tactix_vs_checked_kissat']))
with (target/'comparison.tsv').open('x',newline='') as stream:
    w=csv.DictWriter(stream,fieldnames=rows[0],delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
lines=['# Fresh accepted-binary Rank1–50 comparison (interim)','',
       'Every rank is retained. Blank timings mean no solver record at this snapshot. Ratios require both certificates checked and cleanup verified; no partial aggregate is reported.','',
       '| Rank | Verdict | Tactix s | Kissat s | Checked ratio | Tactix status | Kissat status |',
       '|---|---|---:|---:|---:|---|---|']
def number(v):return '—' if v is None else f'{v:.3f}'
for r in rows:
    lines.append('|'+ '|'.join(map(str,(r['rank'],r['expected'],number(r['tactix_seconds']),number(r['kissat_seconds']),
        number(r['checked_pair_ratio']),r['tactix_status'],r['kissat_status'])))+'|')
(target/'comparison.md').write_text('\n'.join(lines)+'\n')
print(json.dumps(dict(records=summary['records'],final=summary['final_records'],classes=summary['classes'],
    aggregate=summary['ratio'],zero_residue_verified=summary['zero_residue'],path=str(target.relative_to(root))),indent=2))
