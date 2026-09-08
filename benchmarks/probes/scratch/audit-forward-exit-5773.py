#!/usr/bin/env python3
"""Audit the new normal campaigns and report without touching fuzz artifacts."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
target=root / 'benchmarks/reports/2026-09-08-forward-unit-exit-validation.json'
assert not target.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20),b''):h.update(block)
    return h.hexdigest()
hashes={}
def bind(p):hashes[str(p.relative_to(root))]=sha(p)
checks={}
for name in ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary','check-proof-rule-sync','check-inventory'):
    r=subprocess.run(['python3',root / 'tools/docs' / (name+'.py')],cwd=root,capture_output=True,text=True,check=True)
    checks[name]=r.stdout+r.stderr
for label,command in [('fixed-runtime-tests',['python3',root / 'tools/bench/test_fixed_runtime.py']),
                      ('main-ctest',['ctest','--test-dir',root / 'build/release','--output-on-failure','-j8'])]:
    r=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    p=root / 'benchmarks/reports' / ('2026-09-08-forward-unit-exit-'+label+'.log')
    assert not p.exists()
    p.write_text(r.stdout+r.stderr)
    bind(p)
    checks[label]=r.stdout[-500:]+r.stderr[-500:]
campaigns=[]
numbers=(5678,5712,5727,5754,5755,5756,5758,5764,5769,5771)
for number in numbers:
    matches=list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches)==1
    c=matches[0]
    m=json.loads((c / 'manifest.json').read_text())
    for rel,digest in m.get('hashes',m.get('pin_sha256',{})).items():
        assert sha(c / rel)==digest,(number,rel)
        if (c / rel).suffix in ('.py','.patch','.sbatch','.tsv'):bind(c / rel)
    for p in c.glob('*.json'):bind(p)
    results=list((c / 'results').glob('*.json'))
    provisional=list((c / 'provisional').glob('*.json'))
    for p in results:
        r=json.loads(p.read_text())
        if 'solve' in r:
            assert r['expected_match'] and r['valid'] and not r['solve']['timed_out'],p
            assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'],p
        else:
            assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes'],p
            for rel,digest in r['trace_sha256'].items():
                assert sha(c / 'traces' / rel)==digest,(number,rel)
                if not rel.endswith('.decisions.tsv'):bind(c / 'traces' / rel)
        bind(p)
    for p in provisional:
        r=json.loads(p.read_text())
        assert r['expected_match'] and r['solve']['exit_code'] in (10,20) and not r['solve']['timed_out'],p
        bind(p)
    for p in (c / 'logs').glob('*.out'):bind(p)
    campaigns.append(dict(campaign=str(c.relative_to(root)),results=len(results),solver_records=len(provisional)))
c=root / 'benchmarks/runs/20260908-5760-forward-exit-observer'
m=json.loads((c / 'manifest.json').read_text())
for rel,digest in m['files'].items():assert sha(root / m['source'] / rel)==digest
for rel,digest in m['binaries'].items():assert sha(root / rel)==digest
for name,digest in m['patches'].items():assert sha(c / name)==digest
for name,digest in m['logs'].items():assert sha(c / 'logs' / name)==digest
v=json.loads((c / 'fixed-control.json').read_text())
for rel,digest in v['hashes'].items():assert sha(root / rel)==digest
for p in [c / 'manifest.json',c / 'fixed-control.json',c / 'observer.patch',*(c / 'logs').iterdir()]:bind(p)
for rel in ('PROGRESS.md','docs/scheduling-parity-ledger.md','tools/bench/fixed_runtime.py','tools/bench/test_fixed_runtime.py',
            'benchmarks/reports/2026-09-08-forward-unit-exit.md',
            'report-to-andrew/20260908-1055-forward-unit-exit/report.tex',
            'report-to-andrew/20260908-1055-forward-unit-exit/report.pdf'):
    bind(root / rel)
log=(root / 'report-to-andrew/20260908-1055-forward-unit-exit/report.log').read_text()
assert 'Output written on report.pdf' in log and 'Overfull' not in log
queue=subprocess.run(['squeue','-h','-j','1673074,1673169,1673255,1673329,1673386','-o','%i|%T|%R|%C|%m'],
    cwd=root,capture_output=True,text=True,check=True).stdout
subprocess.run(['git','diff','--check'],cwd=root,check=True)
out=dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,checks=checks,campaigns=campaigns,
    normal_queue=queue,andrew_final_compilations=2,
    baseline='Fixed6810.066s vector unchanged;full5758 has40 expected solves7807.759s=1.1465026918x. One new native5771 diagnostic supplies missing coverage beyond100K,not baseline refresh.',
    findings='Rank31 unit stops spend under1% of first-round allowance;actual native/Tactix unit-tail controls and observer neutrality verified. Rank32 now largest loss;all40102204 native100K rows still agree,so5771 extends to300K.',
    fuzz_owner='5765/session8493 live on5741;CLI response only. No fuzz jobs,artifacts,logs or reports read. Parent must also stop5771 on any owner-reported5741 correctness failure.',
    cleanup='No manual deletion. Successful diagnostic jobs removed their own node-local inputs and partial proofs;hashes retained. Unique traces,pins and fixed controls retained. Earlier failed-fixture scratch requires exact ownership audit.',
    limitations='Production unchanged,no repair implemented for forward-unit continuation,no promotion. Pending checks/fuzz are not verified. Push unapproved and not retried.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(campaigns,indent=2))
print('PASS pins,normal results,observer/control evidence,tests,docs and Andrew report')
