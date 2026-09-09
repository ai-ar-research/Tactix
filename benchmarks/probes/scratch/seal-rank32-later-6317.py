#!/usr/bin/env python3
"""Append a late validated result without rewriting the repair checkpoint."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6303-rank32-later-prefix'
s=json.loads((c/'summary.json').read_text())
assert s['matching_decisions_from_start']==105567985 and s['decisions']['left'][0]==1368117
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
a=json.loads((root/'benchmarks/reports/2026-09-09-substitute-collapse-audit-6315.json').read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in a[group].items():
        if rel!='PROGRESS.md':assert sha(root/rel)==digest,rel
a['owned_hashes']['PROGRESS.md']=sha(root/'PROGRESS.md')
for p in (c/'summary.json',c/'results/candidate.json',c/'results/kissat.json',Path(__file__),Path(__file__).with_name('land-rank32-later-6318.py')):
    a['owned_hashes'][str(p.relative_to(root))]=sha(p)
for rel,digest in s['hashes'].items():
    if rel.startswith(str(c.relative_to(root))+'/traces/'):
        a['local_hash_bound'][rel]=dict(bytes=(root/rel).stat().st_size,sha256=digest)
report=root/'report-to-andrew/20260909-0428-rank32-addendum'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
for name in ('report.tex','report.pdf'):a['andrew_report'][str((report/name).relative_to(root))]=sha(report/name)
for name in ('check-doc-size','check-progress'):
    p=subprocess.run(['python3','tools/docs/'+name+'.py'],cwd=root,text=True,capture_output=True,check=True)
    a['checks']['late-'+name]=dict(stdout=p.stdout,stderr=p.stderr)
a['late_result']=dict(summary_sha256=sha(c/'summary.json'),matching_rows=105567985,first_conflict=1368117,preceding_bve_conflict=1366932,
    limitation='6269, not new6305; actual6305 trajectory remains untested. Prior audit job states are timestamped snapshots, not current6303 liveness.')
a['next']='After6311 expected BC outcomes, replay6305 against now-cached native6303 and verify inherited closure; inspect1366932 BVE if mismatch remains. No tracker/FlatLists approval inferred; no promotion.'
target=root/'benchmarks/reports/2026-09-09-rank32-later-audit-6317.json'
assert not target.exists();target.write_text(json.dumps(a,indent=2)+'\n');print(target)
