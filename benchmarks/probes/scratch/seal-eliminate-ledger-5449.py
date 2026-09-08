#!/usr/bin/env python3
"""Validate the selected causal resolution ledger, not an acceptance timing."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root/'benchmarks/runs/20260907-5449-rank37-eliminate-limit'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''): h.update(block)
    return h.hexdigest()
m=json.loads((c/'manifest.json').read_text())
r=json.loads((c/'results/candidate.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest
assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
for name,digest in r['trace_sha256'].items(): assert sha(c/'traces'/name)==digest
rows=[dict(field.split('=',1) for field in line.rstrip().split('\t')[1:])
      for line in (c/'traces/candidate.bve.tsv').read_text().splitlines() if not line.startswith('#')]
first,last=rows[0],rows[-1]
assert all(x['conflicts']=='29761' and x['limit']=='12587131' for x in rows)
assert last['stage']=='end' and last['ticks']=='12587157' and last['applied']=='0'
slots=[line for line in (c/'logs/candidate.out').read_text().splitlines()
       if line.startswith('[slot] phase=6 outer=1') and ('Strengthen ' in line or 'BVE ' in line)]
assert len(slots)==2 and 'meter=12587157 limit=12587131' in slots[1]
result=dict(clean_stops=1, conflict=29761, first_candidate=first, last_candidate=last,
    resolution_ceiling=12587131, final_meter=12587157, overrun=26,
    next_round_slots=slots, ledger_records=len(rows),
    conclusion='The last rejected BVE attempt crosses the shared phase ceiling. Tactix nevertheless offers another Strengthen round. Native eliminate_variables stops at this boundary after round cleanup. This is a missing general policy budget stop, not evidence that extra simplification is intrinsically weaker.',
    limitations='One stopped diagnostic; no checked verdict or performance claim. 5436 boundary-localization.json owns the common first-round output and subsequent 21-unit difference.')
p=c/'summary.json'
assert not p.exists()
p.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
