#!/usr/bin/env python3
"""Scoped non-fuzz artifact audit, preserving every active/unique campaign."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''): h.update(chunk)
 return h.hexdigest()
campaigns=[]
for n in range(5360,5373):
 c=next((root/'benchmarks/runs').glob('20260907-'+str(n)+'-*'))
 m=json.loads((c/'manifest.json').read_text())
 for rel,expected in m.get('hashes',m.get('pin_sha256',{})).items(): assert sha(c/rel)==expected,(c,rel)
 residue=[str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf','.drat','.lrat','.grat','.frat')]
 assert not residue,residue
 campaigns.append(dict(campaign=str(c.relative_to(root)),
  manifest_sha256=sha(c/'manifest.json'),parent=m.get('parent_campaign',m.get('predecessor')),
  submitted=m.get('job',json.loads((c/'submission.json').read_text()).get('job') if (c/'submission.json').exists() else None),
  repository_residue=residue,results=len(list((c/'results').glob('*.json'))),
  retained_reason='Unique causal evidence or active checked gate; pins/runners and parent references retained.',
  trace_files={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in (c/'traces').glob('*') if p.is_file()}))
test_records=[]
for label,build,count,seconds in [('trace','build/pin-5369-congruence-prefix',76,5.99),
                                  ('release','build/pin-5371-congruence-prefix-release',77,5.70)]:
 source=root/build/'Testing/Temporary/LastTest.log'
 text=source.read_text();assert text.count('Test Passed.')==count and 'Test Failed.' not in text
 dest=next((root/'benchmarks/runs').glob('20260907-5369-*'))/(label+'-ctest.log')
 if dest.exists(): assert dest.read_bytes()==source.read_bytes()
 else: shutil.copy2(source,dest)
 test_records.append(dict(build=build,passed=count,total=count,seconds=seconds,log=str(dest.relative_to(root)),sha256=sha(dest)))
report=root/'benchmarks/reports/2026-09-07-congruence-prefix-artifact-index.json'
result=dict(scope='5360–5372 normal campaigns only; no fuzz artifact inspection. Active 5371/5372 scratch remains node-local and job-owned.',
 removed_paths=[],removed_bytes=0,retention='All inspected campaigns have unique evidence or active ownership; no speculative deletion.',campaigns=campaigns,tests=test_records)
assert not report.exists()
report.write_text(json.dumps(result,indent=2)+'\n')
print('PASS',len(campaigns),'campaigns; no repository certificate residue; no deletion')
