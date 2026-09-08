#!/usr/bin/env python3
"""Seal full short-B proofs/models and distinguish mechanism reach from cuts."""
import hashlib
import json
import re
from pathlib import Path
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260907-5462-short-bound-witnesses'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
files=sorted((c/'results').glob('*.json'))
if len(files)!=9:print('5462 pending',len(files),'of 9');raise SystemExit(0)
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['pin_sha256'].items():assert sha(c/rel)==digest
groups={};records={};logs={}
for p in files:
    r=json.loads(p.read_text());task=r['task'];arm=r['arm'];solve=r['solve']
    assert r['valid'] and r['expected_match'] and not solve['timed_out']
    assert solve['exit_code']==(10 if r['expected']=='SAT' else 20)
    assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    assert r['binary_sha256']==m['arms'][arm]['sha256']
    check=r['model_check'] if r['expected']=='SAT' else r['proof_check']
    assert check['exit_code']==0 and not check['timed_out']
    records[p.name]=sha(p)
    stdout=c/'logs'/f'{task:03d}.solver.stdout';stderr=c/'logs'/f'{task:03d}.solver.stderr'
    logs[stdout.name]=sha(stdout);logs[stderr.name]=sha(stderr)
    text=stdout.read_text();err=stderr.read_text()
    row=dict(seconds=solve['wall_seconds'],verdict=r['verdict'],
        statistics=[s for s in r['solver_statistics'] if not s.startswith('c timing')],
        certificate_sha256=r['certificate_sha256'],slot_sha256=sha(stderr))
    if arm=='kissat':
        row['bound_transitions']=[list(map(int,x)) for x in re.findall(r'completed elimination bound (\d+) next (\d+)',text)]
        row['resolution_cut_lines']=[s for s in text.splitlines() if 'hits resolution limit' in s]
    else:
        row['bve_passes']=[dict(zip(('pass','n','bound','meter','limit'),map(int,x)))
            for x in re.findall(r'^\[slot\]\s+BVE pass=(\d+) n=(\d+) bve_bound=(\d+) meter=(\d+) limit=(\d+)',err,re.M)]
        row['selected_work']=[s for s in r['solver_statistics'] if re.match(r'^c (?:factor\s|factor-bnd\s|bve\s|sweep\s|conflicts\s|propagations\s|timing\s)',s)]
    groups.setdefault(r['instance'],{})[arm]=row
for name,g in groups.items():
    assert set(g)=={'predecessor','candidate','kissat'}
    old,new=g['predecessor'],g['candidate']
    assert old['statistics']==new['statistics']
    assert old['certificate_sha256']==new['certificate_sha256']
    assert old['slot_sha256']==new['slot_sha256']
    assert old['bve_passes'] and old['bve_passes']==new['bve_passes']
out=dict(checked_clean=9,solves_per_arm=3,sat_per_arm=2,unsat_per_arm=1,timeouts=0,
    old_new_complete_statistics_proof_bytes_and_slots_identical=True,
    per_instance=groups,result_sha256=records,log_sha256=logs,
    conclusion='All three short B cases reach productive BVE/Factor and normal bound advancement, but do not distinguish the cut-bound repair. They are fast regression controls, not the Rank-37 quota-cut reproducer. Square has substantial inprocessing work and a native time gap, selecting a short causal diagnostic.',
    limitation='Diagnostic logging, one wave, no timing acceptance or full-tier claim. SAT proof-byte identity is not the SAT model oracle; all actual SAT models and UNSAT proofs were independently checked.')
dest=c/'summary.json'
if dest.exists():assert json.loads(dest.read_text())==out
else:dest.write_text(json.dumps(out,indent=2)+'\n')
print('PASS: nine checked/clean solves; three old/new exact-statistics/proof/slot controls')
