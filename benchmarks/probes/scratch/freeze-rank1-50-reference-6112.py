#!/usr/bin/env python3
"""Append the missing matched Kissat vector without refreshing the original40."""
import copy
import datetime as dt
import json
import math
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
c=root/'benchmarks/runs/20260908-6108-rank41-50-matched'
m=json.loads((c/'manifest.json').read_text())
oldpath=root/m['original_reference']['path']
assert f.sha(oldpath)==m['original_reference']['sha256']
old=f.reference(oldpath)
for rel,digest in m['pin_sha256'].items():assert f.sha(c/rel)==digest,rel
for key in ('partition','cpus_per_task','memory_mib','solve_timeout','checker_timeout','one_solver_per_task'):
    assert m['resource_contract'][key]==old['resource_contract'][key],key
assert m['arms']['kissat']['sha256']==old['solver']['sha256']
assert m['arms']['kissat']['arguments']==old['solver']['arguments']
records=copy.deepcopy(old['records'])
checked=0
for task in range(1,20,2):
    p=c/'provisional'/f'{task:03}.json'
    r=json.loads(p.read_text())
    assert r['task']==task and r['rank']==41+task//2 and r['arm']=='kissat'
    assert r['expected_match'] and not r['solve']['timed_out']
    assert r['solve']['exit_code']=={'SAT':10,'UNSAT':20}[r['expected']]
    row=dict(rank=r['rank'],instance=r['instance'],expected=r['expected'],
             input_sha256=m['input_sha256'][r['instance']],measurement=r,
             source_campaign=str(c.relative_to(root)),
             measurement_binding=dict(path=str(p.relative_to(root)),sha256=f.sha(p)))
    final=c/'results'/p.name
    if final.exists():
        a=json.loads(final.read_text())
        assert a['valid'] and a['expected_match'] and a['binary_sha256']==old['solver']['sha256']
        for key in ('solve','certificate_sha256','certificate_bytes','instance','rank','arm'):
            assert a[key]==r[key],key
        checked+=bool(a['artifact_deleted'] and a['derived_artifacts_deleted'] and not a.get('cleanup_pending'))
    records.append(row)
selection=root/m['selection']['path']
sm=json.loads(selection.read_text())
assert f.sha(selection)==m['selection']['sha256']
assert sm['input_sha256']=={r['instance']:r['input_sha256'] for r in records}
dest=root/'benchmarks/baselines/rank1-50-kissat-12cpu-v1'
assert not dest.exists()
ref=dict(schema='tactix-fixed-rank1-50-kissat-reference-v1',
         frozen_at=dt.datetime.now(dt.timezone.utc).isoformat(),
         selection='User-approved50. Original40 timings untouched;first matching twelve-CPU Kissat vector for41-50,not best-of. Pending certificates remain pending.',
         original_reference=m['original_reference'],extension_campaign=str(c.relative_to(root)),
         extension_manifest=dict(path=str((c/'manifest.json').relative_to(root)),sha256=f.sha(c/'manifest.json')),
         source_campaign=old['source_campaign'],source_manifest=old['source_manifest'],
         solver=old['solver'],checkers=old['checkers'],
         resource_contract=dict(old['resource_contract'],array='0-99'),
         solver_seconds=math.fsum(r['measurement']['solve']['wall_seconds'] for r in records),
         instances=50,extension_checked_at_freeze=checked,records=records,
         membership=m['selection'])
dest.mkdir()
(dest/'manifest.json').write_text(json.dumps(ref,indent=2)+'\n')
f.reference(dest/'manifest.json')
print(json.dumps(dict(reference=str(dest.relative_to(root)),seconds=ref['solver_seconds'],
                     original40_seconds=old['solver_seconds'],extension_checked=checked),indent=2))
